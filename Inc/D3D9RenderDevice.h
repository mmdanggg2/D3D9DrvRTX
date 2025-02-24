/*=============================================================================
	D3D9.h: Unreal D3D9 support header.
	Portions copyright 1999 Epic Games, Inc. All Rights Reserved.

	Revision history:

=============================================================================*/


#define DIRECT3D_VERSION 0x0900
#include <d3d9.h>

#include <stdlib.h>

#include "Render.h"
#include "UnRender.h"

#include "D3D9Config.h"

//#define D3D9_DEBUG

#include "D3D9DebugUtils.h"

#include <math.h>
#include <stdio.h>

#include <unordered_map>
#include <unordered_set>
#include <set>
#include <map>
#include <deque>
#include <vector>

#include "c_gclip.h"

#if !UTGLR_USES_ALPHABLEND
	#define UTGLR_USES_ALPHABLEND 0
	#define PF_AlphaBlend 0x020000
#endif

#if !UNREAL_TOURNAMENT_OLDUNREAL && !UNREAL_GOLD_OLDUNREAL
typedef DWORD PTRINT;
#endif

#if UTGLR_DEFINE_FTIME
typedef DOUBLE FTime;
#endif

#if UTGLR_DEFINE_HACK_FLAGS
extern DWORD GUglyHackFlags;
#endif

typedef IDirect3D9 * (WINAPI * LPDIRECT3DCREATE9)(UINT SDKVersion);


//Use debug D3D9 DLL
//#define UTD3D9R_USE_DEBUG_D3D9_DLL


//Optional fastcall calling convention usage
#define UTGLR_USE_FASTCALL

#ifdef UTGLR_USE_FASTCALL
	#ifdef WIN32
	#define FASTCALL	__fastcall
	#else
	#define FASTCALL
	#endif
#else
#define FASTCALL
#endif


//Debug defines
//#define UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
//#define UTGLR_DEBUG_SHOW_CALL_COUNTS
//#define UTGLR_DEBUG_WORLD_WIREFRAME
//#define UTGLR_DEBUG_ACTOR_WIREFRAME


#include "c_rbtree.h"


/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

//If exceeds 7, various things will break
#define MAX_TMUNITS			4		// vogel: maximum number of texture mapping units supported

//Must be at least 2000
#define VERTEX_BUFFER_SIZE	1000	// permanent small draw call buffer


/*-----------------------------------------------------------------------------
	D3D9Drv.
-----------------------------------------------------------------------------*/

class UD3D9RenderDevice;


enum bind_type_t {
	BIND_TYPE_ZERO_PREFIX,
	BIND_TYPE_NON_ZERO_PREFIX,
	BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST
};

constexpr BYTE CT_MIN_FILTER_POINT =		0x00;
constexpr BYTE CT_MIN_FILTER_LINEAR =		0x01;
constexpr BYTE CT_MIN_FILTER_ANISOTROPIC =	0x02;
constexpr BYTE CT_MIN_FILTER_MASK =			0x03;

constexpr BYTE CT_MIP_FILTER_NONE =			0x00;
constexpr BYTE CT_MIP_FILTER_POINT =		0x04;
constexpr BYTE CT_MIP_FILTER_LINEAR =		0x08;
constexpr BYTE CT_MIP_FILTER_MASK =			0x0C;

constexpr BYTE CT_MAG_FILTER_LINEAR_NOT_POINT_BIT = 0x10;

constexpr BYTE CT_HAS_MIPMAPS_BIT = 0x20;

constexpr BYTE CT_ADDRESS_CLAMP_NOT_WRAP_BIT = 0x40;

//Default texture parameters for new D3D texture
constexpr BYTE CT_DEFAULT_TEX_FILTER_PARAMS = CT_MIN_FILTER_POINT | CT_MIP_FILTER_NONE;

struct tex_params_t {
	BYTE filter;
	BYTE reserved1;
	BYTE reserved2;
	BYTE reserved3;
};

//Default texture stage parameters for D3D
constexpr tex_params_t CT_DEFAULT_TEX_PARAMS = { CT_DEFAULT_TEX_FILTER_PARAMS, 0, 0, 0 };

constexpr BYTE DT_NO_SMOOTH_BIT = 0x01;

struct FCachedTexture {
	IDirect3DTexture9 *pTexObj;
	DWORD LastUsedFrameCount;
	BYTE BaseMip;
	BYTE MaxLevel;
	BYTE UBits, VBits;
	DWORD UClampVal, VClampVal;
	FLOAT UMult, VMult;
	BYTE texType;
	BYTE bindType;
	BYTE treeIndex;
	BYTE dynamicTexBits;
#if UNREAL_TOURNAMENT_OLDUNREAL
	INT RealtimeChangeCount;
#elif UNREAL_GOLD_OLDUNREAL
	UINT RealtimeChangeCount;
#endif
	tex_params_t texParams;
	D3DFORMAT texFormat;
	void (FASTCALL UD3D9RenderDevice::*pConvertBGRA7777)(const FMipmapBase *, INT);
	void (FASTCALL UD3D9RenderDevice::*pConvertBGRA8)(const FMipmapBase *, INT);
	void (FASTCALL UD3D9RenderDevice::*pConvertRGBA8)(const FMipmapBase *, INT);
	FCachedTexture *pPrev;
	FCachedTexture *pNext;
};

class CCachedTextureChain {
public:
	CCachedTextureChain() {
		mark_as_clear();
	}
	~CCachedTextureChain() {
	}

	inline void mark_as_clear(void) {
		m_head.pNext = &m_tail;
		m_tail.pPrev = &m_head;
	}

	inline void FASTCALL unlink(FCachedTexture *pCT) {
		pCT->pPrev->pNext = pCT->pNext;
		pCT->pNext->pPrev = pCT->pPrev;
	}
	inline void FASTCALL link_to_tail(FCachedTexture *pCT) {
		pCT->pPrev = m_tail.pPrev;
		pCT->pNext = &m_tail;
		m_tail.pPrev->pNext = pCT;
		m_tail.pPrev = pCT;
	}

	inline FCachedTexture *begin(void) {
		return m_head.pNext;
	}
	inline FCachedTexture *end(void) {
		return &m_tail;
	}

private:
	FCachedTexture m_head;
	FCachedTexture m_tail;
};

struct FTexInfo {
	QWORD CurrentCacheID;
	DWORD CurrentDynamicPolyFlags;
	FCachedTexture *pBind;
	FLOAT UMult;
	FLOAT VMult;
	FLOAT UPan;
	FLOAT VPan;
};

