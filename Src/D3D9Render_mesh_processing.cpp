#include "D3D9Render.h"
#include "vectorUtils.h"

#if UNREAL_GOLD_OLDUNREAL
#include "UnTerrainInfo.h"
#endif

#pragma warning(disable : 4018)

// Class to hold multiple unique T objects, accessable by multiple indices
template <typename T>
class UniqueValueArray {
public:
	UniqueValueArray(int reserve) {
		valueMap.reserve(reserve);
		uniqueValues.reserve(reserve);
	}

	bool insert(int index, const T& value) {
		int uniqueIdx = -1;
		for (int i = 0; i < uniqueValues.size(); i++) {
			if (uniqueValues[i] == value) {
				uniqueIdx = i;
				break;
			}
		}
		bool newVal = false;
		if (uniqueIdx == -1) {
			uniqueIdx = uniqueValues.size();
			uniqueValues.push_back(value);
			newVal = true;
		}
		if (index >= valueMap.size()) {
			valueMap.resize(index + 1, -1);
		}
		valueMap[index] = uniqueIdx;
		return newVal;
	}

	T* at(int index) {
		if (index < valueMap.size()) {
			int uniqueIdx = valueMap[index];
			if (uniqueIdx != -1) return &uniqueValues[uniqueIdx];
		}
		return nullptr;
	}

	bool has(int index) {
		if (index < valueMap.size()) {
			int uniqueIdx = valueMap[index];
			if (uniqueIdx != -1) return true;
		}
		return false;
	}

	int getIndex(T value) {
		for (int i = 0; i < valueMap.size(); i++) {
			int uniqueIdx = valueMap[i];
			if (uniqueIdx != -1 && uniqueValues[uniqueIdx] == value) {
				return i;
			}
		}
		return -1;
	}

	auto begin() {
		return uniqueValues.begin();
	}

	auto end() {
		return uniqueValues.end();
	}

private:
	std::vector<int> valueMap;
	std::vector<T> uniqueValues;
};

static void calcEnvMapping(FRenderVert& vert, const DirectX::XMMATRIX& screenSpaceMat, FSceneNode* frame) {
	using namespace DirectX;
	XMVECTOR ssPoint = XMVector3Transform(FVecToDXVec(vert.Point), screenSpaceMat);
	XMVECTOR ssNormal = XMVector3TransformNormal(FVecToDXVec(vert.Normal), screenSpaceMat);
	ssNormal = XMVector3Normalize(ssNormal);

	XMVECTOR normPoint = XMVector3Normalize(ssPoint);
	XMVECTOR reflected = XMVector3Reflect(normPoint, ssNormal);
	XMVECTOR envNorm = XMVector3TransformNormal(reflected, FCoordToDXMat(frame->Coords));

	vert.U = (XMVectorGetX(envNorm) + 1.0) * 0.5 * 256.0;
	vert.V = (XMVectorGetY(envNorm) + 1.0) * 0.5 * 256.0;
}

void UD3D9RenderDevice::renderMeshActor(FSceneNode* frame, AActor* actor, SpecialCoord* specialCoord) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: renderMeshActor = " << si++ << std::endl;
	}
#endif
	using namespace DirectX;
	guard(UD3D9RenderDevice::renderMeshActor);
	EndBuffering();
	UMesh* mesh = actor->Mesh;
	if (!mesh)
		return;

#if UNREAL_GOLD_OLDUNREAL
	if (mesh->IsA(UStaticMesh::StaticClass())) {
		renderStaticMeshActor(frame, actor, specialCoord);
		return;
	}
	else if (mesh->IsA(UTerrainMesh::StaticClass())) {
		renderTerrainMeshActor(frame, actor, specialCoord);
		return;
	}
#endif

	//dout << "rendering actor " << actor->GetName() << std::endl;
	XMMATRIX actorMatrix = XMMatrixIdentity();

	bool renderAsParticles = false;
	FVector adjustLoc(0, 0, 0);
#if UTGLR_HP_ENGINE
	if (mesh->IsA(USkeletalMesh::StaticClass())) {
		USkeletalMesh* skMesh = static_cast<USkeletalMesh*>(mesh);
		// Fuck your private method
		FARPROC fnPtr = GetProcAddress(GetModuleHandle(L"Engine.dll"), "?MeshAdjust@USkeletalMesh@@ABE?AVFVector@@PBVAActor@@@Z");
		typedef FVector(USkeletalMesh::* fnTyp)(const AActor*)const;
		fnTyp* fn = reinterpret_cast<fnTyp*>(&fnPtr);
		adjustLoc = (skMesh->* * fn)(actor);
#if HARRY_POTTER_2
		adjustLoc.Z += actor->SavedPrePivotZ;
#endif
	}
