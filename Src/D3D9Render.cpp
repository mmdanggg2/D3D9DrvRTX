#include "D3D9Render.h"

#include <unordered_set>

IMPLEMENT_CLASS(UD3D9Render);

UD3D9Render::UD3D9Render() : URender() {
	guard(UD3D9Render::UD3D9Render);
	dout << "Constructing UD3D9Render!" << std::endl;
	unguard;
}

void UD3D9Render::StaticConstructor() {
	guard(UD3D9Render::StaticConstructor);
	URender::StaticConstructor();
	ods_stream() << "Static Constructing UD3D9Render!" << std::endl;
	unguard;
}

void UD3D9Render::getLevelModelFacets(FSceneNode* frame, ModelFacets& modelFacets) {
	UModel* model = frame->Level->Model;
	const FLOAT levelTime = frame->Level->GetLevelInfo()->TimeSeconds;
	const UViewport* viewport = frame->Viewport;

	TexFlagKeyMap<std::vector<INT>> texNodes;

	DWORD flagMask = (viewport->Actor->ShowFlags & SHOW_PlayerCtrl) ? ~PF_FlatShaded : ~PF_Invisible;
	flagMask &= ~PF_Highlighted;
	for (INT iNode = 0; iNode < model->Nodes.Num(); iNode++) {
		const FBspNode* node = &model->Nodes(iNode);
		INT iSurf = node->iSurf;
		const FBspSurf* surf = &model->Surfs(iSurf);
		if (surf->Nodes.Num() == 0) { // Must be a mover, skip it!
			//dout << L"Surf " << iSurf << L" has no nodes!" << std::endl;
			continue;
		}
		UTexture* texture = surf->Texture;
		if (!texture) {
			texture = viewport->Actor->Level->DefaultTexture;
		}
		DWORD flags = surf->PolyFlags;
		flags |= texture->PolyFlags;
		flags |= viewport->ExtraPolyFlags;
		flags &= flagMask;

		if (flags & PF_Invisible) {
			continue;
		}
		if (flags & PF_Mirrored) {
			flags &= ~PF_NoOcclude;
		}

		texNodes[TexFlagKey(texture, flags)].push_back(iNode);
	}

	for (const std::pair<const TexFlagKey, std::vector<INT>>& texNodePair : texNodes) {
		DWORD flags = texNodePair.first.second;

		std::unordered_map<INT, FSurfaceFacet> surfaceMap;
		surfaceMap.reserve(texNodePair.second.size());
		for (INT iNode : texNodePair.second) {
			const FBspNode& node = model->Nodes(iNode);
			FBspSurf* surf = &model->Surfs(node.iSurf);
			FSurfaceFacet* facet;
			if (!surfaceMap.count(node.iSurf)) {
				// New surface, setup...
				facet = &surfaceMap[node.iSurf];
				facet->Polys = NULL;
				facet->Span = NULL;
				facet->MapCoords = FCoords(
					model->Points(surf->pBase),
					model->Vectors(surf->vTextureU),
					model->Vectors(surf->vTextureV),
					model->Vectors(surf->vNormal)
				);
				facet->MapUncoords = facet->MapCoords.Inverse();

				FLOAT panU = surf->PanU;
				FLOAT panV = surf->PanV;
				if (flags & PF_AutoUPan || flags & PF_AutoVPan) {
					const AZoneInfo* zone = nullptr;
					for (int i = 0; i < surf->Nodes.Num(); i++) {
						// Search for a zone actor on any part of the surface since this node may not have it linked.
						const FBspNode& surfNode = model->Nodes(surf->Nodes(i));
						const FZoneProperties* zoneProps = &model->Zones[surfNode.iZone[0]];
						if (zoneProps->ZoneActor) {
							zone = zoneProps->ZoneActor;
							break;
						}
						zoneProps = &model->Zones[surfNode.iZone[1]];
						if (!zone && zoneProps->ZoneActor) {
							zone = zoneProps->ZoneActor;
							break;
						}
					}

					if (flags & PF_AutoUPan) {
						panU += fmod(levelTime * 35.0 * (zone ? zone->TexUPanSpeed : 1.0), 1024.0);
					}
					if (flags & PF_AutoVPan) {
						panV += fmod(levelTime * 35.0 * (zone ? zone->TexVPanSpeed : 1.0), 1024.0);
					}
				}
				if (flags & PF_SmallWavy) {
					panU += 8.0 * appSin(levelTime) + 4.0 * appCos(2.3 * levelTime);
					panV += 8.0 * appCos(levelTime) + 4.0 * appSin(2.3 * levelTime);
				}
				if (panU != 0 || panV != 0) {
					FVector* pan = New<FVector>(GDynMem);
					*pan = FVector(panU, panV, 0);
					// Hide this away in the span coz we're not using it
					facet->Span = (FSpanBuffer*)pan;
				}
			} else {
				facet = &surfaceMap[node.iSurf];
			}

			//dout << L"\t Node " << iNode << std::endl;
			FSavedPoly* poly = (FSavedPoly*)New<BYTE>(GDynMem, sizeof(FSavedPoly) + node.NumVertices * sizeof(FTransform*));
			poly->Next = facet->Polys;
			facet->Polys = poly;
			poly->iNode = iNode;
			poly->NumPts = node.NumVertices;

			for (int i = 0; i < poly->NumPts; i++) {
				FVert vert = model->Verts(node.iVertPool + i);
				FTransform* trans = new(VectorMem)FTransform;
				trans->Point = model->Points(vert.pVertex);
				poly->Pts[i] = trans;
			}
		}

		for (std::pair<const INT, FSurfaceFacet>& facetPair : surfaceMap) {
			RPASS pass = (flags & PF_NoOcclude) ? RPASS::NONSOLID : RPASS::SOLID;
			modelFacets.facetPairs[pass][texNodePair.first].push_back(std::move(facetPair.second));
		}
	}
}

