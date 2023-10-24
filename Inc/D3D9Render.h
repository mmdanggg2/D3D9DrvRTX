#pragma once

#include <Render.h>
#include "D3D9DebugUtils.h"

class UD3D9Render : public URender {
	DECLARE_CLASS(UD3D9Render, URender, CLASS_Config, D3D9DrvRTX);

	ods_stream dout;

	// UObject interface.
	UD3D9Render();
	void StaticConstructor();
	
	// URenderBase interface
	void DrawWorld(FSceneNode* Frame) override;
	void DrawActor(FSceneNode* Frame, AActor* Actor) override;

};
