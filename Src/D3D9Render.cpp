#include "D3D9Render.h"
#include "D3D9RenderDevice.h"
#include <unordered_map>

IMPLEMENT_CLASS(UD3D9Render);

UD3D9Render::UD3D9Render():URender() {
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

		UViewport* viewport = frame->Viewport;
		UModel* model = frame->Level->Model;

		d3d9Dev->startWorldDraw(frame);
		OccludeFrame(frame);
		d3d9Dev->currentFrame = frame;

		std::unordered_map<int, std::vector<int>> surfacePasses[2];
		surfacePasses[0].reserve(model->Surfs.Num());
		surfacePasses[1].reserve(model->Surfs.Num());

		DWORD flagMask = (viewport->Actor->ShowFlags & SHOW_PlayerCtrl) ? ~0 : ~PF_Invisible;
		for (int iNode = 0; iNode < model->Nodes.Num(); iNode++) {
			FBspNode* node = &model->Nodes(iNode);
			int iSurf = node->iSurf;
			FBspSurf* surf = &model->Surfs(iSurf);
			DWORD flags = surf->PolyFlags;
			if (surf->Texture) {
				flags |= surf->Texture->PolyFlags;
			}
			flags |= viewport->ExtraPolyFlags;
			flags &= flagMask;
			surfacePasses[flags & PF_NoOcclude != 0][iSurf].push_back(iNode);
		}

		for (int pass : { 0, 1 }) {
			for (const std::pair<const int, std::vector<int>>& surfPair : surfacePasses[pass]) {
				int iSurf = surfPair.first;
				const std::vector<int>& nodes = surfPair.second;
				FBspSurf* surf = &model->Surfs(iSurf);
				if (surf->Nodes.Num() == 0) { // Must be a mover, skip it!
					//dout << L"Surf " << iSurf << L" has no nodes!" << std::endl;
					continue;
				}

				UTexture* texture = surf->Texture ? surf->Texture->Get(viewport->CurrentTime) : viewport->Actor->Level->DefaultTexture;

				DWORD flags = surf->PolyFlags;
				flags |= texture->PolyFlags;
				flags |= viewport->ExtraPolyFlags;
				flags &= flagMask;

				FLOAT levelTime = frame->Level->GetLevelInfo()->TimeSeconds;
				FLOAT panU = surf->PanU;
				FLOAT panV = surf->PanV;
				if (flags & PF_AutoUPan || flags & PF_AutoVPan) {
					AZoneInfo* zone = nullptr;
					for (int iNode : nodes) {
						FBspNode* node = &model->Nodes(iNode);
						FZoneProperties& zoneProps = model->Zones[node->iZone[0]];
						if (zoneProps.ZoneActor) {
							zone = zoneProps.ZoneActor;
							break;
						}
						zoneProps = model->Zones[node->iZone[1]];
						if (zoneProps.ZoneActor) {
							zone = zoneProps.ZoneActor;
							break;
						}

					}
					if (flags & PF_AutoUPan) {
						panU += ((INT)(levelTime * 35.f * (zone ? zone->TexUPanSpeed : 1.0f) * 256.0) & 0x3ffff) / 256.0;
					}
					if (flags & PF_AutoVPan) {
						panV += ((INT)(levelTime * 35.f * (zone ? zone->TexVPanSpeed : 1.0f) * 256.0) & 0x3ffff) / 256.0;
					}
				}
				if (flags & PF_SmallWavy) {
					panU += 8.0 * appSin(levelTime) + 4.0 * appCos(2.3 * levelTime);
					panV += 8.0 * appCos(levelTime) + 4.0 * appSin(2.3 * levelTime);
				}

				FSurfaceInfo surface{};
				surface.Level = frame->Level;
				surface.PolyFlags = flags;

				FTextureInfo textureMap{};
				texture->Lock(textureMap, viewport->CurrentTime, -1, viewport->RenDev);
				textureMap.Pan = FVector(-panU, -panV, 0);
				surface.Texture = &textureMap;

				FSavedPoly* polyHead = NULL;
				//dout << L"Surf " << iSurf << std::endl;
				for (int iNode : nodes) {
					//dout << L"\t Node " << iNode << std::endl;
					FBspNode* node = &model->Nodes(iNode);
					FSavedPoly* poly = (FSavedPoly*)New<BYTE>(GDynMem, sizeof(FSavedPoly) + node->NumVertices * sizeof(FTransform*));
					poly->Next = polyHead;
					polyHead = poly;
					poly->iNode = iNode;
					poly->NumPts = node->NumVertices;

					for (int j = 0; j < poly->NumPts; j++) {
						FVert vert = model->Verts(node->iVertPool + j);
						FTransform* trans = new(VectorMem)FTransform;
						trans->Point = model->Points(vert.pVertex);
						poly->Pts[j] = trans;
					}
				}

				FSurfaceFacet facet{};
				facet.Polys = polyHead;
				facet.MapCoords = FCoords(
					model->Points(surf->pBase),
					model->Vectors(surf->vTextureU),
					model->Vectors(surf->vTextureV),
					model->Vectors(surf->vNormal)
				);

				d3d9Dev->DrawComplexSurface(frame, surface, facet);

				texture->Unlock(textureMap);
			}
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