void UD3D9Render::DrawWorld(FSceneNode* frame) {
	guard(UD3D9Render::DrawWorld);
	if (GRenderDevice->IsA(UD3D9RenderDevice::StaticClass())) {
		FMemMark sceneMark(GSceneMem);
		FMemMark memMark(GMem);
		FMemMark dynMark(GDynMem);
		FMemMark vectorMark(VectorMem);
		UD3D9RenderDevice* d3d9Dev = (UD3D9RenderDevice*)GRenderDevice;

		const UViewport* viewport = frame->Viewport;
		AActor* playerActor = NULL;
		if (!viewport->Actor->bBehindView) {
			playerActor = viewport->Actor->ViewTarget ? viewport->Actor->ViewTarget : viewport->Actor;
		}

		//dout << "Starting frame" << std::endl;

		d3d9Dev->startWorldDraw(frame);
		// Seems to update mover bsp nodes for decal calculations
		//OccludeBsp(frame);
		//SetupDynamics(frame, playerActor);

		std::unordered_set<INT> visibleZones;
		std::unordered_set<INT> visibleSurfs;
		{
			TArray<INT> visibleSurfsTArr;
			auto savedRotation = viewport->Actor->ViewRotation;
			GetVisibleSurfs(const_cast<UViewport*>(viewport), visibleSurfsTArr);
			viewport->Actor->ViewRotation = savedRotation;
			for (int i = 0; i < visibleSurfsTArr.Num(); i++) {
				visibleSurfs.insert(visibleSurfsTArr(i));// std lib is more efficient
			}

			for (int i = 0; i < frame->Level->Model->Nodes.Num(); i++) {
				FBspNode& node = frame->Level->Model->Nodes(i);
				if (visibleSurfs.count(node.iSurf)) {
					visibleZones.insert(node.iZone[0]);
					visibleZones.insert(node.iZone[1]);
				}
			}
		}

		ModelFacets modelFacets;
		getLevelModelFacets(frame, modelFacets);

		std::vector<AActor*> visibleActors;
		std::vector<AMover*> visibleMovers;

		for (int iActor = 0; iActor < frame->Level->Actors.Num(); iActor++) {
			AActor* actor = frame->Level->Actors(iActor);
			if (!actor) continue;
			if (actor->IsA(AMover::StaticClass())) {
				visibleMovers.push_back((AMover*)actor);
				continue;
			}
			if (!visibleZones.count(actor->Region.ZoneNumber)) continue;
			bool isVisible = true;
			isVisible &= actor != playerActor;
			isVisible &= GIsEditor ? !actor->bHiddenEd : !actor->bHidden;
			bool isOwned = actor->IsOwnedBy(frame->Viewport->Actor);
			isVisible &= !actor->bOnlyOwnerSee || (isOwned && !frame->Viewport->Actor->bBehindView);
			isVisible &= !isOwned || !actor->bOwnerNoSee || (isOwned && frame->Viewport->Actor->bBehindView);
			if (isVisible) {
				visibleActors.push_back(actor);
			}
		}

		std::unordered_map<UTexture*, FTextureInfo> lockedTextures;
		for (RPASS pass : {SOLID, NONSOLID}) {
			for (std::pair<const TexFlagKey, std::vector<FSurfaceFacet>>& facetPair : modelFacets.facetPairs[pass]) {
				UTexture* texture = facetPair.first.first;
				DWORD flags = facetPair.first.second;
				std::vector<FSurfaceFacet>& facets = facetPair.second;

				FTextureInfo* texInfo;
				if (!lockedTextures.count(texture)) {
					texInfo = &lockedTextures[texture];
					texture->Lock(*texInfo, viewport->CurrentTime, -1, viewport->RenDev);
				} else {
					texInfo = &lockedTextures[texture];
				}

				FSurfaceInfo surface{};
				surface.Level = frame->Level;
				surface.PolyFlags = flags;
				surface.Texture = texInfo;

				d3d9Dev->drawLevelSurfaces(frame, surface, facets);
				if (viewport->GetOuterUClient()->Decals) {
					for (FSurfaceFacet facet : facets) {
						drawFacetDecals(frame, d3d9Dev, facet, lockedTextures);
					}
				}
			}
			for (AMover* mover : visibleMovers) {
				d3d9Dev->renderMover(frame, mover);
			}
			for (AActor* actor : visibleActors) {
				UBOOL bTranslucent = actor->Style == STY_Translucent;
				if ((pass == RPASS::NONSOLID && bTranslucent) || (pass == RPASS::SOLID && !bTranslucent)) {
					SpecialCoord specialCoord;
					if ((actor->DrawType == DT_Sprite || actor->DrawType == DT_SpriteAnimOnce || (viewport->Actor->ShowFlags & SHOW_ActorIcons)) && actor->Texture) {
						d3d9Dev->renderSprite(frame, actor);
					} else if (actor->DrawType == DT_Mesh) {
						d3d9Dev->renderMeshActor(frame, actor, &specialCoord);
					}
					if (actor->IsA(APawn::StaticClass())) {
						drawPawnExtras(frame, d3d9Dev, (APawn*)actor, specialCoord);
					}
				}
			}
		}
		for (std::pair<UTexture* const, FTextureInfo>& entry : lockedTextures) {
			entry.first->Unlock(entry.second);
		}

		std::vector<AActor*> lightActors;
		lightActors.reserve(frame->Level->Actors.Num());
		for (int i = 0; i < frame->Level->Actors.Num(); i++) {
			AActor* actor = frame->Level->Actors(i);
			if (actor && actor->LightType != LT_None) {
				lightActors.push_back(actor);
			}
		}
		d3d9Dev->renderLights(lightActors);

		d3d9Dev->endWorldDraw(frame);

		// Render view model actor and extra HUD stuff
		if (!GIsEditor && playerActor && (frame->Viewport->Actor->ShowFlags & SHOW_Actors)) {
			GUglyHackFlags |= 1;
			playerActor->eventRenderOverlays(frame->Viewport->Canvas);
			GUglyHackFlags &= ~1;
		}

		memMark.Pop();
		dynMark.Pop();
		sceneMark.Pop();
		vectorMark.Pop();
		return;
	}
	Super::DrawWorld(frame);
	unguard;
}

