#pragma once

#include "D3D9DebugUtils.h"
#include "D3D9RenderDevice.h"

#include <Render.h>

#include <vector>
#include <array>
#include <unordered_map>

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
		void calculateSurfaceFacet(ULevel* level, const DWORD flags);
	};
	struct ModelFacets {
		std::array<std::array<SurfKeyBucketVector<UTexture*, SurfaceData>, RPASS_MAX>, FBspNode::MAX_ZONES> facetPairs;
	};
	struct FrameActors {
		std::vector<AActor*> actors;
		std::vector<ABrush*> movers;
		std::vector<AActor*> lights;
	};
	struct ParentCoord {
		FCoords worldCoord;
		FCoords localCoord;
	};
	typedef SurfKeyBucketVector<FTextureInfo*, std::vector<FTransTexture>> DecalMap;
	void getLevelModelFacets(FSceneNode* frame, ModelFacets& modelFacets);
	void drawActorSwitch(FSceneNode* frame, UD3D9RenderDevice* d3d9Dev, AActor* actor, ParentCoord* parentCoord = nullptr);
	void drawPawnExtras(FSceneNode* frame, UD3D9RenderDevice* d3d9Dev, APawn* pawn, SpecialCoord& specialCoord);
	void getSurfaceDecals(FSceneNode* frame, const SurfaceData& surfaceData, DecalMap& decals, std::unordered_map<UTexture*, FTextureInfo>& lockedTextures);
	void drawFrame(FSceneNode* frame, UD3D9RenderDevice* d3d9Dev, ModelFacets& modelFacets, FrameActors& objs, std::unordered_map<UTexture*, FTextureInfo>& lockedTextures, bool isSky = false);
#if RUNE
	void drawSkeletalActor(FSceneNode* frame, UD3D9RenderDevice* d3d9Dev, AActor* actor, const ParentCoord* parentCoord);
	// These seem to be non exported global variables that are swapped about in the original URender::DrawWorld
	// currFrameMaxZ seems to set in OccludeFrame to the furthest bsp vert
	static float* const currFrameMaxZ;
	// prevFrameMaxZ * 1.25 is checked against in SetupDynamics I presume to do distance culling of actors
	static float* const prevFrameMaxZ;
#endif
};

#if UNREAL_TOURNAMENT && !UNREAL_TOURNAMENT_OLDUNREAL
constexpr int UT436_size = 264;
static_assert(sizeof(URender) == UT436_size);
#elif UNREAL_GOLD
constexpr int UGold_size = 240;
static_assert(sizeof(URender) == UGold_size);
#elif RUNE
constexpr int Rune_size = 256;
static_assert(sizeof(URender) == Rune_size);
#elif UTGLR_HP_ENGINE
constexpr int HP1_size = 280;
static_assert(sizeof(URender) == HP1_size);
#endif
static_assert(sizeof(UD3D9Render) == sizeof(URender));
