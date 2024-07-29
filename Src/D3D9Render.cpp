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
	dout << "Static Constructing UD3D9Render!" << std::endl;
	unguard;
}

void UD3D9Render::getLevelModelFacets(FSceneNode* frame, ModelFacets& modelFacets) {
	UModel* model = frame->Level->Model;
	const FLOAT levelTime = frame->Level->GetLevelInfo()->TimeSeconds;
	const UViewport* viewport = frame->Viewport;

	TexFlagKeyMap<std::vector<INT>> texNodes;

	DWORD flagMask = (viewport->Actor->ShowFlags & SHOW_PlayerCtrl) ? ~PF_FlatShaded : ~PF_Invisible;
	flagMask &= ~PF_Highlighted;
	// Prepass to sort all nodes into texture/flag groups
	for (INT iNode = 0; iNode < model->Nodes.Num(); iNode++) {
		const FBspNode* node = &model->Nodes(iNode);
		INT iSurf = node->iSurf;
		const FBspSurf* surf = &model->Surfs(iSurf);
		if (frame->Level->BrushTracker && frame->Level->BrushTracker->SurfIsDynamic(iSurf)) { // It's a mover, skip it!
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

		texNodes[TexFlagKey(texture, flags)].push_back(iNode);
	}

	std::unordered_map<INT, FSurfaceFacet*> surfaceMap;
	for (const std::pair<const TexFlagKey, std::vector<INT>>& texNodePair : texNodes) {
		const TexFlagKey& tfKey = texNodePair.first;
		DWORD flags = tfKey.second;

		surfaceMap.reserve(texNodePair.second.size());
		for (INT iNode : texNodePair.second) {
			const FBspNode& node = model->Nodes(iNode);
			if (node.NumVertices < 3) continue;
			FSurfaceFacet*& facet = surfaceMap[node.iSurf];
			if (!facet) {
				const FBspSurf* surf = &model->Surfs(node.iSurf);
				// New surface, setup...
				facet = New<FSurfaceFacet>(GDynMem);
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
#if !UTGLR_OLD_POLY_CLASSES
					for (int i = 0; i < surf->Nodes.Num(); i++) {
						// Search for a zone actor on any part of the surface since this node may not have it linked.
						const FBspNode& surfNode = model->Nodes(surf->Nodes(i));
						const FZoneProperties* zoneProps = &model->Zones[surfNode.iZone[1]];
						if (zoneProps->ZoneActor) {
							zone = zoneProps->ZoneActor;
							break;
						}
						zoneProps = &model->Zones[surfNode.iZone[0]];
						if (!zone && zoneProps->ZoneActor) {
							zone = zoneProps->ZoneActor;
							break;
						}
					}
#endif

					if (flags & PF_AutoUPan) {
						panU += fmod(levelTime * 35.0 * (zone ? zone->TexUPanSpeed : 1.0), 1024.0);
					}
					if (flags & PF_AutoVPan) {
						panV += fmod(levelTime * 35.0 * (zone ? zone->TexVPanSpeed : 1.0), 1024.0);
					}
				}
#if !RUNE
				if (flags & PF_SmallWavy) {
					panU += 8.0 * sin(levelTime) + 4.0 * cos(2.3 * levelTime);
					panV += 8.0 * cos(levelTime) + 4.0 * sin(2.3 * levelTime);
				}
#endif
				if (panU != 0 || panV != 0) {
					FVector* pan = New<FVector>(GDynMem);
					*pan = FVector(panU, panV, 0);
					// Hide this away in the span coz we're not using it
					facet->Span = (FSpanBuffer*)pan;
				}
			}

			FSavedPoly* poly = (FSavedPoly*)New<BYTE>(GDynMem, sizeof(FSavedPoly) + node.NumVertices * sizeof(FTransform*));
			poly->Next = facet->Polys;
			facet->Polys = poly;
#if !UTGLR_OLD_POLY_CLASSES
			poly->iNode = iNode;
#endif
			poly->NumPts = node.NumVertices;

			// Allocate and store each point
			for (int i = 0; i < poly->NumPts; i++) {
				FVert vert = model->Verts(node.iVertPool + i);
				FTransform* trans = New<FTransform>(VectorMem);
				trans->Point = model->Points(vert.pVertex);
				poly->Pts[i] = trans;
			}
		}

		for (std::pair<const INT, FSurfaceFacet*>& facetPair : surfaceMap) {
			// Sort into opaque and non passes
			RPASS pass = (flags & PF_NoOcclude) ? RPASS::NONSOLID : RPASS::SOLID;
			std::vector<FSurfaceFacet>& facets = modelFacets.facetPairs[pass][tfKey];
			facets.reserve(surfaceMap.size());
			facets.push_back(*facetPair.second);
		}
		surfaceMap.clear();
	}
}