void UD3D9Render::drawFacetDecals(FSceneNode* frame, UD3D9RenderDevice* d3d9Dev, FSurfaceFacet& facet, std::unordered_map<UTexture*, FTextureInfo>& lockedTextures) {
	const UViewport* viewport = frame->Viewport;
	const UModel* model = frame->Level->Model;
	const FBspSurf& surf = model->Surfs(model->Nodes(facet.Polys->iNode).iSurf);
	for (int i = 0; i < surf.Decals.Num(); i++) {
		const FDecal* decal = &surf.Decals(i);
		UTexture* texture;
		if (decal->Actor->Texture) {
			texture = decal->Actor->Texture->Get(viewport->CurrentTime);
		} else {
			texture = viewport->Actor->Level->DefaultTexture;
		}
		FTextureInfo* texInfo;
		if (!lockedTextures.count(texture)) {
			texInfo = &lockedTextures[texture];
			texture->Lock(*texInfo, viewport->CurrentTime, -1, viewport->RenDev);
		} else {
			texInfo = &lockedTextures[texture];
		}

		for (FSavedPoly* poly = facet.Polys; poly; poly = poly->Next) {
			INT findIndex;
			if (!decal->Nodes.FindItem(poly->iNode, findIndex)) {
				continue;
			}
			std::vector<FTransTexture> points;
			ClipDecal(frame, decal, &surf, poly, points);

			int numPts = points.size();
			if (numPts < 3) continue;

			// Calculate the normal from the cross of first point and the second and third points
			FVector v1 = points[1].Point - points[0].Point;
			FVector v2 = points[2].Point - points[0].Point;
			FVector normal = v1 ^ v2;
			normal.Normalize();
			if ((normal | model->Vectors(surf.vNormal)) < 0) {
				std::reverse(points.begin(), points.end()); // Reverse the face so it points the correct way
			}

			FTransTexture** pointsPtrs = new FTransTexture*[numPts];
			for (int i = 0; i < numPts; i++) {
				pointsPtrs[i] = &points[i];
			}

			decal->Actor->LastRenderedTime = decal->Actor->Level->TimeSeconds;
			d3d9Dev->DrawGouraudPolygon(frame, *texInfo, pointsPtrs, numPts, PF_Modulated, NULL);
			delete[] pointsPtrs;
		}
	}
}

