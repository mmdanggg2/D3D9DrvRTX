#include "D3D9Render.h"

#include <bit>
#include <bitset>
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
#if UTGLR_HP_ENGINE
	cachedLevelModel.facetsMem.Init(8192, TEXT("CacheLevelFacetMem"));
#else
	cachedLevelModel.facetsMem.Init(8192);
#endif
	cachedLevelModel.facetsMemMark = FMemMark(cachedLevelModel.facetsMem);
	unguard;
}

decltype(UD3D9Render::cachedLevelModel) UD3D9Render::cachedLevelModel;

void UD3D9Render::getLevelModelFacets(FSceneNode* frame, ModelFacets& modelFacets) {
	UModel* model = frame->Level->Model;
	const UViewport* viewport = frame->Viewport;

	struct ZoneNodes {
		BYTE zone;
		std::vector<INT> nodes;
	};

	// Create an array of all surfaces to hold their facets and any nodes of each surface
	std::vector<std::vector<ZoneNodes>> surfaceNodes(model->Surfs.Num());

	for (INT iNode = 0; iNode < model->Nodes.Num(); iNode++) {
		const FBspNode& node = model->Nodes(iNode);
		if (node.NumVertices < 3) continue;
		const INT& iSurf = node.iSurf;
		const BYTE& zone = node.iZone[1];
		std::vector<ZoneNodes>& surfZoneNodes = surfaceNodes[iSurf];
		ZoneNodes* zoneNodes = nullptr;
		for (ZoneNodes& zn : surfZoneNodes) {
			if (zn.zone == zone) {
				zoneNodes = &zn;
				break;
			}
		}
		if (!zoneNodes) {
			zoneNodes = &surfZoneNodes.emplace_back();
			zoneNodes->zone = zone;
#if !UTGLR_OLD_POLY_CLASSES
			zoneNodes->nodes.reserve(model->Surfs(iSurf).Nodes.Num());
#endif
		}
		zoneNodes->nodes.push_back(iNode);
	}

	DWORD flagMask = (viewport->Actor->ShowFlags & SHOW_PlayerCtrl) ? ~PF_FlatShaded : ~PF_Invisible;
	flagMask &= ~(PF_Highlighted | PF_LowShadowDetail | PF_HighShadowDetail);
	// Prepass to sort all surfs into texture/flag groups
	for (INT iSurf = 0; iSurf < model->Surfs.Num(); iSurf++) {
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

		if (flags & (PF_Invisible | PF_FakeBackdrop)) {
			continue;
		}

		// Sort into opaque and non passes
		RPASS pass = (flags & PF_NoOcclude) ? RPASS::NONSOLID : RPASS::SOLID;
		for (ZoneNodes& zoneNodes: surfaceNodes[iSurf]) {
			SurfaceData& surfData = modelFacets.facetPairs[zoneNodes.zone][pass].get(texture, flags).emplace_back();
			surfData.iSurf = iSurf;
			surfData.nodes = std::move(zoneNodes.nodes);
			surfData.calculateSurfaceFacet(frame->Level, flags);
		}
	}
}

void UD3D9Render::SurfaceData::calculateSurfaceFacet(ULevel* level, const DWORD flags) {
	UModel* model = level->Model;
	const FLOAT levelTime = level->GetLevelInfo()->TimeSeconds;
	const FBspSurf* surf = &model->Surfs(this->iSurf);
	FSurfaceFacet*& facet = this->facet;
	// New surface, setup...
	facet = New<FSurfaceFacet>(cachedLevelModel.facetsMem);
	facet->Polys = NULL;
	facet->Span = NULL;
	facet->MapCoords = FCoords(
		model->Points(surf->pBase),
		model->Vectors(surf->vTextureU),
		model->Vectors(surf->vTextureV),
		model->Vectors(surf->vNormal)
	);
	//facet->MapUncoords = facet->MapCoords.Inverse(); unused

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
		FVector* pan = New<FVector>(cachedLevelModel.facetsMem);
		*pan = FVector(panU, panV, 0);
		// Hide this away in the span coz we're not using it
		facet->Span = (FSpanBuffer*)pan;
	}

	for (const INT& iNode : this->nodes) {
		const FBspNode& node = model->Nodes(iNode);
		FSavedPoly* poly = (FSavedPoly*)New<BYTE>(cachedLevelModel.facetsMem, sizeof(FSavedPoly) + node.NumVertices * sizeof(FTransform*));
		poly->Next = facet->Polys;
		facet->Polys = poly;
#if !UTGLR_OLD_POLY_CLASSES
		poly->iNode = iNode;
#endif
		poly->NumPts = node.NumVertices;

		// Allocate and store each point
		FTransform* transArr = New<FTransform>(cachedLevelModel.facetsMem, poly->NumPts);
		for (int i = 0; i < poly->NumPts; i++) {
			FVert& vert = model->Verts(node.iVertPool + i);
			FTransform* trans = transArr + i;
			trans->Point = model->Points(vert.pVertex);
			poly->Pts[i] = trans;
		}
	}
}