//Vertex only (for intermediate buffering)
struct FGLVertex {
	FLOAT x;
	FLOAT y;
	FLOAT z;
};

//Normals
struct FGLNormal {
	FLOAT x;
	FLOAT y;
	FLOAT z;
};

//Vertex and primary color
struct FGLVertexColor {
	FLOAT x;
	FLOAT y;
	FLOAT z;
	FGLNormal norm;
	DWORD color;
};

struct FGLVertexColorTex {
	FLOAT x;
	FLOAT y;
	FLOAT z;
	DWORD color;
	FLOAT u;
	FLOAT v;
};

//Tex coords
struct FGLTexCoord {
	FLOAT u;
	FLOAT v;
};

struct FGLVertexNormTex {
	FGLVertex vert;
	FGLNormal norm;
	FGLTexCoord tex;
};

//Secondary color
struct FGLSecondaryColor {
	DWORD specular;
};

struct FGLMapDot {
	FLOAT u;
	FLOAT v;
};

struct FRenderVert {
	FVector Point{};
	FVector Normal{};
	FLOAT U{}, V{};
	DWORD Color{0xFFFFFFFF};
};

static inline UTexture* getTextureWithoutNext(UTexture* texture, FTime time, FLOAT fraction) {
	INT count = 1;
	for (UTexture* next = texture->AnimNext; next && next != texture; next = next->AnimNext)
		count++;
	INT index = Clamp(appFloor(fraction * count), 0, count - 1);
	while (index-- > 0)
		texture = texture->AnimNext;
	UTexture* oldNext = texture->AnimNext;
	texture->AnimNext = NULL;

#if UNREAL_GOLD_OLDUNREAL
	UTexture* oldCur = texture->AnimCurrent;
	texture->AnimCurrent = NULL;

	UTexture* renderTexture = texture->Get();

	texture->AnimCurrent = oldCur;
#else
	UTexture* oldCur = texture->AnimCur;
	texture->AnimCur = NULL;

	UTexture* renderTexture = texture->Get(time);

	texture->AnimCur = oldCur;
#endif

	texture->AnimNext = oldNext;

	return renderTexture;
}

static inline DWORD getBasePolyFlags(AActor* actor) {
	DWORD basePolyFlags = 0;
	if (actor->Style == STY_Masked) {
		basePolyFlags |= PF_Masked;
	}
	else if (actor->Style == STY_Translucent) {
		basePolyFlags |= PF_Translucent;
	}
	else if (actor->Style == STY_Modulated) {
		basePolyFlags |= PF_Modulated;
	}

	if (actor->bNoSmooth) basePolyFlags |= PF_NoSmooth;
	if (actor->bSelected) basePolyFlags |= PF_Selected;
	if (actor->bMeshEnviroMap) basePolyFlags |= PF_Environment;

	return basePolyFlags;
}

// https://stackoverflow.com/a/57595105/5233018
template <typename T, typename... Rest>
void hash_combine(std::size_t& seed, const T& v, const Rest&... rest) {
	seed ^= std::hash<T>{}(v)+0x9e3779b9 + (seed << 6) + (seed >> 2);
	(hash_combine(seed, rest), ...);
}

template<>
struct std::hash<FVector> {
	std::size_t operator()(const FVector& t) const {
		std::size_t hash = 0;
		hash_combine(hash, t.X, t.Y, t.Z);
		return hash;
	}
};

template<>
struct std::hash<FRotator> {
	std::size_t operator()(const FRotator& t) const {
		std::size_t hash = 0;
		hash_combine(hash, t.Roll, t.Pitch, t.Yaw);
		return hash;
	}
};

template<>
struct std::hash<FTextureInfo> {
	std::size_t operator()(const FTextureInfo& t) const {
		std::size_t hash = 0;
		hash_combine(
			hash,
			t.Texture,
			t.LOD,
			t.Mips[0],
			t.NumMips,
			t.Pan,
			t.CacheID,
			t.PaletteCacheID,
			t.UClamp,
			t.USize,
			t.UScale,
			t.VClamp,
			t.VSize,
			t.VScale
		);
		return hash;
	}
};

bool inline operator==(const FTextureInfo& lhs, const FTextureInfo& rhs) {
	return std::hash<FTextureInfo>()(lhs) == std::hash<FTextureInfo>()(rhs);
}

bool inline operator<(const FPlane& lhs, const FPlane& rhs) {
	if (lhs.X != rhs.X) return lhs.X < rhs.X;
	if (lhs.Y != rhs.Y) return lhs.Y < rhs.Y;
	if (lhs.Z != rhs.Z) return lhs.Z < rhs.Z;
	return lhs.W < rhs.W;
}

typedef const std::pair<FTextureInfo* const, const DWORD> SurfKey;
struct SurfKey_Hash {
	std::size_t operator () (const SurfKey& p) const {
		uint64_t combined = (reinterpret_cast<uint64_t>(p.first) << 32) | p.second;
		return std::hash<uint64_t>{}(combined);
	}
};
template <typename T>
using SurfKeyMap = std::unordered_map<SurfKey, T, SurfKey_Hash>;

template <typename K, typename T>
struct SurfKeyBucket {
	K tex;
	DWORD flags;
	std::vector<T> bucket;
};

template <typename K, typename T>
class SurfKeyBucketVector : public std::vector<SurfKeyBucket<K, T>> {
public:

	inline std::vector<T>& get(K tex, DWORD flags) {
		const size_t size = this->size();
		for (unsigned int i = 0; i < size; i++) {
			auto& entry = (*this)[i];
			if (entry.tex == tex && entry.flags == flags) {
				return entry.bucket;
			}
		}
		// fell through, new entry
		auto& entry = this->emplace_back();
		entry.tex = tex;
		entry.flags = flags;
		return entry.bucket;
	}
};

struct SpecialCoord {
	FCoords coord;
	FCoords baseCoord;
	FCoords worldCoord;
	bool exists = false;
	bool enabled = false;
};

struct ActorRenderData {
	SurfKeyBucketVector<UTexture*, FRenderVert> surfaceBuckets;
	D3DMATRIX actorMatrix;
};

typedef std::vector<ActorRenderData> RenderList;

constexpr const TCHAR* vertexBufferFailMessage = TEXT(
	"CreateVertexBuffer failed (0x%08X)\n"
	"This was likely caused by an error in RTX Remix."
);

