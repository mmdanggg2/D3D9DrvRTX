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
#if UTGLR_ALT_DECLARE_CLASS
	DECLARE_CLASS(UD3D9Render, URender, CLASS_Config);
#else
	DECLARE_CLASS(UD3D9Render, URender, CLASS_Config, D3D9DrvRTX);
#endif

	// UObject interface.
	UD3D9Render();
	void StaticConstructor();
	
	// URenderBase interface
	void DrawWorld(FSceneNode* Frame) override;
	void DrawActor(FSceneNode* Frame, AActor* Actor) override;

#if !UTGLR_NO_DECALS
	void ClipDecal(FSceneNode* frame, const FDecal* decal, const FBspSurf* surf, FSavedPoly* poly, std::vector<FTransTexture>& decalPoints);
#endif

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

#if UNREAL_TOURNAMENT && !UNREAL_TOURNAMENT_OLDUNREAL
constexpr int UT436_size = 264;
static_assert(sizeof(URender) == UT436_size);
static_assert(sizeof(UD3D9Render) == sizeof(URender));
#elif UNREAL_GOLD
constexpr int UGold_size = 240;
static_assert(sizeof(URender) == UGold_size);
static_assert(sizeof(UD3D9Render) == sizeof(URender));
#elif RUNE
constexpr int Rune_size = 256;
static_assert(sizeof(URender) == Rune_size);
static_assert(sizeof(UD3D9Render) == sizeof(URender));
#endif
