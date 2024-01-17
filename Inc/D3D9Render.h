#pragma once

#include <Render.h>
#include "D3D9DebugUtils.h"

#include <vector>
#include <unordered_map>

typedef const std::pair<UTexture* const, const DWORD> TexFlagKey;
struct TexFlagKey_Hash {
	std::size_t operator () (const TexFlagKey& p) const {
		auto ptr_hash = std::hash<UTexture*>{}(p.first);
		auto dword_hash = std::hash<DWORD>{}(p.second);

		return ptr_hash ^ (dword_hash << 1);  // Shift dword_hash to ensure the upper bits are also involved in the final hash
	}
};
template <typename T>
using TexFlagKeyMap = std::unordered_map<TexFlagKey, T, TexFlagKey_Hash>;

class UD3D9Render : public URender {
	DECLARE_CLASS(UD3D9Render, URender, CLASS_Config, D3D9DrvRTX);

	ods_stream dout;

	// UObject interface.
	UD3D9Render();
	void StaticConstructor();
	
	// URenderBase interface
	void DrawWorld(FSceneNode* Frame) override;
	void DrawActor(FSceneNode* Frame, AActor* Actor) override;


private:
	struct ModelFacets {
		TexFlagKeyMap<std::vector<FSurfaceFacet>> facetPairs[2];
	};
	void getLevelModelFacets(FSceneNode* frame, ModelFacets& modelFacets);
};