#if UNREAL_TOURNAMENT_OLDUNREAL
typedef URenderDeviceOldUnreal469 RENDERDEVICE_SUPER;
#else
typedef URenderDevice RENDERDEVICE_SUPER;
#endif
//
// A D3D9 rendering device attached to a viewport.
//
class UD3D9RenderDevice : public RENDERDEVICE_SUPER {
#if UTGLR_ALT_DECLARE_CLASS
	DECLARE_CLASS(UD3D9RenderDevice, RENDERDEVICE_SUPER, CLASS_Config)
#else
	DECLARE_CLASS(UD3D9RenderDevice, RENDERDEVICE_SUPER, CLASS_Config, D3D9DrvRTX)
#endif

	// Static variables.
	static INT NumDevices;
	static INT LockCount;

	static HMODULE hModuleD3d9;
	static LPDIRECT3DCREATE9 pDirect3DCreate9;

#ifdef WIN32
	// Permanent variables.
	HWND m_hWnd;
	HDC m_hDC;
#endif

	IDirect3D9* m_d3d9;
	IDirect3DDevice9* m_d3dDevice;

	D3DCAPS9 m_d3dCaps;
	bool m_dxt1TextureCap;
	bool m_dxt3TextureCap;
	bool m_dxt5TextureCap;

	D3DPRESENT_PARAMETERS m_d3dpp;

	IDirect3DTexture9* m_pNoTexObj;

	//Vertex declarations
	IDirect3DVertexDeclaration9 *m_oneColorVertexDecl;
	IDirect3DVertexDeclaration9* m_ColorTexVertexDecl;
	IDirect3DVertexDeclaration9 *m_standardNTextureVertexDecl[MAX_TMUNITS];
	IDirect3DVertexDeclaration9 *m_twoColorSingleTextureVertexDecl;

	//Current vertex declaration state tracking
	IDirect3DVertexDeclaration9 *m_curVertexDecl;
	//Vertex and primary color
	std::vector<FRenderVert> m_csVertexArray;
	IDirect3DVertexBuffer9 *m_d3dVertexColorBuffer;
	FGLVertexColor *m_pVertexColorArray;
	INT m_vertexTempBufferSize;
	IDirect3DVertexBuffer9* m_d3dTempVertexColorBuffer;
	IDirect3DVertexBuffer9* m_currentVertexColorBuffer;

	//Quad buffer
	IDirect3DVertexBuffer9* m_d3dQuadBuffer;
	DWORD m_QuadBufferColor;

	//Secondary color
	IDirect3DVertexBuffer9 *m_d3dSecondaryColorBuffer;
	FGLSecondaryColor *m_pSecondaryColorArray;

	//Tex coords
	IDirect3DVertexBuffer9 *m_d3dTexCoordBuffer[MAX_TMUNITS];
	FGLTexCoord *m_pTexCoordArray[MAX_TMUNITS];
	INT m_texTempBufferSize[MAX_TMUNITS];
	IDirect3DVertexBuffer9* m_d3dTempTexCoordBuffer[MAX_TMUNITS];
	IDirect3DVertexBuffer9* m_currentTexCoordBuffer[MAX_TMUNITS];

	//Vertex buffer state flags
	INT m_curVertexBufferPos;
	bool m_vertexColorBufferNeedsDiscard;
	bool m_secondaryColorBufferNeedsDiscard;
	bool m_texCoordBufferNeedsDiscard[MAX_TMUNITS];

	void (FASTCALL* m_pBuffer3BasicVertsProc)(UD3D9RenderDevice*, FTransTexture**);
	void (FASTCALL* m_pBuffer3ColoredVertsProc)(UD3D9RenderDevice*, FTransTexture**);
	void (FASTCALL* m_pBuffer3FoggedVertsProc)(UD3D9RenderDevice*, FTransTexture**);

	void (FASTCALL* m_pBuffer3VertsProc)(UD3D9RenderDevice*, FTransTexture**);

	//Texture state cache information
	BYTE m_texEnableBits;

	UBOOL WasFullscreen;

	bool m_frameRateLimitTimerInitialized;

	bool m_prevSwapBuffersStatus;

	std::set<FPlane> Modes;

	// Timing.
	DWORD BindCycles, ImageCycles, ComplexCycles, GouraudCycles, TileCycles;

	DWORD m_vbFlushCount;

	// Hardware constraints.
	FLOAT LODBias;
	UBOOL OneXBlending;
	INT MaxLogUOverV;
	INT MaxLogVOverU;
	INT MinLogTextureSize;
	INT MaxLogTextureSize;
	INT MaxAnisotropy;
	INT TMUnits;
	INT RefreshRate;
	UBOOL UsePrecache;
	UBOOL UseTrilinear;
	UBOOL UseS3TC;
	UBOOL NoFiltering;
	UBOOL SinglePassFog;
	UBOOL UseTexIdPool;
	UBOOL UseTexPool;
	INT DynamicTexIdRecycleLevel;
	UBOOL TexDXT1ToDXT3;
	INT FrameRateLimit;
	FTime m_prevFrameTimestamp;
	UBOOL SmoothMaskedTextures;

	UBOOL NonSolidTranslucentHack;
	UBOOL EnableSkyBoxRendering;
	UBOOL EnableSkyBoxAnchors;
	UBOOL EnableHashTextures;
	FLOAT LightMultiplier;
	FLOAT LightRadiusDivisor;
	FLOAT LightRadiusExponent;

	FColor SurfaceSelectionColor;

	std::unordered_set<std::wstring> hashTexBlacklist;

	//Previous lock variables
	//Used to detect changes in settings
	UBOOL PL_OneXBlending;
	INT PL_MaxLogUOverV;
	INT PL_MaxLogVOverU;
	INT PL_MinLogTextureSize;
	INT PL_MaxLogTextureSize;
	UBOOL PL_NoFiltering;
	UBOOL PL_UseTrilinear;
	UBOOL PL_TexDXT1ToDXT3;
	INT PL_MaxAnisotropy;
	UBOOL PL_SmoothMaskedTextures;
	FLOAT PL_LODBias;

	INT m_rpPassCount;
	INT m_rpTMUnits;
	bool m_rpForceSingle;
	bool m_rpMasked;
	bool m_rpSetDepthEqual;

	// Hit info.
	BYTE* m_HitData;
	INT* m_HitSize;
	INT m_HitBufSize;
	INT m_HitCount;
	CGClip m_gclip;