#else
	renderAsParticles = actor->bParticles;
#endif

	if (!renderAsParticles) {
		XMMATRIX matScale = XMMatrixScaling(actor->DrawScale, actor->DrawScale, actor->DrawScale);
		actorMatrix *= matScale;
	}
	if (specialCoord && specialCoord->enabled) {
		actorMatrix *= FCoordToDXMat(specialCoord->coord);
		actorMatrix *= FCoordToDXMat(specialCoord->baseCoord);
	}
	else {
		XMMATRIX matLoc = XMMatrixTranslationFromVector(FVecToDXVec(actor->Location + actor->PrePivot + adjustLoc));
		XMMATRIX matRot = FRotToDXRotMat(actor->Rotation);
		actorMatrix *= matRot;
		actorMatrix *= matLoc;
	}

	// The old switcheroo, trick the game to not transform the mesh verts to object position
	FVector origLoc = actor->Location;
	FVector origPrePiv = actor->PrePivot;
	FRotator origRot = actor->Rotation;
	FLOAT origScale = actor->DrawScale;
	actor->Location = FVector(0, 0, 0);
	actor->PrePivot = FVector(0, 0, 0);
	actor->Rotation = FRotator(0, 0, 0);
	actor->DrawScale = 1.0f;
#if UTGLR_HP_ENGINE
	bool origAlignBot = actor->bAlignBottom;
	actor->bAlignBottom = false;
#if HARRY_POTTER_2
	FLOAT origSavedPrePivotZ = actor->SavedPrePivotZ;
	bool origAlignBotAlways = actor->bAlignBottomAlways;
	actor->SavedPrePivotZ = 0.0f;
	actor->bAlignBottomAlways = false;
#endif
#endif

	int numVerts;
	int numTris;
	FVector* samples;
	bool isLod;

	if (mesh->IsA(ULodMesh::StaticClass())) {
		isLod = true;
		ULodMesh* meshLod = (ULodMesh*)mesh;
		numVerts = meshLod->ModelVerts;
		FVector* allSamples = New<FVector>(GMem, numVerts + meshLod->SpecialVerts);
		// First samples are special coordinates
		samples = &allSamples[meshLod->SpecialVerts];
#if UNREAL_GOLD_OLDUNREAL
		meshLod->GetFrame(allSamples, sizeof(samples[0]), GMath.UnitCoords, actor);
#else
		meshLod->GetFrame(allSamples, sizeof(samples[0]), GMath.UnitCoords, actor, numVerts);
#endif
		numTris = meshLod->Faces.Num();
#if UTGLR_HP_ENGINE
		if (specialCoord && !specialCoord->enabled && mesh->IsA(USkeletalMesh::StaticClass())) {
			USkeletalMesh* skelMesh = static_cast<USkeletalMesh*>(mesh);
			if (skelMesh->WeaponBoneIndex > -1) {
				specialCoord->coord = skelMesh->ClassicWeaponCoords.Inverse();
				specialCoord->baseCoord = DXMatToFCoord(actorMatrix);
				specialCoord->worldCoord = DXMatToFCoord(FCoordToDXMat(specialCoord->coord) * actorMatrix);
				specialCoord->exists = true;
			}
		}
		else
#endif
		if (specialCoord && !specialCoord->enabled && meshLod->SpecialFaces.Num() > 0) {
			// Setup special coordinate (attachment point)
			FVector& v0 = allSamples[0];
			FVector& v1 = allSamples[1];
			FVector& v2 = allSamples[2];
			FCoords coord;
			coord.Origin = (v0 + v2) * 0.5f;
			coord.XAxis = (v1 - v0).SafeNormal();
			coord.YAxis = ((v0 - v2) ^ coord.XAxis).SafeNormal();
			coord.ZAxis = coord.XAxis ^ coord.YAxis;
			specialCoord->coord = coord;
			specialCoord->baseCoord = DXMatToFCoord(actorMatrix);
			specialCoord->worldCoord = DXMatToFCoord(FCoordToDXMat(specialCoord->coord) * actorMatrix);
			specialCoord->exists = true;
		}
	}
	else {
		isLod = false;
		numVerts = mesh->FrameVerts;
		samples = New<FVector>(GMem, numVerts);
		mesh->GetFrame(samples, sizeof(samples[0]), GMath.UnitCoords, actor);
		numTris = mesh->Tris.Num();
	}

	actor->Location = origLoc;
	actor->PrePivot = origPrePiv;
	actor->Rotation = origRot;
	actor->DrawScale = origScale;
#if UTGLR_HP_ENGINE
	actor->bAlignBottom = origAlignBot;