void UD3D9Render::DrawWorld(FSceneNode* frame) {
	guard(UD3D9Render::DrawWorld);
	if (!GRenderDevice->IsA(UD3D9RenderDevice::StaticClass())) {
		dout << "Not using D3D9DrvRTX Device! " << GRenderDevice->GetName() << std::endl;
		Super::DrawWorld(frame);
		return;
	}
#if UNREAL_TOURNAMENT && !UNREAL_TOURNAMENT_OLDUNREAL
	else if (FString(GRenderDevice->GetName()).InStr(TEXT("RenderDeviceProxy")) == 0) {
		appErrorf(TEXT("D3D9DrvRTX for UT v436 is not compatible with v469.\nInstall D3D9DrvRTX for v469 instead!"));
	}
#endif

	UD3D9RenderDevice* d3d9Dev = (UD3D9RenderDevice*)GRenderDevice;

	FMemMark sceneMark(GSceneMem);
	FMemMark memMark(GMem);
	FMemMark dynMark(GDynMem);
	FMemMark vectorMark(VectorMem);

	if (Engine->Audio && !GIsEditor)
		Engine->Audio->RenderAudioGeometry(frame);

	const UViewport* viewport = frame->Viewport;
	AActor* playerActor = NULL;
	if (!viewport->Actor->bBehindView) {
		playerActor = viewport->Actor->ViewTarget ? viewport->Actor->ViewTarget : viewport->Actor;
	}

	//dout << "Starting frame" << std::endl;

	std::vector<AActor*> lightActors;
	lightActors.reserve(frame->Level->Actors.Num());
	std::vector<AActor*> viableActors;
	viableActors.reserve(frame->Level->Actors.Num());
	std::vector<ABrush*> visibleMovers;
	std::vector<ASkyZoneInfo*> skyZones;

	// Sort through all actors and put them in the appropriate pile
	for (int iActor = 0; iActor < frame->Level->Actors.Num(); iActor++) {
		AActor* actor = frame->Level->Actors(iActor);
		if (!actor) continue;
		if (actor->LightType != LT_None) {
			lightActors.push_back(actor);
		}
		if (actor->IsA(ASkyZoneInfo::StaticClass())) {
			skyZones.push_back((ASkyZoneInfo*)actor);
			continue;
		}
		bool isVisible = true;
		isVisible &= actor != playerActor;
		isVisible &= GIsEditor ? !actor->bHiddenEd : !actor->bHidden;
		bool isOwned = actor->IsOwnedBy(frame->Viewport->Actor);
		isVisible &= !actor->bOnlyOwnerSee || (isOwned && !frame->Viewport->Actor->bBehindView);
		isVisible &= !isOwned || !actor->bOwnerNoSee || (isOwned && frame->Viewport->Actor->bBehindView);
		if (isVisible) {
			if (actor->IsA(AMover::StaticClass())) {
				visibleMovers.push_back((ABrush*)actor);
				continue;
			}
#if RUNE
			else if (actor->IsA(APolyobj::StaticClass())) {
				visibleMovers.push_back((ABrush*)actor);
				continue;
			}
			if (actor->bSpecialRender || actor->bCarriedItem) {
				continue;
			}
#endif
			viableActors.push_back(actor);
		}
	}

	// Seems to also update mover bsp nodes for colision decal calculations
	OccludeFrame(frame);

	// Add all actors in view and also any in zones that are visible
	std::unordered_set<AActor*> visibleActors;
	std::unordered_set<INT> visibleZones;
	for (int pass : {0, 1, 2}) {
		for (FBspDrawList* drawList = frame->Draw[pass]; drawList; drawList = drawList->Next) {
			visibleZones.insert(drawList->iZone);
		}
	}
	for (FDynamicSprite* sprite = frame->Sprite; sprite; sprite = sprite->RenderNext) {
		visibleZones.insert(sprite->Actor->Region.ZoneNumber);
		visibleActors.insert(sprite->Actor);
	}
	for (AActor* actor : viableActors) {
		if (visibleZones.count(actor->Region.ZoneNumber)) {
			visibleActors.insert(actor);
		}
	}

	ModelFacets modelFacets;
	getLevelModelFacets(frame, modelFacets);

	d3d9Dev->startWorldDraw(frame);

	std::unordered_map<UTexture*, FTextureInfo> lockedTextures;
	for (RPASS pass : {SOLID, NONSOLID}) {
		DecalMap decalMap;
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
#if !UTGLR_NO_DECALS
			if (viewport->GetOuterUClient()->Decals) {
				for (FSurfaceFacet& facet : facets) {
					getFacetDecals(frame, facet, decalMap, lockedTextures);
				}
			}
#endif
		}
		// Render all the decals
		for (const auto& decalsPair : decalMap) {
			FTextureInfo *const& texInfo = decalsPair.first;
			const std::vector<std::vector<FTransTexture>>& decals = decalsPair.second;
			for (std::vector<FTransTexture> decal : decals) {
				int numPts = decal.size();
				FTransTexture** pointsPtrs = new FTransTexture * [numPts];
				for (int i = 0; i < numPts; i++) {
					pointsPtrs[i] = &decal[i];
				}
				d3d9Dev->DrawGouraudPolygon(frame, *texInfo, pointsPtrs, numPts, PF_Modulated, NULL);
				delete[] pointsPtrs;
			}
		}
		if (pass == RPASS::SOLID) {
			for (ABrush* mover : visibleMovers) {
				d3d9Dev->renderMover(frame, mover);
			}
		}
		for (AActor* actor : visibleActors) {
			UBOOL bTranslucent = actor->Style == STY_Translucent;
			if ((pass == RPASS::NONSOLID && bTranslucent) || (pass == RPASS::SOLID && !bTranslucent)) {
				drawActorSwitch(frame, d3d9Dev, actor);
			}
		}
	}
	for (std::pair<UTexture* const, FTextureInfo>& entry : lockedTextures) {
		entry.first->Unlock(entry.second);
	}

	for (ASkyZoneInfo* zone : skyZones) {
		d3d9Dev->renderSkyZoneAnchor(zone, &frame->Coords.Origin);
	}

	d3d9Dev->renderLights(lightActors);

	d3d9Dev->endWorldDraw(frame);

	// Render view model actor and extra HUD stuff
	if (!GIsEditor && playerActor && (viewport->Actor->ShowFlags & SHOW_Actors)) {
		GUglyHackFlags |= 1;
		playerActor->eventRenderOverlays(viewport->Canvas);
		GUglyHackFlags &= ~1;
	}

	memMark.Pop();
	dynMark.Pop();
	sceneMark.Pop();
	vectorMark.Pop();
	unguard;
}