	DWORD m_currentFrameCount;

	// Lock variables.
	FPlane FlashScale, FlashFog;
	FLOAT m_RFX2, m_RFY2;
	INT m_sceneNodeX, m_sceneNodeY;

	DWORD m_curBlendFlags;
	DWORD m_smoothMaskedTexturesBit;
	DWORD m_curPolyFlags;

	enum {
		CF_COLOR_ARRAY		= 0x01,
		CF_FOG_MODE			= 0x02,
	};
	BYTE m_requestedColorFlags;
#if UTGLR_USES_ALPHABLEND
	BYTE m_gpAlpha;
#endif

	DWORD m_curTexEnvFlags[MAX_TMUNITS];
	tex_params_t m_curTexStageParams[MAX_TMUNITS];
	FTexInfo TexInfo[MAX_TMUNITS];

	INT m_SetRes_NewX;
	INT m_SetRes_NewY;
	INT m_SetRes_NewColorBytes;
	UBOOL m_SetRes_Fullscreen;
	bool m_SetRes_isDeviceReset;

	// MultiPass rendering information
	struct FGLRenderPass {
		struct FGLSinglePass {
			FTextureInfo* Info;
			DWORD PolyFlags;
			FLOAT PanBias;
		} TMU[MAX_TMUNITS];
	} MultiPass;				// vogel: MULTIPASS!!! ;)

	typedef rbtree<DWORD, FCachedTexture> DWORD_CTTree_t;
	typedef rbtree_allocator<DWORD_CTTree_t> DWORD_CTTree_Allocator_t;
	typedef rbtree<QWORD, FCachedTexture> QWORD_CTTree_t;
	typedef rbtree_allocator<QWORD_CTTree_t> QWORD_CTTree_Allocator_t;
	typedef rbtree_node_pool<QWORD_CTTree_t> QWORD_CTTree_NodePool_t;
	typedef DWORD TexPoolMapKey_t;
	typedef rbtree<TexPoolMapKey_t, QWORD_CTTree_NodePool_t> TexPoolMap_t;
	typedef rbtree_allocator<TexPoolMap_t> TexPoolMap_Allocator_t;

	enum { NUM_CTTree_TREES = 16 }; //Must be a power of 2
	inline DWORD FASTCALL CTZeroPrefixCacheIDSuffixToTreeIndex(DWORD CacheIDSuffix) {
		return ((CacheIDSuffix >> 12) & (NUM_CTTree_TREES - 1));
	}
	inline DWORD FASTCALL CTNonZeroPrefixCacheIDSuffixToTreeIndex(DWORD CacheIDSuffix) {
		return ((CacheIDSuffix >> 20) & (NUM_CTTree_TREES - 1));
	}
	inline DWORD FASTCALL MakeTexPoolMapKey(DWORD UBits, DWORD VBits) {
		return ((UBits << 16) | VBits);
	}

	DWORD_CTTree_t m_localZeroPrefixBindTrees[NUM_CTTree_TREES], * m_zeroPrefixBindTrees;
	QWORD_CTTree_t m_localNonZeroPrefixBindTrees[NUM_CTTree_TREES], * m_nonZeroPrefixBindTrees;
	CCachedTextureChain m_localNonZeroPrefixBindChain, * m_nonZeroPrefixBindChain;
	TexPoolMap_t m_localRGBA8TexPool, * m_RGBA8TexPool;

	DWORD_CTTree_Allocator_t m_DWORD_CTTree_Allocator;
	QWORD_CTTree_Allocator_t m_QWORD_CTTree_Allocator;
	TexPoolMap_Allocator_t m_TexPoolMap_Allocator;

	QWORD_CTTree_NodePool_t m_nonZeroPrefixNodePool;

	//Fixed texture cache ids
#define TEX_CACHE_ID_UNUSED		0xFFFFFFFFFFFFFFFFULL
#define TEX_CACHE_ID_NO_TEX		0xFFFFFFFF00000010ULL

//Mask for poly flags that impact texture object state
#define TEX_DYNAMIC_POLY_FLAGS_MASK		(PF_NoSmooth)

// Information about a cached texture.
	enum tex_type_t {
		TEX_TYPE_NONE,
		TEX_TYPE_COMPRESSED_DXT1,
		TEX_TYPE_COMPRESSED_DXT1_TO_DXT3,
		TEX_TYPE_COMPRESSED_DXT3,
		TEX_TYPE_COMPRESSED_DXT5,
		TEX_TYPE_PALETTED,
		TEX_TYPE_HAS_PALETTE,
		TEX_TYPE_NORMAL,
		TEX_TYPE_CACHE_GEN,
	};
#define TEX_FLAG_NO_CLAMP	0x00000001

	struct FTexConvertCtx {
		INT stepBits;
		DWORD texWidthPow2;
		DWORD texHeightPow2;
		const FCachedTexture* pBind;
		D3DLOCKED_RECT lockRect;
	} m_texConvertCtx;

	class LightSlots {
	private:
		std::unordered_map<AActor*, int> actorSlots;
		std::deque<int> availableSlots;
		ods_stream dout;

	public:
		LightSlots(int numSlots) {
			actorSlots.reserve(numSlots);
			for (int i = 0; i < numSlots; ++i) {
				availableSlots.push_back(i);
			}
		}

		// Updates which actors are in the slots, returns a set of slots that are no longer used
		std::unordered_set<int> updateActors(const std::vector<AActor*>& actors);

		const std::deque<int> unusedSlots() {
			return availableSlots;
		}

		const std::unordered_map<AActor*, int> slotMap() {
			return actorSlots;
		}
	};
	LightSlots* lightSlots;

	struct TileFuncCall {
		FSceneNode frame;
		FTextureInfo texInfo;
		FLOAT X, Y, XL, YL, U, V, UL, VL, Z;
		FPlane Color;
		FPlane Fog;
		DWORD PolyFlags;

		TileFuncCall() {}
		TileFuncCall(const TileFuncCall& other) = default;

		void operator()(UD3D9RenderDevice* device) {
			device->DrawTile(&frame, texInfo, X, Y, XL, YL, U, V, UL, VL, nullptr, Z, Color, Fog, PolyFlags);
		}
	};
	bool bufferTileDraws;
	std::vector<TileFuncCall> bufferedTileDraws;

