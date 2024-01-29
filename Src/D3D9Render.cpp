#include "D3D9Render.h"
#include "D3D9RenderDevice.h"

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
	const UModel* model = frame->Level->Model;
	const FLOAT levelTime = frame->Level->GetLevelInfo()->TimeSeconds;
	const UViewport* viewport = frame->Viewport;

	TexFlagKeyMap<std::vector<INT>> texNodes;

	DWORD flagMask = (viewport->Actor->ShowFlags & SHOW_PlayerCtrl) ? ~PF_FlatShaded : ~PF_Invisible;
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

		texNodes[TexFlagKey(texture, flags)].push_back(iNode);
	}

	for (const std::pair<const TexFlagKey, std::vector<INT>>& texNodePair : texNodes) {
		DWORD flags = texNodePair.first.second;

		std::unordered_map<INT, FSurfaceFacet> surfaceMap;
		for (INT iNode : texNodePair.second) {
			const FBspNode& node = model->Nodes(iNode);
			const FBspSurf* surf = &model->Surfs(node.iSurf);
			FSurfaceFacet* facet;
			if (surfaceMap.find(node.iSurf) == surfaceMap.end()) {
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

		for (const std::pair<const INT, FSurfaceFacet>& facetPair : surfaceMap) {
			modelFacets.facetPairs[(flags & PF_NoOcclude) != 0][texNodePair.first].push_back(facetPair.second);
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
		OccludeFrame(frame);

		ModelFacets modelFacets;
		getLevelModelFacets(frame, modelFacets);

		std::unordered_map<UTexture*, FTextureInfo> lockedTextures;

		for (int pass : {1, 2}) {
			for (std::pair<const TexFlagKey, std::vector<FSurfaceFacet>>& facetPair : modelFacets.facetPairs[pass-1]) {
				UTexture* texture = facetPair.first.first;
				DWORD flags = facetPair.first.second;
				std::vector<FSurfaceFacet>& facets = facetPair.second;

				FTextureInfo* texInfo;
				if (lockedTextures.find(texture) == lockedTextures.end()) {
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
			}
			//for (FDynamicSprite* sprite = frame->Sprite; sprite; sprite = sprite->RenderNext) {
			//	UBOOL bTranslucent = sprite->Actor && sprite->Actor->Style == STY_Translucent;
			//	if ((pass == 2 && bTranslucent) || (pass == 1 && !bTranslucent)) {
			//		AActor* actor = sprite->Actor;
			//		if ((actor->DrawType == DT_Sprite || actor->DrawType == DT_SpriteAnimOnce || (viewport->Actor->ShowFlags & SHOW_ActorIcons)) && actor->Texture) {
			//			d3d9Dev->renderSprite(frame, actor);
			//		} else if (actor->DrawType == DT_Mesh) {
			//			d3d9Dev->renderMeshActor(frame, actor);
			//		}
			//	}
			//}
			for (int iActor = 0; iActor < frame->Level->Actors.Num(); iActor++) {
				AActor* actor = frame->Level->Actors(iActor);
				if (!actor) continue;
				bool isVisible = true;
				isVisible &= actor != playerActor;
				isVisible &= GIsEditor ? !actor->bHiddenEd : !actor->bHidden;
				bool isOwned = actor->IsOwnedBy(frame->Viewport->Actor);
				isVisible &= !actor->bOnlyOwnerSee || (isOwned && !frame->Viewport->Actor->bBehindView);
				isVisible &= !isOwned || !actor->bOwnerNoSee || (isOwned && frame->Viewport->Actor->bBehindView);
				if (isVisible) {
					if (actor->IsA(AMover::StaticClass()) && pass == 1) {
						d3d9Dev->renderMover(frame, (AMover*)actor);
						continue;
					}
					SpecialCoord specialCoord;
					UBOOL bTranslucent = actor && actor->Style == STY_Translucent;
					if ((pass == 2 && bTranslucent) || (pass == 1 && !bTranslucent)) {
						if ((actor->DrawType == DT_Sprite || actor->DrawType == DT_SpriteAnimOnce || (viewport->Actor->ShowFlags & SHOW_ActorIcons)) && actor->Texture) {
							d3d9Dev->renderSprite(frame, actor);
						} else if (actor->DrawType == DT_Mesh) {
							d3d9Dev->renderMeshActor(frame, actor, &specialCoord);
						}
					}
					if (actor->IsA(APawn::StaticClass())) {
						APawn* pawn = (APawn*)actor;
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
				}
			}
		}
		for (std::pair<UTexture* const, FTextureInfo>& entry : lockedTextures) {
			entry.first->Unlock(entry.second);
		}

		// Render view model actor and extra HUD stuff
		if (!GIsEditor && playerActor && (frame->Viewport->Actor->ShowFlags & SHOW_Actors)) {
			GUglyHackFlags |= 1;
			playerActor->eventRenderOverlays(frame->Viewport->Canvas);
			GUglyHackFlags &= ~1;
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
		memMark.Pop();
		dynMark.Pop();
		sceneMark.Pop();
		vectorMark.Pop();
		return;
	}
	Super::DrawWorld(frame);
	unguard;
}


void UD3D9Render::DrawActor(FSceneNode* frame, AActor* actor) {
	guard(UD3D9Render::DrawActor);
	if (GRenderDevice->IsA(UD3D9RenderDevice::StaticClass())) {
		UD3D9RenderDevice* d3d9Dev = (UD3D9RenderDevice*)GRenderDevice;
		d3d9Dev->renderMeshActor(frame, actor);
		//dout << "Drawing actor! " << actor->GetName() << std::endl;
		return;
	}
	Super::DrawActor(frame, actor);
	unguard;
}
