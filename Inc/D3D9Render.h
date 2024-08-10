#pragma once

#include "D3D9DebugUtils.h"
#include "D3D9RenderDevice.h"

#include <Render.h>

#include <vector>
#include <unordered_map>

template <typename T>
struct TexFlagBucket {
	UTexture* tex;
	DWORD flags;
	std::vector<T> bucket;
};

template <typename T>
class TexFlagBucketVector : public std::vector<TexFlagBucket<T>> {
public:
	inline std::vector<T>& get(UTexture* tex, DWORD flags) {
		for (auto& entry : *this) {
			if (entry.tex == tex && entry.flags == flags) {
				return entry.bucket;
			}
		}
		// fell through, new entry
		auto* entry = &this->emplace_back();
		entry->tex = tex;
		entry->flags = flags;
		return entry->bucket;
	}
};

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
	struct SurfaceData {
		std::vector<INT> nodes;
		const FBspSurf* surf;
		FSurfaceFacet* facet = nullptr;
	};
	struct ModelFacets {
		TexFlagBucketVector<SurfaceData> facetPairs[RPASS_MAX];
	};
	struct ParentCoord {
		FCoords worldCoord;
		FCoords localCoord;
	};
	typedef SurfKeyBucketVector<std::vector<FTransTexture>> DecalMap;
	void getLevelModelFacets(FSceneNode* frame, ModelFacets& modelFacets);
	void drawActorSwitch(FSceneNode* frame, UD3D9RenderDevice* d3d9Dev, AActor* actor, ParentCoord* parentCoord = nullptr);
	void drawPawnExtras(FSceneNode* frame, UD3D9RenderDevice* d3d9Dev, APawn* pawn, SpecialCoord& specialCoord);
	void getSurfaceDecals(FSceneNode* frame, const SurfaceData& surfaceData, DecalMap& decals, std::unordered_map<UTexture*, FTextureInfo>& lockedTextures);
#if RUNE
	void drawSkeletalActor(FSceneNode* frame, UD3D9RenderDevice* d3d9Dev, AActor* actor, const ParentCoord* parentCoord);
#endif
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
