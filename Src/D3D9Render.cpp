#include "D3D9Render.h"
#include "D3D9RenderDevice.h"
#include <unordered_map>

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


void UD3D9Render::DrawWorld(FSceneNode* frame) {
	guard(UD3D9Render::DrawWorld);
	if (GRenderDevice->IsA(UD3D9RenderDevice::StaticClass())) {
		FMemMark sceneMark(GSceneMem);
		FMemMark memMark(GMem);
		FMemMark dynMark(GDynMem);
		FMemMark vectorMark(VectorMem);
		UD3D9RenderDevice* d3d9Dev = (UD3D9RenderDevice*)GRenderDevice;

		const UViewport* viewport = frame->Viewport;
		const UModel* model = frame->Level->Model;
		FLOAT levelTime = frame->Level->GetLevelInfo()->TimeSeconds;

		d3d9Dev->startWorldDraw(frame);
		OccludeFrame(frame);
		d3d9Dev->currentFrame = frame;

		SurfKeyMap<std::vector<INT>> surfacePasses[2];
		surfacePasses[0].reserve(model->Surfs.Num());
		surfacePasses[1].reserve(model->Surfs.Num());
		std::unordered_map<UTexture*, FTextureInfo> lockedTextures;

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

			FTextureInfo* texInfo;
			if (lockedTextures.find(texture) == lockedTextures.end()) {
				texInfo = &lockedTextures[texture];
				texture->Lock(*texInfo, viewport->CurrentTime, -1, viewport->RenDev);
			} else {
				texInfo = &lockedTextures[texture];
			}

			surfacePasses[(flags & PF_NoOcclude) != 0][SurfKey(texInfo, flags)].push_back(iNode);
		}

		for (int pass : { 0, 1 }) {
			for (const std::pair<const SurfKey, std::vector<INT>>& texNodePair : surfacePasses[pass]) {
				FTextureInfo* texInfo = texNodePair.first.first;
				DWORD flags = texNodePair.first.second;

				FSurfaceInfo surface{};
				surface.Level = frame->Level;
				surface.PolyFlags = flags;
				surface.Texture = texInfo;

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
						FVector pan = FVector(-panU, -panV, 0);
						if (texInfo->Pan != pan) {
							FTextureInfo* newTexInfo = New<FTextureInfo>(GDynMem);
							*newTexInfo = *texInfo;
							newTexInfo->Pan = pan;
							// Hide this away in the span coz we're not using it
							facet->Span = (FSpanBuffer*)newTexInfo;
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
				std::vector<FSurfaceFacet> facets;
				for (const std::pair<const INT, FSurfaceFacet>& facetPair : surfaceMap) {
					facets.push_back(facetPair.second);
				}

				d3d9Dev->drawLevelSurfaces(frame, surface, facets);

			}
		}
		for (std::pair<UTexture* const, FTextureInfo>& entry : lockedTextures) {
			entry.first->Unlock(entry.second);
		}

		for (int pass : {1, 2}) {
			for (FDynamicSprite* sprite = frame->Sprite; sprite; sprite = sprite->RenderNext) {
				UBOOL bTranslucent = sprite->Actor && sprite->Actor->Style == STY_Translucent;
				if ((pass == 2 && bTranslucent) || (pass == 1 && !bTranslucent)) {
					AActor* actor = sprite->Actor;
					if ((actor->DrawType == DT_Sprite || actor->DrawType == DT_SpriteAnimOnce || (viewport->Actor->ShowFlags & SHOW_ActorIcons)) && actor->Texture) {
						d3d9Dev->renderSprite(frame, actor);
					} else if (actor->DrawType == DT_Mesh) {
						d3d9Dev->renderMeshActor(frame, actor);
					}
				}
			}
			for (int iActor = 0; iActor < frame->Level->Actors.Num(); iActor++) {
				AActor* actor = frame->Level->Actors(iActor);
				if (actor && actor != viewport->Actor) {
					if (actor->IsA(AMover::StaticClass()) && pass == 1) {
						d3d9Dev->renderMover(frame, (AMover*)actor);
						continue;
					}
					//UBOOL bTranslucent = actor && actor->Style == STY_Translucent;
					//if ((pass == 2 && bTranslucent) || (pass == 1 && !bTranslucent)) {
					//	//DrawActorSprite(frame, sprite);
					//	d3d9Dev->renderMeshActor(frame, actor);
					//}
				}
			}
		}

		// Render view model actor and extra HUD stuff
		AActor* viewModelActor = frame->Viewport->Actor->bBehindView ? NULL : 
			frame->Viewport->Actor->ViewTarget ? frame->Viewport->Actor->ViewTarget : frame->Viewport->Actor;
		if (!GIsEditor && viewModelActor && (frame->Viewport->Actor->ShowFlags & SHOW_Actors)) {
			GUglyHackFlags |= 1;
			viewModelActor->eventRenderOverlays(frame->Viewport->Canvas);
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