void UD3D9Render::drawActorSwitch(FSceneNode* frame, UD3D9RenderDevice* d3d9Dev, AActor* actor, ParentCoord* parentCoord)
{
	SpecialCoord specialCoord;
	if ((actor->DrawType == DT_Sprite || actor->DrawType == DT_SpriteAnimOnce || (frame->Viewport->Actor->ShowFlags & SHOW_ActorIcons)) && actor->Texture) {
		d3d9Dev->renderSprite(frame, actor);
	}
	else if (actor->DrawType == DT_Mesh) {
		d3d9Dev->renderMeshActor(frame, actor, &specialCoord);
	}
#if RUNE
	else if (actor->DrawType == DT_SkeletalMesh) {
		drawSkeletalActor(frame, d3d9Dev, actor, parentCoord);
	}
	else if (actor->DrawType == DT_ParticleSystem && actor->IsA(AParticleSystem::StaticClass())) {
		//DrawParticleSystem(frame, actor, nullptr, parentCoord ? parentCoord->localCoord : GMath.UnitCoords);
		d3d9Dev->renderParticleSystemActor(frame, (AParticleSystem*)actor, parentCoord ? parentCoord->localCoord : GMath.UnitCoords);
	}
#endif
	if (actor->IsA(APawn::StaticClass())) {
		drawPawnExtras(frame, d3d9Dev, (APawn*)actor, specialCoord);
	}
}