void UD3D9Render::ClipDecal(FSceneNode* frame, const FDecal* decal, const FBspSurf* surf, FSavedPoly* poly, std::vector<FTransTexture>& decalPts) {
	UModel* const& model = frame->Level->Model;
	FVector& surfNormal = model->Vectors(surf->vNormal);
	FVector& surfBase = model->Points(surf->pBase);

	FVector decalOffset = surfNormal;
	decalOffset.Normalize();
	decalOffset = decalOffset * 0.4; // offset from wall
	decalOffset += surfBase;
	for (int i = 0; i < 4; i++) {
		// Add the 4 decal points to start
		FTransTexture pt{};
		pt.Point = decal->Vertices[i] + decalOffset;
		decalPts.push_back(std::move(pt));
	}

	int polyPrevIdx = poly->NumPts - 1;
	for (int polyIdx = 0; polyIdx < poly->NumPts; polyIdx++) {
		FVector edgeVector = poly->Pts[polyPrevIdx]->Point - poly->Pts[polyIdx]->Point;
		FVector clipNorm = edgeVector ^ surfNormal;
		FPlane clipPlane = FPlane(poly->Pts[polyIdx]->Point, clipNorm);

		std::vector<bool> isInside;
		// Calculate if the point is isInside or outside the clip plane
		for (FTransTexture& point : decalPts) {
			isInside.push_back(clipPlane.PlaneDot(point.Point) >= 0);
		}
		for (int decalIdx = 0; decalIdx < decalPts.size(); decalIdx++) {
			int decalNextIdx = (decalIdx + 1) % decalPts.size();
			if ((isInside[decalIdx] && !isInside[decalNextIdx]) || (!isInside[decalIdx] && isInside[decalNextIdx])) {
				FTransTexture newPt{};
				newPt.Point = FLinePlaneIntersection(decalPts[decalIdx].Point, decalPts[decalNextIdx].Point, clipPlane);
				decalPts.insert(decalPts.begin() + decalIdx + 1, newPt);
				isInside.insert(isInside.begin() + decalIdx + 1, true);
				decalIdx++;
			}
		}
		// Remove outside points
		for (int i = 0; i < decalPts.size(); i++) {
			if (!isInside[i]) {
				decalPts.erase(decalPts.begin() + i);
				isInside.erase(isInside.begin() + i);
				i--;
			}
		}
		if (decalPts.empty()) return;
		polyPrevIdx = polyIdx;
	}
	FLOAT vertColor = Clamp(decal->Actor->ScaleGlow * 0.5f + decal->Actor->AmbientGlow / 256.f, 0.f, 1.f);
	FVector edgeU = decal->Vertices[1] - decal->Vertices[0];
	FVector edgeV = decal->Vertices[3] - decal->Vertices[0];
	for (FTransTexture& point : decalPts) {
		// Calculate point UVs, assumes square
		FVector relativePoint = point.Point - surfBase - decal->Vertices[0];
		point.U = ((relativePoint | edgeU) / edgeU.Size()) / decal->Actor->DrawScale;
		point.V = ((relativePoint | edgeV) / edgeV.Size()) / decal->Actor->DrawScale;

		point.Light = FVector(vertColor, vertColor, vertColor);
	}
}