#if HARRY_POTTER_2
	actor->SavedPrePivotZ = origSavedPrePivotZ;
	actor->bAlignBottomAlways = origAlignBotAlways;
#endif
#endif

	FTime currentTime = frame->Viewport->CurrentTime;
	DWORD baseFlags = getBasePolyFlags(actor);

	if (renderAsParticles) {
		FLOAT lux = Clamp(actor->ScaleGlow * 0.5f + actor->AmbientGlow / 256.f, 0.f, 1.f);
		FPlane color = FVector(lux, lux, lux);
		if (GIsEditor && (baseFlags & PF_Selected)) {
			color = color * 0.4 + FVector(0.0, 0.6, 0.0);
		}
#if UNREAL_GOLD_OLDUNREAL
		UTexture* tex = actor->Texture->Get();
#else
		UTexture* tex = actor->Texture->Get(currentTime);
#endif
		for (INT i = 0; i < numVerts; i++) {
			FVector& sample = samples[i];
			if (actor->bRandomFrame) {
				tex = actor->MultiSkins[appCeil((&sample - samples) / 3.f) % 8];
				if (tex) {
					tex = getTextureWithoutNext(tex, currentTime, actor->LifeFraction());
				}
			}
			if (tex) {

				// Transform the local-space point into world-space
				XMVECTOR xpoint = FVecToDXVec(sample);
				xpoint = XMVector3Transform(xpoint, actorMatrix);
				FVector point = DXVecToFVec(xpoint);

				FTextureInfo texInfo;
#if UNREAL_GOLD_OLDUNREAL
				texInfo = *tex->GetTexture(-1, this);
#else
				tex->Lock(texInfo, currentTime, -1, this);
#endif
				renderSpriteGeo(frame, point, actor->DrawScale, texInfo, baseFlags, color);
#if !UNREAL_GOLD_OLDUNREAL
				tex->Unlock(texInfo);
#endif
			}
		}
		return;
	}

	UniqueValueArray<UTexture*> textures(mesh->Textures.Num());
	UTexture* envTex = nullptr;

	// Lock all mesh textures
	for (int i = 0; i < mesh->Textures.Num(); i++) {
		UTexture* tex = mesh->GetTexture(i, actor);
		if (!tex && actor->Texture) {
			tex = actor->Texture;
		}
		else if (!tex) {
			continue;
		}
#if UNREAL_GOLD_OLDUNREAL
		tex = tex->Get();
#else
		tex = tex->Get(currentTime);
#endif
		textures.insert(i, tex);
		envTex = tex;
	}
	if (actor->Texture) {
		envTex = actor->Texture;
	}
	else if (actor->Region.Zone && actor->Region.Zone->EnvironmentMap) {
		envTex = actor->Region.Zone->EnvironmentMap;
	}
	else if (actor->Level->EnvironmentMap) {
		envTex = actor->Level->EnvironmentMap;
	}
	if (!envTex) {
		return;
	}

	bool fatten = actor->Fatness != 128;
	FLOAT fatness = (actor->Fatness / 16.0) - 8.0;

	FVector* normals = NewZeroed<FVector>(GMem, numVerts);

	// Calculate normals
	// Already zeroed on new
	for (int i = 0; i < numTris; i++) {
		int sampleIdx[3];
		if (isLod) {
			ULodMesh* meshLod = (ULodMesh*)mesh;
			FMeshFace& face = meshLod->Faces(i);
			for (int j = 0; j < 3; j++) {
				FMeshWedge& wedge = meshLod->Wedges(face.iWedge[j]);
				sampleIdx[j] = wedge.iVertex;
			}
		}
		else {
			FMeshTri& tri = mesh->Tris(i);
			for (int j = 0; j < 3; j++) {
				sampleIdx[j] = tri.iVertex[j];
			}
		}
		FVector fNorm = (samples[sampleIdx[1]] - samples[sampleIdx[0]]) ^ (samples[sampleIdx[2]] - samples[sampleIdx[0]]);
		for (int j = 0; j < 3; j++) {
			normals[sampleIdx[j]] += fNorm;
		}
	}
	for (int i = 0; i < numVerts; i++) {
#if HARRY_POTTER_2
		// Try and compensate for harry's cape having inner and outer faces sharing a vert
		if (normals[i].Size() < 0.01) {
			normals[i] = samples[i];
		}
#endif
		XMVECTOR normal = FVecToDXVec(normals[i]);
		normal = XMVector3Normalize(normal);
		normals[i] = DXVecToFVec(normal);
	}

	XMMATRIX screenSpaceMat = actorMatrix * FCoordToDXMat(frame->Uncoords);

	SurfKeyBucketVector<UTexture*, FRenderVert> surfaceBuckets;
	surfaceBuckets.reserve(numTris);

	int vertMaxCount = numTris * 3;
	// Process all triangles on the mesh
	for (INT i = 0; i < numTris; i++) {
		INT sampleIdx[3];
		DWORD polyFlags;
		INT texIdx;
		FMeshUV triUV[3];
		if (isLod) {
			ULodMesh* meshLod = (ULodMesh*)mesh;
			FMeshFace& face = meshLod->Faces(i);
			for (int j = 0; j < 3; j++) {
				FMeshWedge& wedge = meshLod->Wedges(face.iWedge[j]);
				sampleIdx[j] = wedge.iVertex;
				triUV[j] = wedge.TexUV;
			}
			FMeshMaterial& mat = meshLod->Materials(face.MaterialIndex);
			texIdx = mat.TextureIndex;
			polyFlags = mat.PolyFlags;
		}
		else {
			FMeshTri& tri = mesh->Tris(i);
			for (int j = 0; j < 3; j++) {
				sampleIdx[j] = tri.iVertex[j];
				triUV[j] = tri.Tex[j];
			}
			texIdx = tri.TextureIndex;
			polyFlags = tri.PolyFlags;
		}

		polyFlags |= baseFlags;

		bool environMapped = polyFlags & PF_Environment;
		UTexture** tex = textures.at(texIdx);
		if (environMapped || !tex) {
			tex = &envTex;
		}
#if UNREAL_GOLD_OLDUNREAL
		float scaleU = (*tex)->DrawScale * (*tex)->USize / 256.0;
		float scaleV = (*tex)->DrawScale * (*tex)->VSize / 256.0;
#else
		float scaleU = (*tex)->Scale * (*tex)->USize / 256.0;
		float scaleV = (*tex)->Scale * (*tex)->VSize / 256.0;
#endif

		// Sort triangles into surface/flag groups
		std::vector<FRenderVert>& pointsVec = surfaceBuckets.get(*tex, polyFlags);
		pointsVec.reserve(vertMaxCount);
		for (INT j = 0; j < 3; j++) {
			FRenderVert& vert = pointsVec.emplace_back();
			vert.Point = samples[sampleIdx[j]];
			vert.Normal = normals[sampleIdx[j]];
			vert.U = triUV[j].U;
			vert.V = triUV[j].V;
			if (fatten) {
				vert.Point += vert.Normal * fatness;
			}

			// Calculate the environment UV mapping
			if (environMapped) {
				calcEnvMapping(vert, screenSpaceMat, frame);
			}
			vert.U *= scaleU;
			vert.V *= scaleV;
		}
	}
	ActorRenderData renderData{ surfaceBuckets, ToD3DMATRIX(actorMatrix) };
	renderSurfaceBuckets(renderData, currentTime);

	unguard;
}