#if !UTGLR_NO_DECALS
void UD3D9Render::getFacetDecals(FSceneNode* frame, const FSurfaceFacet& facet, DecalMap& decals, std::unordered_map<UTexture*, FTextureInfo>& lockedTextures) {
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

		std::vector<std::vector<FTransTexture>>& decalPoints = decals[texInfo];

		for (FSavedPoly* poly = facet.Polys; poly; poly = poly->Next) {
			INT findIndex;
			if (!decal->Nodes.FindItem(poly->iNode, findIndex) && decal->Nodes.Num() > 0) {
				continue;
			}
			std::vector<FTransTexture> points;

			ClipDecal(frame, decal, &surf, poly, points);

			if (points.size() < 3) continue;

			// Calculate the normal from the cross of first point and the second and third points
			FVector v1 = points[1].Point - points[0].Point;
			FVector v2 = points[2].Point - points[0].Point;
			FVector normal = v1 ^ v2;
			normal.Normalize();
			if ((normal | model->Vectors(surf.vNormal)) < 0) {
				std::reverse(points.begin(), points.end()); // Reverse the face so it points the correct way
			}

			decalPoints.push_back(points);
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
		FTransTexture& pt = decalPts.emplace_back();
		pt.Point = decal->Vertices[i] + decalOffset;
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
		for (unsigned int decalIdx = 0; decalIdx < decalPts.size(); decalIdx++) {
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
		for (unsigned int i = 0; i < decalPts.size(); i++) {
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
#endif

void UD3D9Render::drawPawnExtras(FSceneNode* frame, UD3D9RenderDevice* d3d9Dev, APawn* pawn, SpecialCoord& specialCoord) {
#if !RUNE
	AInventory* weapon = pawn->Weapon;
	if (specialCoord.exists && weapon && weapon->ThirdPersonMesh) {
		specialCoord.enabled = true;
		UMesh* origMesh = weapon->Mesh;
		FLOAT origDrawScale = weapon->DrawScale;
		weapon->Mesh = weapon->ThirdPersonMesh;
		weapon->DrawScale = weapon->ThirdPersonScale;
		d3d9Dev->renderMeshActor(frame, weapon, &specialCoord);
#if UNREAL_TOURNAMENT
		if (weapon->bSteadyFlash3rd) {
			weapon->bSteadyToggle = !weapon->bSteadyToggle;
		}
		if (weapon->MuzzleFlashMesh &&
			(weapon->bSteadyFlash3rd && (!weapon->bToggleSteadyFlash || weapon->bSteadyToggle)) ||
			(!weapon->bFirstFrame && (weapon->FlashCount != weapon->OldFlashCount))) {
			BYTE origStyle = weapon->Style;
			UTexture* origTexture = weapon->Texture;
			bool origParticles = weapon->bParticles;
			FName origAnim = weapon->AnimSequence;
			FLOAT origFrame = weapon->AnimFrame;
			INT origLit = weapon->bUnlit;
			weapon->Mesh = weapon->MuzzleFlashMesh;
			weapon->DrawScale = weapon->MuzzleFlashScale;
			weapon->Style = weapon->MuzzleFlashStyle;
			weapon->Texture = weapon->MuzzleFlashTexture;
			weapon->bParticles = weapon->bMuzzleFlashParticles;
			weapon->AnimSequence = NAME_All;
			weapon->AnimFrame = appFrand();
			weapon->bUnlit = true;
			d3d9Dev->renderMeshActor(frame, weapon, &specialCoord);
			weapon->Style = origStyle;
			weapon->Texture = origTexture;
			weapon->bParticles = origParticles;
			weapon->AnimSequence = origAnim;
			weapon->AnimFrame = origFrame;
			weapon->bUnlit = origLit;
		}
		weapon->OldFlashCount = weapon->FlashCount;
		weapon->bFirstFrame = 0;
#endif
		weapon->Mesh = origMesh;
		weapon->DrawScale = origDrawScale;
	}
#endif
#if !UTGLR_NO_PLAYER_FLAG
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
#endif
}

#if RUNE
void UD3D9Render::drawSkeletalActor(FSceneNode* frame, UD3D9RenderDevice* d3d9Dev, AActor* actor, const ParentCoord* parentCoord) {
	d3d9Dev->renderSkeletalMeshActor(frame, actor, parentCoord ? &parentCoord->worldCoord : nullptr);
	USkelModel* skel = actor->Skeletal;
	if (!skel) return;
	skel->GetFrame(actor, parentCoord ? parentCoord->localCoord : GMath.UnitCoords, 0, nullptr); // Updates the skel position with the real actor coords
	for (int i = 0; i < MAX_JOINTS; i++) {
		AActor* child = actor->JointChild[i];
		if (!child) continue;
		FCacheItem* cacheItem = nullptr;
		DynSkel* dynSkel = skel->LockDSkel(actor, cacheItem);
		DynJoint* dynJoint = &dynSkel->joint[i];
		FCoords coords = dynJoint->coords;
		skel->UnlockDSkel(cacheItem);
		//FString childName = FString(child->GetName());
		//if (childName != L"torch1" && childName != L"torchfire1") {
		//	continue;
		//}
		//dout << L"Parent: " << actor->GetName() << L", Child: " << child->GetName() << std::endl;
		ParentCoord childCoords;
		childCoords.worldCoord = coords;
		coords /= child->Rotation;
		if (child->DrawType == DT_Sprite || child->IsA(AParticleSystem::StaticClass())) {
			FVector particleLoc(0, 0, 0);
			particleLoc = particleLoc.TransformPointBy(coords);
			child->XLevel->FarMoveActor(child, particleLoc, 1, 1);
		}
		coords /= child->Location;
		childCoords.localCoord = coords;
		drawActorSwitch(frame, d3d9Dev, child, &childCoords);
		//FDynamicSprite sprite(child);
		//DrawActorSprite(frame, &sprite, coords);
	}
}
#endif

void UD3D9Render::DrawActor(FSceneNode* frame, AActor* actor) {
	guard(UD3D9Render::DrawActor);
	if (!GRenderDevice->IsA(UD3D9RenderDevice::StaticClass())) {
		Super::DrawActor(frame, actor);
		return;
	}
	UD3D9RenderDevice* d3d9Dev = (UD3D9RenderDevice*)GRenderDevice;
	// TODO: fix this muzzle flash schtuff
	d3d9Dev->EndBuffering(); // muzzle flash is drawing before weapon
	d3d9Dev->startWorldDraw(frame);
	d3d9Dev->renderMeshActor(frame, actor);
	//dout << "Drawing actor! " << actor->GetName() << std::endl;
	d3d9Dev->endWorldDraw(frame);
	unguard;
}