	inline void FlushVertexBuffers(void) {
		//dout << L"Vertex buffers flushed" << std::endl;
		m_curVertexBufferPos = 0;
		m_vertexColorBufferNeedsDiscard = true;
		m_secondaryColorBufferNeedsDiscard = true;
		for (int u = 0; u < MAX_TMUNITS; u++) {
			m_texCoordBufferNeedsDiscard[u] = true;
		}

#ifdef D3D9_DEBUG
		m_vbFlushCount++;
#endif
	}

	// Gets the current vert buffer position and increments it by numPoints
	inline INT getVertBufferPos(INT numPoints) {
		INT bufferPos = 0;
		if (numPoints <= VERTEX_BUFFER_SIZE) {
			bufferPos = m_curVertexBufferPos;

			//Advance vertex buffer position
			m_curVertexBufferPos += numPoints;
		}
		return bufferPos;
	}

	inline bool needsNewBuffer(DWORD polyFlags, int numVerts, FTextureInfo* info = nullptr) {
		if (m_curPolyFlags != polyFlags) return true;
		QWORD cacheId = info ? calcCacheID(*info, polyFlags) : TEX_CACHE_ID_NO_TEX;
		if (TexInfo[0].CurrentCacheID != cacheId) return true;
		if ((m_curVertexBufferPos + m_bufferedVerts + numVerts) >= (VERTEX_BUFFER_SIZE)) return true;
		return m_bufferedVerts == 0;
	}

	// Locks/Creates a vertex buffer appropriate for the given number of points
	inline void LockVertexColorBuffer(INT numPoints) {
		DWORD lockFlags = D3DLOCK_NOSYSLOCK;
		HRESULT hResult;
		INT bufferPos;
		IDirect3DVertexBuffer9* vertBuffer;

		// Bigger than our main buffer, allocate new one of the appropriate size
		if (numPoints > VERTEX_BUFFER_SIZE) {
			if (m_vertexTempBufferSize != numPoints || !m_d3dTempVertexColorBuffer) {
				if (m_d3dTempVertexColorBuffer) {
					//dout << L"Releasing vert buffer of size " << m_vertexTempBufferSize << std::endl;
					m_d3dTempVertexColorBuffer->Release();
				}
				//dout << L"Creating vert buffer of size " << numPoints << std::endl;
				hResult = m_d3dDevice->CreateVertexBuffer(sizeof(FGLVertexColor) * numPoints, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &m_d3dTempVertexColorBuffer, NULL);
				if (FAILED(hResult)) {
					appErrorf(vertexBufferFailMessage, hResult);
				}
				m_vertexTempBufferSize = numPoints;
			} else {
				lockFlags |= D3DLOCK_DISCARD;
			}
			vertBuffer = m_d3dTempVertexColorBuffer;
			bufferPos = 0;
		} else {
			if (m_vertexColorBufferNeedsDiscard) {
				m_vertexColorBufferNeedsDiscard = false;
				lockFlags |= D3DLOCK_DISCARD;
			} else {
				lockFlags |= D3DLOCK_NOOVERWRITE;
			}
			vertBuffer = m_d3dVertexColorBuffer;
			bufferPos = m_curVertexBufferPos;
		}
		// Set the stream if the buffer changed
		if (vertBuffer != m_currentVertexColorBuffer) {
			hResult = m_d3dDevice->SetStreamSource(0, vertBuffer, 0, sizeof(FGLVertexColor));
			if (FAILED(hResult)) {
				appErrorf(TEXT("SetStreamSource failed"));
			}
			m_currentVertexColorBuffer = vertBuffer;
		}

		BYTE* pData = nullptr;

		//dout << L"Locking vert buffer of size " << numPoints << std::endl;
		if (FAILED(vertBuffer->Lock(0, 0, (VOID**)&pData, lockFlags))) {
			appErrorf(TEXT("Vertex buffer lock failed"));
		}

		m_pVertexColorArray = (FGLVertexColor*)(pData + (bufferPos * sizeof(FGLVertexColor)));
	}
	inline void UnlockVertexColorBuffer(void) {
		if (FAILED(m_currentVertexColorBuffer->Unlock())) {
			appErrorf(TEXT("Vertex buffer unlock failed"));
		}
	}

	inline void LockSecondaryColorBuffer(INT numPoints) {
		appErrorf(TEXT("Can't be bothered, don't use LockSecondaryColorBuffer thx bye!"));
		FGLSecondaryColor* pData = nullptr;
		if (FAILED(m_d3dSecondaryColorBuffer->Lock(0, 0, (VOID**)&pData, D3DLOCK_NOSYSLOCK))) {
			appErrorf(TEXT("Vertex buffer lock failed"));
		}

		m_pSecondaryColorArray = pData;
	}
	inline void UnlockSecondaryColorBuffer(void) {
		if (FAILED(m_d3dSecondaryColorBuffer->Unlock())) {
			appErrorf(TEXT("Vertex buffer unlock failed"));
		}
	}

	// Locks/Creates a UV buffer appropriate for the given number of points
	inline void FASTCALL LockTexCoordBuffer(DWORD texUnit, INT numPoints) {
		DWORD lockFlags = D3DLOCK_NOSYSLOCK;
		HRESULT hResult;
		IDirect3DVertexBuffer9* texBuffer;
		INT bufferPos;

		if (numPoints > VERTEX_BUFFER_SIZE) {
			check(m_vertexTempBufferSize == numPoints);
			IDirect3DVertexBuffer9*& texCoordBuffer = m_d3dTempTexCoordBuffer[texUnit];
			if (m_texTempBufferSize[texUnit] != numPoints || !texCoordBuffer) {
				if (texCoordBuffer) {
					//dout << L"Releasing tex buffer of size " << m_texTempBufferSize[texUnit] << std::endl;
					texCoordBuffer->Release();
				}
				//dout << L"Creating tex buffer of size " << numPoints << std::endl;
				hResult = m_d3dDevice->CreateVertexBuffer(sizeof(FGLTexCoord) * numPoints, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &texCoordBuffer, NULL);
				if (FAILED(hResult)) {
					appErrorf(vertexBufferFailMessage, hResult);
				}
				m_texTempBufferSize[texUnit] = numPoints;
			} else {
				lockFlags |= D3DLOCK_DISCARD;
			}
			texBuffer = texCoordBuffer;
			bufferPos = 0;
		} else {
			if (m_texCoordBufferNeedsDiscard[texUnit]) {
				m_texCoordBufferNeedsDiscard[texUnit] = false;
				lockFlags |= D3DLOCK_DISCARD;
			} else {
				lockFlags |= D3DLOCK_NOOVERWRITE;
			}
			texBuffer = m_d3dTexCoordBuffer[texUnit];
			bufferPos = m_curVertexBufferPos;
		}
		if (m_currentTexCoordBuffer[texUnit] != texBuffer) {
			hResult = m_d3dDevice->SetStreamSource(2 + texUnit, texBuffer, 0, sizeof(FGLTexCoord));
			if (FAILED(hResult)) {
				appErrorf(TEXT("SetStreamSource failed"));
			}
			m_currentTexCoordBuffer[texUnit] = texBuffer;
		}

		BYTE* pData = nullptr;

		//dout << L"Locking tex buffer of size " << numPoints << std::endl;
		if (FAILED(texBuffer->Lock(0, 0, (VOID**)&pData, lockFlags))) {
			appErrorf(TEXT("Vertex buffer lock failed"));
		}

		m_pTexCoordArray[texUnit] = (FGLTexCoord*)(pData + (bufferPos * sizeof(FGLTexCoord)));
	}
	inline void FASTCALL UnlockTexCoordBuffer(DWORD texUnit) {
		if (FAILED(m_currentTexCoordBuffer[texUnit]->Unlock())) {
			appErrorf(TEXT("Vertex buffer unlock failed"));
		}
	}