#if UNREAL_GOLD_OLDUNREAL
void UD3D9RenderDevice::renderStaticMeshActor(FSceneNode* frame, AActor* actor, RenderList& renderList, SpecialCoord* specialCoord) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: renderStaticMeshActor = " << si++ << std::endl;
	}
#endif
	using namespace DirectX;
	guard(UD3D9RenderDevice::renderStaticMeshActor);
	EndBuffering();
	UStaticMesh* mesh = Cast<UStaticMesh>(actor->Mesh);
	if (!mesh)
		return;

	mesh->SMTris.Load();
	if (!mesh->SMTris.Num()) {
		return;
	}

	//dout << "rendering actor " << actor->GetName() << std::endl;
	XMMATRIX actorMatrix = XMMatrixIdentity();

	FVector adjustLoc(0, 0, 0);
	bool renderAsParticles = actor->bParticles;

	if (!renderAsParticles) {
		XMMATRIX matScale = XMMatrixScaling(actor->DrawScale, actor->DrawScale, actor->DrawScale);
		actorMatrix *= matScale;
	}
	if (specialCoord && specialCoord->enabled) {
		actorMatrix *= FCoordToDXMat(specialCoord->coord);
		actorMatrix *= FCoordToDXMat(specialCoord->baseCoord);
	}
	else {
		XMMATRIX matLoc = XMMatrixTranslationFromVector(FVecToDXVec(actor->Location + actor->PrePivot + adjustLoc));
		XMMATRIX matRot = FRotToDXRotMat(actor->Rotation);
		actorMatrix *= matRot;
		actorMatrix *= matLoc;
	}

	FTime currentTime = frame->Viewport->CurrentTime;
	DWORD baseFlags = getBasePolyFlags(actor);

	if (renderAsParticles) {
		FLOAT lux = Clamp(actor->ScaleGlow * 0.5f + actor->AmbientGlow / 256.f, 0.f, 1.f);
		FPlane color = FVector(lux, lux, lux);
		if (GIsEditor && (baseFlags & PF_Selected)) {
			color = color * 0.4 + FVector(0.0, 0.6, 0.0);
		}
		UTexture* tex = actor->Texture->Get();
		for (INT i = 0; i < mesh->SMVerts.Num(); i++) {
			FVector& sample = mesh->SMVerts(i);
			if (actor->bRandomFrame) {
				tex = actor->MultiSkins[appCeil((&sample - &mesh->SMVerts(0)) / 3.f) % 8];
				if (tex) {
					tex = getTextureWithoutNext(tex, currentTime, actor->LifeFraction());
				}
			}
			if (tex) {

				// Transform the local-space point into world-space
				XMVECTOR xpoint = FVecToDXVec(sample);
				xpoint = XMVector3Transform(xpoint, actorMatrix);
				FVector point = DXVecToFVec(xpoint);

				FTextureInfo texInfo;
				texInfo = *tex->GetTexture(-1, this);
				renderSpriteGeo(frame, point, actor->DrawScale, texInfo, baseFlags, color);
			}
		}
		return;
	}

	UniqueValueArray<UTexture*> textures(mesh->Textures.Num());
	UTexture* envTex = nullptr;

	// Lock all mesh textures
	for (int i = 0; i < mesh->Textures.Num(); i++) {
		UTexture* tex = mesh->GetTexture(i, actor);
		if (!tex && actor->Texture) {
			tex = actor->Texture;
		}
		else if (!tex) {
			continue;
		}
		tex = tex->Get();
		textures.insert(i, tex);
		envTex = tex;
	}
	if (actor->Texture) {
		envTex = actor->Texture;
	}
	else if (actor->Region.Zone && actor->Region.Zone->EnvironmentMap) {
		envTex = actor->Region.Zone->EnvironmentMap;
	}
	else if (actor->Level->EnvironmentMap) {
		envTex = actor->Level->EnvironmentMap;
	}
	if (!envTex) {
		return;
	}

	bool fatten = actor->Fatness != 128;
	FLOAT fatness = (actor->Fatness / 16.0) - 8.0;

	// Calculate normals
	if (!mesh->SMNormals.Num()) {
		mesh->CalcSMNormals();
	}

	XMMATRIX screenSpaceMat = actorMatrix * FCoordToDXMat(frame->Uncoords);

	SurfKeyBucketVector<UTexture*, FRenderVert> surfaceBuckets;
	surfaceBuckets.reserve(mesh->SMGroups.Num());

	INT vertMaxCount = mesh->SMGroups.Num();
	// Process all triangles on the mesh
	for (INT i = 0; i < mesh->SMTris.Num(); i++) {
		FStaticMeshTri& tri = mesh->SMTris(i);
		FStaticMeshTexGroup& group = mesh->SMGroups(tri.GroupIndex);
		INT texIdx = group.Texture;
		DWORD polyFlags = group.RealPolyFlags;

		polyFlags |= baseFlags;

		bool environMapped = polyFlags & PF_Environment;
		UTexture** tex = textures.at(texIdx);
		if (environMapped || !tex) {
			tex = &envTex;
		}
		float scaleU = (*tex)->DrawScale * (*tex)->USize;
		float scaleV = (*tex)->DrawScale * (*tex)->VSize;

		// Sort triangles into surface/flag groups
		std::vector<FRenderVert>& pointsVec = surfaceBuckets.get(*tex, polyFlags);
		pointsVec.reserve(vertMaxCount);
		for (INT j = 0; j < 3; j++) {
			FRenderVert& vert = pointsVec.emplace_back();
			vert.Point = mesh->SMVerts(tri.iVertex[j]);
			vert.Normal = mesh->SMNormals(i);
			vert.U = tri.Tex[j].U;
			vert.V = tri.Tex[j].V;
			if (fatten) {
				vert.Point += vert.Normal * fatness;
			}

			// Calculate the environment UV mapping
			if (environMapped) {
				calcEnvMapping(vert, screenSpaceMat, frame);
			}
			vert.U *= scaleU;
			vert.V *= scaleV;
		}
	}

	ActorRenderData renderData{ surfaceBuckets, ToD3DMATRIX(actorMatrix) };
	renderSurfaceBuckets(renderData, currentTime);

	unguard;
}

