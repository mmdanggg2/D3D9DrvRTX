#pragma once

#include "D3D9DebugUtils.h"
#include "D3D9RenderDevice.h"

#include <Render.h>

#include <vector>
#include <unordered_map>

typedef const std::pair<UTexture* const, const DWORD> TexFlagKey;
struct TexFlagKey_Hash {
	inline std::size_t operator () (const TexFlagKey& p) const {
		uint64_t combined = (reinterpret_cast<uint64_t>(p.first) << 32) | p.second;
		return std::hash<uint64_t>{}(combined);
	}
};
template <typename T>
using TexFlagKeyMap = std::unordered_map<TexFlagKey, T, TexFlagKey_Hash>;

class UD3D9Render : public URender {
	DECLARE_CLASS(UD3D9Render, URender, CLASS_Config, D3D9DrvRTX);

	// UObject interface.
	UD3D9Render();
	void StaticConstructor();
	
	// URenderBase interface
	void DrawWorld(FSceneNode* Frame) override;
	void DrawActor(FSceneNode* Frame, AActor* Actor) override;

	void ClipDecal(FSceneNode* frame, const FDecal* decal, const FBspSurf* surf, FSavedPoly* poly, std::vector<FTransTexture>& decalPoints);


private:
	enum RPASS {
		SOLID, NONSOLID, RPASS_MAX
	};
	struct ModelFacets {
		TexFlagKeyMap<std::vector<FSurfaceFacet>> facetPairs[RPASS_MAX];
	};
	typedef std::unordered_map<FTextureInfo*, std::vector<std::vector<FTransTexture>>> DecalMap;
	void getLevelModelFacets(FSceneNode* frame, ModelFacets& modelFacets);
	void drawPawnExtras(FSceneNode* frame, UD3D9RenderDevice* d3d9Dev, APawn* pawn, SpecialCoord& specialCoord);
	void getFacetDecals(FSceneNode* frame, const FSurfaceFacet& facet, DecalMap& decals, std::unordered_map<UTexture*, FTextureInfo>& lockedTextures);
};