static bool getRenderInterfaceActors(AActor* actor, FSceneNode* frame, std::vector<AActor*>& actorsFound) {
	UClass*& iterClass = actor->RenderIteratorClass;
	URenderIterator*& iface = actor->RenderInterface;
	if (iterClass) {
		if (!iface || !iface->IsValid()) {
			iface = ConstructObject<URenderIterator>(iterClass, actor);
		}
#if UNREAL_GOLD_OLDUNREAL
		for (AActor* a = iface->OnGetActors(frame); a; a = a->Target) {
#else
#if UTGLR_RENDERITERATOR_ACTOR_INIT
		iface->Init(frame->Viewport->Actor);
#else
		iface->Init(frame);
#endif
		for (iface->First(); !iface->IsDone(); iface->Next()) {
			AActor* a = iface->CurrentItem();
#endif
			actorsFound.push_back(a);
		}
#if !UTGLR_RENDERITERATOR_ACTOR_INIT && !UNREAL_GOLD_OLDUNREAL
		iface->UnInit();
#endif
		return true;
	}
	else if (iface) {
		iface->Destroy();
		iface = nullptr;
	}
	return false;
}

void UD3D9Render::DrawWorld(FSceneNode* frame) {
	guard(UD3D9Render::DrawWorld);
	if (!GRenderDevice->IsA(UD3D9RenderDevice::StaticClass())) {
#ifndef NDEBUG
		dout << "Not using D3D9DrvRTX Device! " << GRenderDevice->GetName() << std::endl;
#endif
		Super::DrawWorld(frame);
		return;
	}
#if UNREAL_TOURNAMENT && !UNREAL_TOURNAMENT_OLDUNREAL
	else if (FString(GRenderDevice->GetName()).InStr(TEXT("RenderDeviceProxy")) == 0) {
		appErrorf(TEXT("D3D9DrvRTX for UT v436 is not compatible with v469.\nInstall D3D9DrvRTX for v469 instead!"));
	}
#endif

	UD3D9RenderDevice* d3d9Dev = (UD3D9RenderDevice*)GRenderDevice;

	FMemMark memMark(GMem);
	//FMemMark sceneMark(GSceneMem);
	//FMemMark dynMark(GDynMem);
	FMemMark vectorMark(VectorMem);

	//dout << "Starting frame" << std::endl;

	if (Engine->Audio && !GIsEditor) {
		Engine->Audio->RenderAudioGeometry(frame);
	}

	const UViewport* viewport = frame->Viewport;
	AActor* playerActor = NULL;
	if (!viewport->Actor->bBehindView) {
		playerActor = viewport->Actor->ViewTarget ? viewport->Actor->ViewTarget : viewport->Actor;
	}

	FrameActors objs;
	objs.lights.reserve(frame->Level->Actors.Num());
	objs.actors.reserve(frame->Level->Actors.Num());
	std::vector<ASkyZoneInfo*> skyZones;

	// Sort through all actors and put them in the appropriate pile
	for (int iActor = 0; iActor < frame->Level->Actors.Num(); iActor++) {
		AActor* actor = frame->Level->Actors(iActor);
		if (!actor) continue;
		if (actor->LightType != LT_None) {
#if UTGLR_HAS_DARK_LIGHT
			if (!actor->bDarkLight)
#endif
			objs.lights.push_back(actor);
		}
		if (actor->IsA(ASkyZoneInfo::StaticClass())) {
			skyZones.push_back((ASkyZoneInfo*)actor);
		}
		bool isVisible = true;
		isVisible &= actor != playerActor;
		isVisible &= GIsEditor ? !actor->bHiddenEd : !actor->bHidden;
		bool isOwned = actor->IsOwnedBy(frame->Viewport->Actor);
		isVisible &= !actor->bOnlyOwnerSee || (isOwned && !frame->Viewport->Actor->bBehindView);
		isVisible &= !isOwned || !actor->bOwnerNoSee || (isOwned && frame->Viewport->Actor->bBehindView);
		if (isVisible) {
			if (actor->IsA(AMover::StaticClass())) {
				objs.movers.push_back((ABrush*)actor);
				continue;
			}
#if RUNE
			else if (actor->IsA(APolyobj::StaticClass()) && actor->DrawType == DT_Brush) {
				APolyobj* polyObj = static_cast<APolyobj*>(actor);
				if (polyObj->bCanRender) {
					objs.movers.push_back(polyObj);
				}
				continue;
			} else if (actor->IsA(ATrigger::StaticClass())) {
				// Wacky trigger logic, only try and render triggers if directly viewable by OccludeFrame
				continue;
			}
			if (actor->bCarriedItem) {
				continue;
			}
#endif
			if (getRenderInterfaceActors(actor, frame, objs.actors)) {
				continue;
			}
			if (actor->DrawType == DT_Brush) {
				continue;
			}
			objs.actors.push_back(actor);
		}
	}

#if RUNE
	// Allows frame->Sprite to be populated
	*prevFrameMaxZ = *currFrameMaxZ;
	*currFrameMaxZ = -1.0f;
#endif
	// Seems to also update mover bsp nodes for colision decal calculations
	OccludeFrame(frame);

	if (cachedLevelModel.currentLevel != frame->Level) {
		cachedLevelModel.facetsMemMark.Pop();
		cachedLevelModel.facets = ModelFacets();
		getLevelModelFacets(frame, cachedLevelModel.facets);
		cachedLevelModel.currentLevel = frame->Level;
	}
	ModelFacets& modelFacets = cachedLevelModel.facets;

	std::unordered_map<UTexture*, FTextureInfo> lockedTextures;

	ASkyZoneInfo* skyZone = frame->Level->GetZoneActor(frame->ZoneNumber)->SkyZone;
	if (d3d9Dev->EnableSkyBoxRendering && skyZone) {
		FSceneNode child = *frame;
		child.ZoneNumber = skyZone->Region.ZoneNumber;
		// Keep parent frame rotation and apply sky rotation, then location
		child.Coords.Origin = FVector();
		child.Coords /= skyZone->Rotation;
		child.Coords.Origin = skyZone->Location;
		child.Draw[0] = NULL;
		child.Draw[1] = NULL;
		child.Draw[2] = NULL;
		child.Sprite = NULL;
		d3d9Dev->startWorldDraw(&child);
		drawFrame(&child, d3d9Dev, modelFacets, objs, lockedTextures, true);
		//d3d9Dev->endWorldDraw(&child);
		d3d9Dev->ClearZ(frame);
	}

	d3d9Dev->startWorldDraw(frame);

	drawFrame(frame, d3d9Dev, modelFacets, objs, lockedTextures);

	for (ASkyZoneInfo* zone : skyZones) {
		d3d9Dev->renderSkyZoneAnchor(zone, &frame->Coords.Origin);
	}

	d3d9Dev->renderLights(frame, objs.lights);

	d3d9Dev->endWorldDraw(frame);

#if !UNREAL_GOLD_OLDUNREAL
	for (std::pair<UTexture* const, FTextureInfo>& entry : lockedTextures) {
		entry.first->Unlock(entry.second);
	}
#endif

	// Render view model actor and extra HUD stuff
	if (!GIsEditor && playerActor && (viewport->Actor->ShowFlags & SHOW_Actors)) {
		GUglyHackFlags |= 1;
		playerActor->eventRenderOverlays(viewport->Canvas);
		GUglyHackFlags &= ~1;
	}

	memMark.Pop();
	//dynMark.Pop();
	//sceneMark.Pop();
	vectorMark.Pop();
	unguard;
}

void UD3D9Render::drawFrame(FSceneNode* frame, UD3D9RenderDevice* d3d9Dev, ModelFacets& modelFacets, FrameActors& objs, std::unordered_map<UTexture*, FTextureInfo>& lockedTextures, bool isSky) {
	guard(UD3D9Render::drawFrame);
	// Add all actors in view and also any in zones that are visible
	std::unordered_set<AActor*> visibleActors;
	std::unordered_set<ABrush*> visibleMovers;
	std::bitset<64> visibleZoneBits;
	visibleZoneBits[frame->ZoneNumber] = true;
	for (int pass : {0, 1, 2}) {
		for (FBspDrawList* drawList = frame->Draw[pass]; drawList; drawList = drawList->Next) {
			if (frame->Level->BrushTracker && frame->Level->BrushTracker->SurfIsDynamic(drawList->iSurf)) {
				visibleMovers.insert(frame->Level->Model->Surfs(drawList->iSurf).Actor);
			}
			visibleZoneBits[drawList->iZone] = true;
		}
	}
	for (FDynamicSprite* sprite = frame->Sprite; sprite; sprite = sprite->RenderNext) {
		if (sprite->Actor->RenderIteratorClass) continue;
		if (!isSky) {
			visibleZoneBits[sprite->Actor->Region.ZoneNumber] = true;
		}
		visibleActors.insert(sprite->Actor);
	}
	for (AActor* actor : objs.actors) {
		if (actor->Region.ZoneNumber == 0) {
			frame->Level->SetActorZone(actor, 1, 0);
		}
		if (visibleZoneBits[actor->Region.ZoneNumber]) {
			visibleActors.insert(actor);
		}
	}
	// Add all connected zones to render adjacent level geo
	QWORD visibleZoneMask = visibleZoneBits.to_ullong();
	while (visibleZoneMask) {
		DWORD zone = std::countr_zero(visibleZoneMask);
		visibleZoneBits |= frame->Level->Model->Zones[zone].Connectivity;
		visibleZoneMask &= (visibleZoneMask - 1);
	}

	for (ABrush* mover : objs.movers) {
		if (visibleZoneBits[mover->Region.ZoneNumber]) {
			visibleMovers.insert(mover);
		}
	}

	// Add the zones as a set to easily iterate on it
	std::unordered_set<INT> visibleZones;
	visibleZoneMask = visibleZoneBits.to_ullong();
	while (visibleZoneMask) {
		DWORD zone = std::countr_zero(visibleZoneMask);
		visibleZones.insert(zone);
		visibleZoneMask &= (visibleZoneMask - 1);
	}

	for (RPASS pass : {SOLID, NONSOLID}) {
		DecalMap decalMap;
		for (int zone : visibleZones) {
			for (auto& facetPair : modelFacets.facetPairs[zone][pass]) {
				UTexture* texture = facetPair.tex;
				DWORD flags = facetPair.flags;
				std::vector<SurfaceData>& surfaces = facetPair.bucket;

				FTextureInfo* texInfo;
				if (!lockedTextures.count(texture)) {
					texInfo = &lockedTextures[texture];
#if UNREAL_GOLD_OLDUNREAL
					*texInfo = *texture->GetTexture(-1, d3d9Dev);
#else
					texture->Lock(*texInfo, frame->Viewport->CurrentTime, -1, d3d9Dev);
#endif
				}
				else {
					texInfo = &lockedTextures[texture];
				}

				FSurfaceInfo surfaceInfo{};
				surfaceInfo.Level = frame->Level;
				surfaceInfo.PolyFlags = flags;
				surfaceInfo.Texture = texInfo;

				std::vector<FSurfaceFacet*> facets;
				facets.reserve(surfaces.size());
				for (const SurfaceData& surface : surfaces) {
					facets.push_back(surface.facet);
				}

				d3d9Dev->drawLevelSurfaces(frame, surfaceInfo, facets);
#if !UTGLR_NO_DECALS
				if (frame->Viewport->GetOuterUClient()->Decals) {
					for (const SurfaceData& surface : surfaces) {
						getSurfaceDecals(frame, surface, decalMap, lockedTextures);
					}
				}
#endif
			}
		}
		// Render all the decals
		for (const auto& decalsPair : decalMap) {
			FTextureInfo* const& texInfo = decalsPair.tex;
			for (std::vector<FTransTexture> decal : decalsPair.bucket) {
				int numPts = decal.size();
				FTransTexture** pointsPtrs = new FTransTexture * [numPts];
				for (int i = 0; i < numPts; i++) {
					pointsPtrs[i] = &decal[i];
				}
				d3d9Dev->setIdentityMatrix();
				d3d9Dev->DrawGouraudPolygon(frame, *texInfo, pointsPtrs, numPts, decalsPair.flags, NULL);
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
#if RUNE
			bTranslucent |= actor->Style == STY_AlphaBlend;
#endif
			if ((pass == RPASS::NONSOLID && bTranslucent) || (pass == RPASS::SOLID && !bTranslucent)) {
				RenderList renderList;
				drawActorSwitch(frame, d3d9Dev, actor, renderList);
				for (const ActorRenderData& renderData : renderList) {
					d3d9Dev->renderSurfaceBuckets(renderData, frame->Viewport->CurrentTime);
				}
			}
		}
	}
	unguardf((TEXT("(isSky = %i)"), isSky));
}

void UD3D9Render::drawActorSwitch(FSceneNode* frame, UD3D9RenderDevice* d3d9Dev, AActor* actor, RenderList& renderList, ParentCoord* parentCoord) {
	guard(UD3D9Render::drawActorSwitch);
	SpecialCoord specialCoord{};
#if RUNE
	ATrigger* trigger = Cast<ATrigger>(actor);
	if (trigger && trigger->TriggerType == TT_Sight) {
		// "Draw" triggers because for some unholy reason trigger logic is in the render module
		FDynamicSprite sprite(actor);
		d3d9Dev->setCompatMatrix(frame);
		DrawActorSprite(frame, &sprite, GMath.UnitCoords);
		return;
	}
	actor->bRenderedLastFrame = true;
	if (actor->bSpecialRender) {
		return;
	}
	if (actor->DrawType == DT_SkeletalMesh) {
		drawSkeletalActor(frame, d3d9Dev, actor, renderList, parentCoord);
	}
	else if (actor->DrawType == DT_ParticleSystem && actor->IsA(AParticleSystem::StaticClass())) {
		AParticleSystem* particle = static_cast<AParticleSystem*>(actor);
		if (particle->ParticleSpriteType == PSPRITE_Normal) {
			d3d9Dev->renderParticleSystemActor(frame, (AParticleSystem*)actor, parentCoord ? parentCoord->localCoord : GMath.UnitCoords);
		}
		else {
			d3d9Dev->setCompatMatrix(frame);
			DrawParticleSystem(frame, actor, nullptr, parentCoord ? parentCoord->localCoord : GMath.UnitCoords);
		}
	}
	else
#endif
#if UTGLR_HP_ENGINE
	if (actor->DrawType == DT_Particles) {
		d3d9Dev->setCompatMatrix(frame);
		FDynamicSprite sprite(actor);
		DrawParticleSystem(frame, &sprite);
	}
	else
#endif
#if UNREAL_GOLD_OLDUNREAL
	if (actor->RealBasedActors) {
		if (actor->DrawType == DT_Mesh && actor->Mesh && actor->MeshInstance) {
			actor->MeshInstance->UpdateAttachments(false);
		}
		for (INT i = 0; i < actor->RealBasedActors->Num(); i++) {
			AActor* const& childActor = (*actor->RealBasedActors)(i);
			// block actors which should have already been iterated
			if (childActor->RenderIteratorClass) continue;

			if (childActor->bOnlyDrawWithBase || (!childActor->bHardAttach && childActor->AttachedBone.GetIndex() != 0)) {
				if (childActor->bHidden && !(GUglyHackFlags & HACKFLAGS_DrawHiddenActor)) continue;
				if (childActor->bHardAttach && childActor->AttachedBone.GetIndex() == 0) {
					FCheckResult checkResult{};
					childActor->XLevel->MoveActor(childActor, FVector(), FRotator(), checkResult);
				}
				drawActorSwitch(frame, d3d9Dev, childActor, renderList);
			}
		}
	}

	d3d9Dev->setCompatMatrix(frame);
	// Terrain actor vftable can be nullptr so check first!
	if (*((void**)actor) && actor->OverrideMeshRender(frame)) {}
	else
#endif
	if ((actor->DrawType == DT_Sprite || actor->DrawType == DT_SpriteAnimOnce || (frame->Viewport->Actor->ShowFlags & SHOW_ActorIcons)) && actor->Texture) {
		d3d9Dev->renderSprite(frame, actor);
	}
	else if (actor->DrawType == DT_Mesh) {
		d3d9Dev->renderMeshActor(frame, actor ,renderList, &specialCoord);
	}
	if (actor->IsA(APawn::StaticClass())) {
		drawPawnExtras(frame, d3d9Dev, (APawn*)actor, renderList, specialCoord);
	}
	unguardf((TEXT("(%ls)"), actor->GetFullName()));
}

#if !UTGLR_NO_DECALS
void UD3D9Render::getSurfaceDecals(FSceneNode* frame, const SurfaceData& surfaceData, DecalMap& decals, std::unordered_map<UTexture*, FTextureInfo>& lockedTextures) {
	const UViewport* viewport = frame->Viewport;
	const UModel* model = frame->Level->Model;
	const FBspSurf& surf = model->Surfs(surfaceData.iSurf);
	for (int i = 0; i < surf.Decals.Num(); i++) {
		const FDecal* decal = &surf.Decals(i);
		UTexture* texture;
		if (decal->Actor->Texture) {
#if UNREAL_GOLD_OLDUNREAL
			texture = decal->Actor->Texture->Get();
#else
			texture = decal->Actor->Texture->Get(viewport->CurrentTime);
#endif
		} else {
			texture = viewport->Actor->Level->DefaultTexture;
		}
		FTextureInfo* texInfo;
		if (!lockedTextures.count(texture)) {
			texInfo = &lockedTextures[texture];
#if UNREAL_GOLD_OLDUNREAL
			*texInfo = *texture->GetTexture(-1, viewport->RenDev);
#else
			texture->Lock(*texInfo, viewport->CurrentTime, -1, viewport->RenDev);
#endif
		} else {
			texInfo = &lockedTextures[texture];
		}

		DWORD polyFlags = PF_Modulated;
#ifdef RUNE
		switch (decal->Actor->Style) {
		case STY_Masked:
			polyFlags = PF_Masked; break;
		case STY_Translucent:
			polyFlags = PF_Translucent; break;
		case STY_Modulated:
			polyFlags = PF_Modulated; break;
		case STY_AlphaBlend:
			polyFlags = PF_AlphaBlend; break;
			texture->Alpha = decal->Actor->AlphaScale;
		default:
			break;
		}
#endif // RUNE

		std::vector<std::vector<FTransTexture>>& decalPoints = decals.get(texInfo, polyFlags);

		for (FSavedPoly* poly = surfaceData.facet->Polys; poly; poly = poly->Next) {
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
	decalPts.reserve(6);
	for (int i = 0; i < 4; i++) {
		// Add the 4 decal points to start
		FTransTexture& pt = decalPts.emplace_back();
		pt.Point = decal->Vertices[i] + decalOffset;
	}

	std::vector<bool> isInside;
	isInside.reserve(6);
	int polyPrevIdx = poly->NumPts - 1;
	for (int polyIdx = 0; polyIdx < poly->NumPts; polyIdx++) {
		FVector edgeVector = poly->Pts[polyPrevIdx]->Point - poly->Pts[polyIdx]->Point;
		FVector clipNorm = edgeVector ^ surfNormal;
		FPlane clipPlane = FPlane(poly->Pts[polyIdx]->Point, clipNorm);

		isInside.clear();
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

void UD3D9Render::drawPawnExtras(FSceneNode* frame, UD3D9RenderDevice* d3d9Dev, APawn* pawn, RenderList& renderList, SpecialCoord& specialCoord) {
#if !RUNE
#if HARRY_POTTER_2
	if (specialCoord.exists) {
		FCoords wsCoords = specialCoord.worldCoord;
		wsCoords.YAxis = -wsCoords.YAxis;
		pawn->WeaponLoc = wsCoords.Origin;
		pawn->WeaponRot = wsCoords.OrthoRotation();
	}
#endif
	AInventory* weapon = pawn->Weapon;
	if (specialCoord.exists && weapon && weapon->ThirdPersonMesh) {
		specialCoord.enabled = true;
		UMesh* origMesh = weapon->Mesh;
		FLOAT origDrawScale = weapon->DrawScale;
		weapon->Mesh = weapon->ThirdPersonMesh;
		weapon->DrawScale = weapon->ThirdPersonScale;
		d3d9Dev->renderMeshActor(frame, weapon, renderList, &specialCoord);
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
			d3d9Dev->renderMeshActor(frame, weapon, renderList, &specialCoord);
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
#endif // !RUNE
#if !UTGLR_NO_PLAYER_FLAG
	if (pawn->PlayerReplicationInfo && pawn->PlayerReplicationInfo->HasFlag) {
		AActor* flag = pawn->PlayerReplicationInfo->HasFlag;

		FVector origLoc = flag->Location;
		FRotator origRot = flag->Rotation;
		float dist = Clamp(2.0f + 20.0f * GMath.SinTab(flag->Rotation.Pitch), 2.0f, 3.0f);
		flag->Location = pawn->Location - dist * pawn->CollisionRadius * pawn->Rotation.Vector() + FVector(0, 0, 0.7 * pawn->BaseEyeHeight);
		flag->Rotation = pawn->Rotation;
		d3d9Dev->renderMeshActor(frame, flag, renderList);
		flag->Location = origLoc;
		flag->Rotation = origRot;
	}
#endif
}

#if RUNE
void UD3D9Render::drawSkeletalActor(FSceneNode* frame, UD3D9RenderDevice* d3d9Dev, AActor* actor, RenderList& renderList, const ParentCoord* parentCoord) {
	d3d9Dev->renderSkeletalMeshActor(frame, actor, renderList, parentCoord ? &parentCoord->worldCoord : nullptr);
	USkelModel* skel = actor->Skeletal;
	if (!skel) return;
	skel->GetFrame(actor, parentCoord ? parentCoord->localCoord : GMath.UnitCoords, 0, nullptr); // Updates the skel position with the real actor coords
	for (int i = 0; i < skel->numjoints; i++) {
		AActor* child = actor->JointChild[i];
		if (!child || child->bHidden) continue;

		ParentCoord childCoords;
		FCacheItem* cacheItem = nullptr;
		DynSkel* dynSkel = skel->LockDSkel(actor, cacheItem);
		DynJoint* dynJoint = &dynSkel->joint[i];
		childCoords.worldCoord = dynJoint->coords;
		skel->UnlockDSkel(cacheItem);

		FCoords coords = childCoords.worldCoord / child->Rotation;
		if (child->DrawType == DT_Sprite || child->IsA(AParticleSystem::StaticClass())) {
			FVector particleLoc(0, 0, 0);
			particleLoc = particleLoc.TransformPointBy(coords);
			child->GetLevel()->FarMoveActor(child, particleLoc, 1, 1);
		}
		childCoords.localCoord = coords / child->Location;
		drawActorSwitch(frame, d3d9Dev, child, renderList, &childCoords);
	}
}

// Just the offsets from the closest exported symbols
float* const UD3D9Render::prevFrameMaxZ = ((float*)&UD3D9Render::Stamp) - 196;
float* const UD3D9Render::currFrameMaxZ = ((float*)&UD3D9Render::MaxLeafLights) - 1;
#endif

#if UNREAL_GOLD_OLDUNREAL
void UD3D9Render::DrawActor(FSceneNode* frame, AActor* actor, FDynamicSprite* Sprite) {
#else
void UD3D9Render::DrawActor(FSceneNode* frame, AActor* actor) {
#endif
	guard(UD3D9Render::DrawActor);
	if (!GRenderDevice->IsA(UD3D9RenderDevice::StaticClass())) {
		Super::DrawActor(frame, actor);
		return;
	}
	// dout << "Drawing actor! " << actor->GetName() << std::endl;
	UD3D9RenderDevice* d3d9Dev = (UD3D9RenderDevice*)GRenderDevice;
	// TODO: fix this muzzle flash schtuff
	d3d9Dev->executeBufferedTileDraws();
	d3d9Dev->startWorldDraw(frame);
	RenderList renderList;
	drawActorSwitch(frame, d3d9Dev, actor, renderList);
	for (const ActorRenderData& renderData : renderList) {
		d3d9Dev->renderSurfaceBuckets(renderData, frame->Viewport->CurrentTime);
	}
	d3d9Dev->endWorldDraw(frame);
	unguard;
}
