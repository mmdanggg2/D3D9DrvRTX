#include "D3D9Render.h"
#include "D3D9RenderDevice.h"

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
		FMemMark SceneMark(GSceneMem);
		FMemMark MemMark(GMem);
		FMemMark DynMark(GDynMem);
		FMemMark VectorMark(VectorMem);
		UD3D9RenderDevice* d3d9Dev = (UD3D9RenderDevice*)GRenderDevice;
		d3d9Dev->startWorldDraw(frame);
		OccludeFrame(frame);
		//OccludeBsp(frame);
		FStaticBspInfoBase staticBsp = FStaticBspInfoBase(frame->Level);
		d3d9Dev->currentFrame = frame;
		d3d9Dev->SetStaticBsp(staticBsp);

		for (FDynamicSprite* sprite = frame->Sprite; sprite; sprite = sprite->RenderNext) {
			d3d9Dev->renderActor(frame, sprite->Actor);
		}
		d3d9Dev->endWorldDraw(frame);
		MemMark.Pop();
		DynMark.Pop();
		SceneMark.Pop();
		VectorMark.Pop();
		return;
	}
	Super::DrawWorld(frame);
	unguard;
}


void UD3D9Render::DrawActor(FSceneNode* frame, AActor* actor) {
	guard(UD3D9Render::DrawActor);
	dout << "Drawing actor! " << actor->GetName() << std::endl;
	Super::DrawActor(frame, actor);
	unguard;
}