	// Updates the vertex colour of the quad buffer
	inline void updateQuadBuffer(DWORD color);

	enum {
		BV_TYPE_NONE			= 0x00,
		BV_TYPE_GOURAUD_POLYS	= 0x01,
		BV_TYPE_TILES			= 0x02,
		BV_TYPE_LINES			= 0x03,
		BV_TYPE_POINTS			= 0x04,
	};
	BYTE m_bufferedVertsType;
	DWORD m_bufferedVerts;

	inline void FASTCALL StartBuffering(DWORD bvType) {
		m_bufferedVertsType = bvType;
	}
	inline void FASTCALL EndBufferingExcept(DWORD bvExceptType) {
		if (m_bufferedVertsType != bvExceptType) {
			EndBuffering();
		}
	}
	inline void EndBuffering(void) {
		if (m_bufferedVerts > 0) {
			EndBufferingNoCheck();
		}
	}
	void EndBufferingNoCheck(void);
	void EndGouraudPolygonBufferingNoCheck(void);
	void EndTileBufferingNoCheck(void);
	void EndLineBufferingNoCheck(void);
	void EndPointBufferingNoCheck(void);


	static const TCHAR* StaticConfigName() { return TEXT("D3D9DrvRTX"); }

	// UObject interface.
	void StaticConstructor();

	// FExec interface
	UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar) override;

	// URenderDevice interface.
	UBOOL Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) override;
	UBOOL SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) override;
	void Exit() override;
#if UTGLR_ALT_FLUSH
	void Flush() override;
#else
	void Flush(UBOOL AllowPrecache) override;
#endif
	void Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize) override;
	void Unlock(UBOOL Blit) override;
	void DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet) override;
	void DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span) override;
	void DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags) override;
	void Draw3DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2) override;
	void Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2) override;
	void Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z) override;
	void ClearZ(FSceneNode* Frame) override;
	void PushHit(const BYTE* Data, INT Count) override;
	void PopHit(INT Count, UBOOL bForce) override;
	void GetStats(TCHAR* Result) override;
#if UNREAL_GOLD_OLDUNREAL
	void ReadPixels(FColor* Pixels, UBOOL bGammaCorrectOutput) override;
#else
	void ReadPixels(FColor* Pixels) override;
#endif
	void EndFlash() override;

	void SetSceneNode(FSceneNode* Frame) override;
	void PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags) override;

#if UNREAL_TOURNAMENT_OLDUNREAL
	UBOOL SupportsTextureFormat(ETextureFormat Format) override;
#endif
#ifdef RUNE
	void PreDrawFogSurface() override;
	void PostDrawFogSurface() override;
	void DrawFogSurface(FSceneNode* Frame, FFogSurf &FogSurf) override;
	void PreDrawGouraud(FSceneNode* Frame, FLOAT FogDistance, FPlane FogColor) override;
	void PostDrawGouraud(FLOAT FogDistance) override;
#endif
#if UTGLR_HP_ENGINE
	INT MaxVertices() override { return 0xFFFF; }
	void DrawTriangles(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, USHORT* Indices, INT NumIdx, DWORD PolyFlags, FSpanBuffer* Span) override;
