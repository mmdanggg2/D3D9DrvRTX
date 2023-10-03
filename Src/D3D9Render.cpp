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
		FStaticBspInfoBase staticBsp = FStaticBspInfoBase(frame->Level);
		((UD3D9RenderDevice*)GRenderDevice)->SetStaticBsp(staticBsp);
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