void UD3D9RenderDevice::renderTerrainMeshActor(FSceneNode* frame, AActor* actor, SpecialCoord* specialCoord) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: renderTerrainMeshActor = " << si++ << std::endl;
	}
#endif
	using namespace DirectX;
	typedef UTerrainMesh::FTerrainQuad FTerrainQuad;
	typedef UTerrainMesh::FTerrainTris FTerrainTris;
	typedef UTerrainMesh::FTerrainVert FTerrainVert;
	guard(UD3D9RenderDevice::renderTerrainMeshActor);
	EndBuffering();
	UTerrainMesh* mesh = Cast<UTerrainMesh>(actor->Mesh);
	if (!mesh)
		return;

	//dout << "rendering actor " << actor->GetName() << std::endl;
	XMMATRIX actorMatrix = XMMatrixIdentity();

	//XMMATRIX matLoc = XMMatrixTranslationFromVector(FVecToDXVec(actor->Location + actor->PrePivot));
	XMMATRIX matRot = FRotToDXRotMat(actor->Rotation);
	actorMatrix *= matRot;
	//actorMatrix *= matLoc;

	FTime currentTime = frame->Viewport->CurrentTime;
	DWORD baseFlags = getBasePolyFlags(actor);

	UniqueValueArray<UTexture*> textures(mesh->Textures.Num());
	UniqueValueArray<FTextureInfo> texInfos(mesh->Textures.Num());
	UTexture* envTex = nullptr;
	FTextureInfo envTexInfo;

	// Lock all mesh textures
	for (int i = 0; i < mesh->Textures.Num(); i++) {
		UTexture* tex = mesh->GetTexture(i, actor);
		if (!tex && actor->Texture) {
			tex = actor->Texture;
		}
		else if (!tex) {
			continue;
		}
		tex = tex->Get();
		textures.insert(i, tex);
		envTex = tex;
	}
	if (actor->Texture) {
		envTex = actor->Texture;
	}
	else if (actor->Region.Zone && actor->Region.Zone->EnvironmentMap) {
		envTex = actor->Region.Zone->EnvironmentMap;
	}
	else if (actor->Level->EnvironmentMap) {
		envTex = actor->Level->EnvironmentMap;
	}
	if (!envTex) {
		return;
	}
	envTexInfo = *envTex->GetTexture(-1, this);

	bool fatten = actor->Fatness != 128;
	FLOAT fatness = (actor->Fatness / 16.0) - 8.0;

	XMMATRIX screenSpaceMat = actorMatrix * FCoordToDXMat(frame->Uncoords);

	FTerrainQuad* quad = &mesh->TerrainQuads(actor->LatentInt);
	FTerrainTris* tris = &mesh->TerrainTris(quad->TrisOffset);

	SurfKeyBucketVector<UTexture*, FRenderVert> surfaceBuckets;
	surfaceBuckets.reserve(quad->Verts.Num());

	INT vertMaxCount = quad->NumTris * 3;
	// Process all triangles on the mesh
	for (INT i = 0; i < quad->NumTris; i++) {
		FTerrainTris& tri = tris[i];
		INT texIdx = tri.GetTexIndex();
		DWORD polyFlags = 0;

		polyFlags |= tri.IsAlpha() ? (baseFlags | PF_AlphaBlend) : baseFlags;

		bool environMapped = polyFlags & PF_Environment;
		UTexture** tex = textures.at(texIdx);
		if (environMapped || !tex) {
			tex = &envTex;
		}
		float scaleU = (*tex)->DrawScale * (*tex)->USize;
		float scaleV = (*tex)->DrawScale * (*tex)->VSize;

		// Sort triangles into surface/flag groups
		std::vector<FRenderVert>& pointsVec = surfaceBuckets.get(*tex, polyFlags);
		pointsVec.reserve(vertMaxCount);
		for (INT j = 0; j < 3; j++) {
			FTerrainVert& terrainVert = mesh->TerrainVerts(quad->Verts(tri.RenderVerts[j]));
			FRenderVert& vert = pointsVec.emplace_back();
			vert.Point = terrainVert.Vert;
			vert.Normal = terrainVert.Normal;
			vert.U = tri.UV[j].U / 256.0f;
			vert.V = tri.UV[j].V / 256.0f;
			DWORD alpha = tri.EdgeAlpha[j] * 255;
			vert.Color = (alpha << 24) | 0x00FFFFFF;
			if (fatten) {
				vert.Point += vert.Normal * fatness;
			}

			// Calculate the environment UV mapping
			if (environMapped) {
				calcEnvMapping(vert, screenSpaceMat, frame);
			}
			vert.U *= scaleU;
			vert.V *= scaleV;
		}
	}

	ActorRenderData renderData{ surfaceBuckets, ToD3DMATRIX(actorMatrix) };
	renderSurfaceBuckets(renderData, currentTime);

	unguard;
}
#endif

