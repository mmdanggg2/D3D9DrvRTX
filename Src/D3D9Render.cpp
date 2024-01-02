#include "D3D9Render.h"
#include "D3D9RenderDevice.h"
#include <unordered_set>

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
		//d3d9Dev->SetStaticBsp(staticBsp);

		//std::unordered_set<int> nodes;

		for (int pass : { 1, 2 }) {
			for (FBspDrawList* draw = frame->Draw[pass]; draw; draw = draw->Next) {
				FBspSurf* surf = &frame->Level->Model->Surfs(draw->iSurf);

				UTexture* texture = surf->Texture ? surf->Texture->Get(viewport->CurrentTime) : viewport->Actor->Level->DefaultTexture;

				FLOAT levelTime = frame->Level->GetLevelInfo()->TimeSeconds;
				FLOAT panU = surf->PanU;
				FLOAT panV = surf->PanV;
				if (surf->PolyFlags & PF_AutoUPan) {
					panU += ((INT)(levelTime * 35.f * draw->Zone->TexUPanSpeed * 256.0) & 0x3ffff) / 256.0;
				}
				if (surf->PolyFlags & PF_AutoVPan) {
					panV += ((INT)(levelTime * 35.f * draw->Zone->TexVPanSpeed * 256.0) & 0x3ffff) / 256.0;
				}
				if (surf->PolyFlags & PF_SmallWavy) {
					panU += 8.0 * appSin(levelTime) + 4.0 * appCos(2.3 * levelTime);
					panV += 8.0 * appCos(levelTime) + 4.0 * appSin(2.3 * levelTime);
				}

				FSurfaceInfo surface{};
				surface.Level = frame->Level;
				surface.PolyFlags = draw->PolyFlags;

				FTextureInfo textureMap{};
				texture->Lock(textureMap, viewport->CurrentTime, -1, viewport->RenDev);
				textureMap.Pan = FVector(-panU, -panV, 0);
				surface.Texture = &textureMap;

				FSavedPoly* polyHead = NULL;

				for (int i = 0; i < surf->Nodes.Num(); i++) {
					FBspNode* node = &frame->Level->Model->Nodes(surf->Nodes(i));
					FSavedPoly* poly = (FSavedPoly*)New<BYTE>(GDynMem, sizeof(FSavedPoly) + node->NumVertices * sizeof(FTransform*));
					poly->Next = polyHead;
					polyHead = poly;
					poly->iNode = draw->iNode;
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
				facet.Span = &draw->Span;
				facet.MapCoords = FCoords
				(
					model->Points(surf->pBase),
					model->Vectors(surf->vTextureU),
					model->Vectors(surf->vTextureV),
					model->Vectors(surf->vNormal)
				);

				d3d9Dev->DrawComplexSurface(frame, surface, facet);
			}
		}

		for (int pass : {1, 2}) {
			for (FDynamicSprite* sprite = frame->Sprite; sprite; sprite = sprite->RenderNext) {
				UBOOL bTranslucent = sprite->Actor && sprite->Actor->Style == STY_Translucent;
				if ((pass == 2 && bTranslucent) || (pass == 1 && !bTranslucent)) {
					//DrawActorSprite(frame, sprite);
					d3d9Dev->renderActor(frame, sprite->Actor);
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

		d3d9Dev->renderLights(frame);

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
		d3d9Dev->renderActor(frame, actor);
		//dout << "Drawing actor! " << actor->GetName() << std::endl;
		return;
	}
	Super::DrawActor(frame, actor);
	unguard;
}