#endif

	// Implementation.
	void InitFrameRateLimitTimerSafe(void);
	void ShutdownFrameRateLimitTimer(void);

	UBOOL FailedInitf(const TCHAR* Fmt, ...);
	void ShutdownAfterError() override;

	void UnsetRes();
	UBOOL ResetDevice();

	void ConfigValidate_RequiredExtensions(void);

	void InitPermanentResourcesAndRenderingState(void);
	void FreePermanentResources(void);

	void DrawGouraudPolygonOld(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span);

	void InitNoTextureSafe(void);

	void ScanForOldTextures(void);

	inline void FASTCALL SetNoTexture(INT Multi) {
		if (TexInfo[Multi].CurrentCacheID != TEX_CACHE_ID_NO_TEX) {
			SetNoTextureNoCheck(Multi);
		}
	}

	void FASTCALL SetNoTextureNoCheck(INT Multi);

	// Reworked masked texture hack
	// sets the last bit to 1 when a texture is masked this stops the same texture
	// with different flags clashing and having the wrong properties
	inline QWORD calcCacheID(const FTextureInfo& info, DWORD polyFlags) {
		QWORD cacheID = info.CacheID;
		if ((cacheID & 0xFF) == CID_RenderTexture && (polyFlags & PF_Masked)) {
			cacheID |= 1;
		}
		return cacheID;
	}

	inline void FASTCALL SetTexture(INT Multi, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias) {
		FTexInfo& Tex = TexInfo[Multi];

		// Set panning.
		Tex.UPan = Info.Pan.X + (PanBias * Info.UScale);
		Tex.VPan = Info.Pan.Y + (PanBias * Info.VScale);

		//Load texture cache id
		QWORD CacheID = calcCacheID(Info, PolyFlags);

		//Get dynamic poly flags
		DWORD DynamicPolyFlags = PolyFlags & TEX_DYNAMIC_POLY_FLAGS_MASK;

		// Find in cache.
		if ((CacheID == Tex.CurrentCacheID) && (DynamicPolyFlags == Tex.CurrentDynamicPolyFlags) && !Info.bRealtimeChanged) {
			return;
		}

		//Update soon to be current texture cache id
		Tex.CurrentCacheID = CacheID;
		Tex.CurrentDynamicPolyFlags = DynamicPolyFlags;

		SetTextureNoCheck(Multi, Tex, Info, PolyFlags);

		return;
	}

	inline void FASTCALL SetTextureNoPanBias(INT Multi, FTextureInfo& Info, DWORD PolyFlags) {
		FTexInfo& Tex = TexInfo[Multi];

		// Set panning.
		Tex.UPan = Info.Pan.X;
		Tex.VPan = Info.Pan.Y;

		//Load texture cache id
		QWORD CacheID = calcCacheID(Info, PolyFlags);

		//Get dynamic poly flags
		DWORD DynamicPolyFlags = PolyFlags & TEX_DYNAMIC_POLY_FLAGS_MASK;

		// Find in cache.
		if ((CacheID == Tex.CurrentCacheID) && (DynamicPolyFlags == Tex.CurrentDynamicPolyFlags) && !Info.bRealtimeChanged) {
			return;
		}

		//Update soon to be current texture cache id
		Tex.CurrentCacheID = CacheID;
		Tex.CurrentDynamicPolyFlags = DynamicPolyFlags;

		SetTextureNoCheck(Multi, Tex, Info, PolyFlags);

		return;
	}

	bool FASTCALL BindTexture(DWORD texNum, FTexInfo& Tex, FTextureInfo& Info, DWORD PolyFlags, FCachedTexture*& Bind);
	void FASTCALL SetTextureNoCheck(DWORD texNum, FTexInfo& Tex, FTextureInfo& Info, DWORD PolyFlags);
	void FASTCALL CacheTextureInfo(FCachedTexture *pBind, const FTextureInfo &Info, DWORD PolyFlags);

	void FASTCALL ConvertDXT1_DXT1(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertDXT1_DXT3(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertDXT35_DXT35(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertP8_RGBA8888(const FMipmapBase *Mip, const FColor *Palette, INT Level);
	void FASTCALL ConvertP8_RGBA8888_NoStep(const FMipmapBase *Mip, const FColor *Palette, INT Level);
	void FASTCALL ConvertP8_RGB565(const FMipmapBase *Mip, const FColor *Palette, INT Level);
	void FASTCALL ConvertP8_RGB565_NoStep(const FMipmapBase *Mip, const FColor *Palette, INT Level);
	void FASTCALL ConvertP8_RGBA5551(const FMipmapBase *Mip, const FColor *Palette, INT Level);
	void FASTCALL ConvertP8_RGBA5551_NoStep(const FMipmapBase *Mip, const FColor *Palette, INT Level);
	void FASTCALL ConvertBGRA7777_BGRA8888(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertBGRA7777_BGRA8888_NoClamp(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertBGRA8_BGRA8888(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertBGRA8_BGRA8888_NoClamp(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertRGBA8_BGRA8888(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertRGBA8_BGRA8888_NoClamp(const FMipmapBase *Mip, INT Level);

	inline void FASTCALL SetBlend(DWORD PolyFlags, bool isUI = false) {
#if UTGLR_USES_ALPHABLEND
		if (PolyFlags & PF_AlphaBlend) {
			if (!(PolyFlags & PF_Masked)) {
				PolyFlags |= PF_Occlude;
			}
			else {
				PolyFlags &= ~PF_Masked;
			}
		}
		else
#endif
		if (!(PolyFlags & (PF_Translucent | PF_Modulated | PF_Highlighted))) {
			PolyFlags |= PF_Occlude;
		}
		else if (PolyFlags & PF_Translucent) {
			PolyFlags &= ~PF_Masked;
		}

		//Only check relevant blend flags
		DWORD blendFlags = PolyFlags & (
			PF_Translucent | 
			PF_Modulated | 
			PF_Invisible | 
			PF_Occlude | 
			PF_Masked | 
			PF_Highlighted | 
			PF_RenderFog | 
			PF_AlphaBlend | 
			PF_TwoSided | 
			PF_NotSolid
			);
		if (m_curBlendFlags != blendFlags) {
			SetBlendNoCheck(blendFlags, isUI);
		}
	}
	void FASTCALL SetBlendNoCheck(DWORD blendFlags, bool isUI);

	inline void FASTCALL SetTexEnv(INT texUnit, DWORD PolyFlags) {
		//Only check relevant tex env flags
		DWORD texEnvFlags = PolyFlags & (PF_Modulated | PF_Highlighted | PF_Memorized | PF_FlatShaded);
		//Modulated by default
		if ((texEnvFlags & (PF_Modulated | PF_Highlighted | PF_Memorized)) == 0) {
			texEnvFlags |= PF_Modulated;
		}
		if (m_curTexEnvFlags[texUnit] != texEnvFlags) {
			SetTexEnvNoCheck(texUnit, texEnvFlags);
		}
	}
	void InitOrInvalidateTexEnvState(void);
	void FASTCALL SetTexLODBiasState(INT TMUnits);
	void FASTCALL SetTexMaxAnisotropyState(INT TMUnits);
	void FASTCALL SetTexEnvNoCheck(DWORD texUnit, DWORD texEnvFlags);

	inline void FASTCALL SetTexFilter(DWORD texNum, BYTE texFilterParams) {
		if (m_curTexStageParams[texNum].filter != texFilterParams) {
			SetTexFilterNoCheck(texNum, texFilterParams);
		}
	}
	void FASTCALL SetTexFilterNoCheck(DWORD texNum, BYTE texFilterParams);


	inline void SetDefaultTextureState(void) {
		//Check if only texture unit zero is enabled
		if (m_texEnableBits != 0x1) {
			DWORD texUnit;
			DWORD texBit;

			//Disable all texture units except texture unit zero
			for (texUnit = 1, texBit = 0x2; m_texEnableBits != 0x1; texUnit++, texBit <<= 1) {
				//See if the texture unit is enabled
				if (texBit & m_texEnableBits) {
					//Update tex enable bits (sub to clear known set bit)
					m_texEnableBits -= texBit;

					//Mark the texture unit as disabled
					m_curTexEnvFlags[texUnit] = 0;

					//Disable the texture unit
					m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLOROP, D3DTOP_DISABLE);
					m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
				}
			}
		}

		return;
	}

	inline void FASTCALL DisableSubsequentTextures(DWORD firstTexUnit) {
		DWORD texUnit;
		BYTE texBit = 1U << firstTexUnit;

		//Disable subsequent texture units
		for (texUnit = firstTexUnit; texBit <= m_texEnableBits; texUnit++, texBit <<= 1) {
			//See if the texture unit is enabled
			if (texBit & m_texEnableBits) {
				//Update tex enable bits (sub to clear known set bit)
				m_texEnableBits -= texBit;

				//Mark the texture unit as disabled
				m_curTexEnvFlags[texUnit] = 0;

				//Disable the texture unit
				m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLOROP, D3DTOP_DISABLE);
				m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
			}
		}

		return;
	}

	inline void SetDefaultStreamState(void) {
		if (m_curVertexDecl != m_standardNTextureVertexDecl[0]) {
			SetVertexDeclNoCheck(m_standardNTextureVertexDecl[0]);
		}
	}
	inline void FASTCALL SetStreamState(IDirect3DVertexDeclaration9 *vertexDecl) {
		if (m_curVertexDecl != vertexDecl) {
			SetVertexDeclNoCheck(vertexDecl);
		}
	}
	void FASTCALL SetVertexDeclNoCheck(IDirect3DVertexDeclaration9 *vertexDecl);

	inline void RenderPasses(void) {
		if (m_rpPassCount != 0) {
			RenderPassesExec();
		}
	}

	inline void FASTCALL AddRenderPass(FTextureInfo* Info, DWORD PolyFlags, FLOAT PanBias) {
		INT rpPassCount = m_rpPassCount;

		MultiPass.TMU[rpPassCount].Info      = Info;
		MultiPass.TMU[rpPassCount].PolyFlags = PolyFlags;
		MultiPass.TMU[rpPassCount].PanBias   = PanBias;

		//Single texture rendering forced here by setting m_rpTMUnits equal to 1
		rpPassCount++;
		m_rpPassCount = rpPassCount;
		if (rpPassCount >= m_rpTMUnits) {
			//m_rpPassCount will never be equal to 0 here
			RenderPassesExec();
		}
	}

	void RenderPassesExec(void);

	void RenderPassesNoCheckSetup(void);

	INT FASTCALL BufferStaticComplexSurfaceGeometry(const FSurfaceFacet& Facet, const FGLMapDot& csDot, bool append = false);
	INT FASTCALL BufferTriangleSurfaceGeometry(const std::vector<FRenderVert>& vertices);

	void FASTCALL BufferAdditionalClippedVerts(FTransTexture** Pts, INT NumPts);

	// Takes a list of faces and draws them in batches
	void drawLevelSurfaces(FSceneNode* frame, FSurfaceInfo& surface, std::vector<FSurfaceFacet*>& facets);

	// Render a sprite actor
	void renderSprite(FSceneNode* frame, AActor* actor);
	// Renders a sprite at the given location
	void renderSpriteGeo(FSceneNode* frame, const FVector& location, FLOAT drawScaleU, FLOAT drawScaleV, FTextureInfo& texInfo, DWORD basePolyFlags, FPlane color);
	inline void renderSpriteGeo(FSceneNode* frame, const FVector& location, FLOAT drawScale, FTextureInfo& texInfo, DWORD basePolyFlags, FPlane color) {
		renderSpriteGeo(frame, location, drawScale, drawScale, texInfo, basePolyFlags, color);
	}

	// Renders a mesh actor
	void renderMeshActor(FSceneNode* frame, AActor* actor, RenderList& renderList, SpecialCoord* specialCoord = nullptr);

#if UNREAL_GOLD_OLDUNREAL
	void renderStaticMeshActor(FSceneNode* frame, AActor* actor, RenderList& renderList, SpecialCoord* specialCoord = nullptr);
	void renderTerrainMeshActor(FSceneNode* frame, AActor* actor, RenderList& renderList, SpecialCoord* specialCoord = nullptr);
#endif
#if RUNE
	// Renders a skeletal mesh actor
	void renderSkeletalMeshActor(FSceneNode* frame, AActor* actor, RenderList& renderList, const FCoords* parentCoord = nullptr);
	// Renders a particleSystem actor
	void renderParticleSystemActor(FSceneNode* frame, AParticleSystem* actor, const FCoords& parentCoord);
#endif
	// Renders a mover brush
	void renderMover(FSceneNode* frame, ABrush* mover);
	// Updates and sends the given lights to dx
	void renderLights(FSceneNode* frame, std::vector<AActor*> lightActors);
	// Renders a magic shape for anchoring stuff to the sky box
	void renderSkyZoneAnchor(ASkyZoneInfo* zone, const FVector* location);
	// Given a set of verts and textures, render them with the actor matrix.
	void renderSurfaceBuckets(const ActorRenderData& renderData, FTime currentTime);

	void fillHashTexture(FTexConvertCtx convertContext, FTextureInfo& tex);
	bool shouldGenHashTexture(const FTextureInfo& tex);

	void executeBufferedTileDraws() {
		bool wasBuffered = bufferTileDraws;
		bufferTileDraws = false;
		for (TileFuncCall& call : bufferedTileDraws) {
			call(this);
		}
		bufferedTileDraws.clear();
		bufferTileDraws = wasBuffered;
	}

	// Sets up the projections ready for drawing in the world
	void startWorldDraw(FSceneNode* frame);
	// Sets up the projections ready for drawing UI elements
	void endWorldDraw(FSceneNode* frame);

	void setProjection(float aspect, float fovAngle);
	void setCompatMatrix(FSceneNode* frame);
	void setIdentityMatrix();

	__forceinline void renderCoord(FSceneNode* frame, const FCoords& coord, float scale = 10.0f) {
		setCompatMatrix(frame);
		Draw3DLine(frame, FPlane(1, 0, 0, 1), 0, coord.Origin, coord.Origin + coord.XAxis * scale);
		Draw3DLine(frame, FPlane(0, 1, 0, 1), 0, coord.Origin, coord.Origin + coord.YAxis * scale);
		Draw3DLine(frame, FPlane(0, 0, 1, 1), 0, coord.Origin, coord.Origin + coord.ZAxis * scale);
	}
};

#if __STATIC_LINK

/* No native execs. */

#define AUTO_INITIALIZE_REGISTRANTS_D3D9DRV \
	UD3D9RenderDevice::StaticClass();

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