void UD3D9Render::drawPawnExtras(FSceneNode* frame, UD3D9RenderDevice* d3d9Dev, APawn* pawn, SpecialCoord& specialCoord) {
	AInventory* weapon = pawn->Weapon;
	if (specialCoord.exists && weapon && weapon->ThirdPersonMesh) {
		specialCoord.enabled = true;
		Exchange(weapon->Mesh, weapon->ThirdPersonMesh);
		Exchange(weapon->DrawScale, weapon->ThirdPersonScale);
		d3d9Dev->renderMeshActor(frame, weapon, &specialCoord);
		Exchange(weapon->Mesh, weapon->ThirdPersonMesh);
		Exchange(weapon->DrawScale, weapon->ThirdPersonScale);
		if (weapon->bSteadyFlash3rd) {
			weapon->bSteadyToggle = !weapon->bSteadyToggle;
		}
		if (weapon->MuzzleFlashMesh &&
			(weapon->bSteadyFlash3rd && (!weapon->bToggleSteadyFlash || weapon->bSteadyToggle)) ||
			(!weapon->bFirstFrame && (weapon->FlashCount != weapon->OldFlashCount))) {
			Exchange(weapon->Mesh, weapon->MuzzleFlashMesh);
			Exchange(weapon->DrawScale, weapon->MuzzleFlashScale);
			Exchange(weapon->Style, weapon->MuzzleFlashStyle);
			Exchange(weapon->Texture, weapon->MuzzleFlashTexture);
			bool origParticles = weapon->bParticles;
			FName origAnim = weapon->AnimSequence;
			FLOAT origFrame = weapon->AnimFrame;
			INT origLit = weapon->bUnlit;
			weapon->bParticles = weapon->bMuzzleFlashParticles;
			weapon->AnimSequence = NAME_All;
			weapon->AnimFrame = appFrand();
			weapon->bUnlit = true;
			d3d9Dev->renderMeshActor(frame, weapon, &specialCoord);
			weapon->bParticles = origParticles;
			weapon->AnimSequence = origAnim;
			weapon->AnimFrame = origFrame;
			weapon->bUnlit = origLit;
			Exchange(weapon->Mesh, weapon->MuzzleFlashMesh);
			Exchange(weapon->DrawScale, weapon->MuzzleFlashScale);
			Exchange(weapon->Style, weapon->MuzzleFlashStyle);
			Exchange(weapon->Texture, weapon->MuzzleFlashTexture);
		}
		weapon->OldFlashCount = weapon->FlashCount;
		weapon->bFirstFrame = 0;
	}
	if (pawn->PlayerReplicationInfo && pawn->PlayerReplicationInfo->HasFlag) {
		AActor* flag = pawn->PlayerReplicationInfo->HasFlag;

		FVector origLoc = flag->Location;
		FRotator origRot = flag->Rotation;
		float dist = Clamp(2.0f + 20.0f * GMath.SinTab(flag->Rotation.Pitch), 2.0f, 3.0f);
		flag->Location = pawn->Location - dist * pawn->CollisionRadius * pawn->Rotation.Vector() + FVector(0, 0, 0.7 * pawn->BaseEyeHeight);
		flag->Rotation = pawn->Rotation;
		d3d9Dev->renderMeshActor(frame, flag);
		flag->Location = origLoc;
		flag->Rotation = origRot;
	}
}

void UD3D9Render::DrawActor(FSceneNode* frame, AActor* actor) {
	guard(UD3D9Render::DrawActor);
	if (GRenderDevice->IsA(UD3D9RenderDevice::StaticClass())) {
		UD3D9RenderDevice* d3d9Dev = (UD3D9RenderDevice*)GRenderDevice;
		d3d9Dev->EndBuffering(); // muzzle flash is drawing before weapon
		d3d9Dev->startWorldDraw(frame);
		d3d9Dev->renderMeshActor(frame, actor);
		//dout << "Drawing actor! " << actor->GetName() << std::endl;
		d3d9Dev->endWorldDraw(frame);
		return;
	}
	Super::DrawActor(frame, actor);
	unguard;
}