#if RUNE
void UD3D9RenderDevice::renderSkeletalMeshActor(FSceneNode* frame, AActor* actor, const FCoords* parentCoord) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: renderSkeletalMeshActor = " << si++ << std::endl;
	}
#endif
	using namespace DirectX;
	guard(UD3D9RenderDevice::renderSkeletalMeshActor);
	EndBuffering();
	USkelModel* skelModel = actor->Skeletal;

	if (!skelModel)
		return;

	STAT(clockFast(GStat.SkelGetFrameTime));

	USkelModel* usedSkel = actor->SubstituteMesh ? actor->SubstituteMesh : skelModel;
	INT meshIdx = actor->SkelMesh;
	meshIdx = meshIdx < usedSkel->nummeshes ? meshIdx : usedSkel->nummeshes - 1;

	Mesh* mesh = &usedSkel->meshes(meshIdx);

	XMMATRIX actorMatrix = XMMatrixIdentity();

	if (!actor->bParticles) {
		XMMATRIX matScale = XMMatrixScaling(actor->DrawScale, actor->DrawScale, actor->DrawScale);
		actorMatrix *= matScale;
	}

	if (parentCoord) {
		FCoords adjustedCoord = *parentCoord;
		actorMatrix *= FCoordToDXMat(adjustedCoord.Inverse());
	}
	else {
		XMMATRIX matLoc = XMMatrixTranslationFromVector(FVecToDXVec(actor->Location + actor->PrePivot));
		XMMATRIX matRot = FRotToDXRotMat(actor->Rotation);
		actorMatrix *= matRot;
		actorMatrix *= matLoc;
	}

	// The old switcheroo, trick the game to not transform the mesh verts to object position
	FVector origLoc = actor->Location;
	FVector origPrePiv = actor->PrePivot;
	FRotator origRot = actor->Rotation;
	FLOAT origScale = actor->DrawScale;
	actor->Location = FVector(0, 0, 0);
	actor->PrePivot = FVector(0, 0, 0);
	actor->Rotation = FRotator(0, 0, 0);
	actor->DrawScale = 1.0f;

	int numVerts = mesh->numverts;
	int numTris = mesh->numtris;

	FVector* deformed = New<FVector>(GMem, numVerts);

	skelModel->GetFrame(actor, GMath.UnitCoords, 0.0f, deformed);

	actor->Location = origLoc;
	actor->PrePivot = origPrePiv;
	actor->Rotation = origRot;
	actor->DrawScale = origScale;

	FTime currentTime = frame->Viewport->CurrentTime;
	DWORD baseFlags = getBasePolyFlags(actor);

	STAT(unclockFast(GStat.SkelGetFrameTime));

	if (actor->bParticles) {
		if (!actor->Texture) {
			return;
		}
		UTexture* tex = actor->Texture->Get(currentTime);
		FLOAT lux = Clamp(actor->ScaleGlow * 0.5f + actor->AmbientGlow / 256.f, 0.f, 1.f);
		FPlane color = FVector(lux, lux, lux);
		if (!actor->ColorAdjust.IsZero()) {
			color = color * 0.4 + actor->ColorAdjust / 255.f * 0.6;
		}
		if (actor->ScaleGlow != 1.0) {
			color *= actor->ScaleGlow;
		}
		for (INT i = 0; i < numVerts; i++) {
			FVector& sample = deformed[i];
			if (actor->bRandomFrame) {
				tex = actor->MultiSkins[appCeil((&sample - deformed) / 3.f) % 8];
				if (tex) {
					tex = getTextureWithoutNext(tex, currentTime, actor->LifeFraction());
				}
			}
			if (tex) {

				// Transform the local-space point into world-space
				XMVECTOR xpoint = FVecToDXVec(sample);
				xpoint = XMVector3Transform(xpoint, actorMatrix);
				FVector point = DXVecToFVec(xpoint);

				FTextureInfo texInfo;
				tex->Lock(texInfo, currentTime, -1, this);
				renderSpriteGeo(frame, point, actor->DrawScale, texInfo, baseFlags, color);
				tex->Unlock(texInfo);
			}
		}
		return;
	}

	STAT(clockFast(GStat.SkelRenderTime));
	STAT(clockFast(GStat.SkelSetupTime));

	UniqueValueArray<UTexture*> textures(NUM_POLYGROUPS);
	UniqueValueArray<FTextureInfo> texInfos(NUM_POLYGROUPS);

	// Lock all mesh textures
	for (int i = 0; i < NUM_POLYGROUPS; i++) {
		UTexture* tex = actor->SkelGroupSkins[i];
		if (!tex && mesh->PolyGroupSkinNames[i] != NAME_None) {
			tex = (UTexture*)StaticLoadObject(
				UTexture::StaticClass(),
				usedSkel->GetOuter(),
				*mesh->PolyGroupSkinNames[i],
				NULL,
				LOAD_NoFail,
				NULL
			);
		}

		if (!tex) {
			continue;
		}
		tex = tex->Get(currentTime);
		if (textures.insert(i, tex)) {
			FTextureInfo texInfo{};
			textures.at(i)->Lock(texInfo, currentTime, -1, this);
			texInfos.insert(i, texInfo);
		}
		else {
			texInfos.insert(i, texInfos.at(textures.getIndex(tex)));
		}
		if (actor->Style == STY_AlphaBlend) {
			tex->Alpha = actor->AlphaScale;
		}
		else {
			tex->Alpha = 1;
		}
	}

	bool fatten = actor->Fatness != 128;
	FLOAT fatness = (actor->Fatness / 16.0) - 8.0;

	STAT(unclockFast(GStat.SkelSetupTime));
	STAT(clockFast(GStat.SkelDecimateTime));

	FVector* normals = New<FVector>(GMem, numVerts);

	// Calculate normals
	for (int i = 0; i < numVerts; i++) {
		normals[i] = FVector(0, 0, 0);
	}
	for (int i = 0; i < numTris; i++) {
		FVector* points[3];
		Triangle& tri = mesh->tris(i);
		bool mirror = actor->bMirrored;
		points[0] = &deformed[tri.vIndex[mirror ? 2 : 0]];
		points[1] = &deformed[tri.vIndex[1]];
		points[2] = &deformed[tri.vIndex[mirror ? 0 : 2]];
		FVector fNorm = (*points[1] - *points[0]) ^ (*points[2] - *points[0]);
		for (int j = 0; j < 3; j++) {
			normals[tri.vIndex[j]] += fNorm;
		}
	}
	for (int i = 0; i < numVerts; i++) {
		XMVECTOR normal = FVecToDXVec(normals[i]);
		normal = XMVector3Normalize(normal);
		normals[i] = DXVecToFVec(normal);
	}

	STAT(unclockFast(GStat.SkelDecimateTime));
	STAT(clockFast(GStat.SkelClipTime));

	XMMATRIX screenSpaceMat = actorMatrix * FCoordToDXMat(frame->Uncoords);

	SurfKeyBucketVector<FTextureInfo*, FRenderVert> surfaceBuckets;
	surfaceBuckets.reserve(numTris);

	int vertMaxCount = numTris * 3;
	// Process all triangles on the mesh
	for (INT i = 0; i < numTris; i++) {
		Triangle& tri = mesh->tris(i);
		INT texIdx = tri.polygroup;
		DWORD polyFlags = actor->SkelGroupFlags[tri.polygroup];

		polyFlags |= baseFlags;

		if (polyFlags & PF_Invisible) continue;

		FTextureInfo* texInfo = &texInfos.at(texIdx);
		if (!texInfo->Texture) {
			continue;
		}
		float scaleU = texInfo->UScale * texInfo->USize / 256.0;
		float scaleV = texInfo->VScale * texInfo->VSize / 256.0;

		// Sort triangles into surface/flag groups
		std::vector<FRenderVert>& pointsVec = surfaceBuckets.get(texInfo, polyFlags);
		pointsVec.reserve(vertMaxCount);
		STAT(clockFast(GStat.SkelLightTime))
			for (INT j = 0; j < 3; j++) {
				const INT idx = actor->bMirrored ? 2 - j : j;
				FRenderVert& vert = pointsVec.emplace_back();
				vert.Point = deformed[tri.vIndex[idx]];
				vert.Normal = normals[tri.vIndex[idx]];
				vert.U = tri.tex[idx].u;
				vert.V = tri.tex[idx].v;
				if (fatten) {
					vert.Point += vert.Normal * fatness;
				}

				// Calculate the environment UV mapping
				if (polyFlags & PF_Environment) {
					calcEnvMapping(vert, screenSpaceMat, frame);
				}
				vert.U *= scaleU;
				vert.V *= scaleV;
			}
		STAT(unclockFast(GStat.SkelLightTime));
	}

	STAT(unclockFast(GStat.SkelClipTime));

	STAT(clockFast(GStat.SkelRasterTime));

	renderSurfaceBuckets(surfaceBuckets, &ToD3DMATRIX(actorMatrix));

	STAT(unclockFast(GStat.SkelRasterTime));

	for (UTexture* tex : textures) {
		if (!tex)
			continue;
		tex->Unlock(texInfos.at(textures.getIndex(tex)));
	}

	STAT(unclockFast(GStat.SkelRenderTime));

	unguard;
}
#endif
