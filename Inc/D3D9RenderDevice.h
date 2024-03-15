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


//Make sure valid build config selected
#undef UTGLR_VALID_BUILD_CONFIG

//#define D3D9_DEBUG

#if defined(UNREAL_TOURNAMENT)
	#define UTGLR_UT_BUILD 1
	#define UTGLR_VALID_BUILD_CONFIG 1
		#if UNREAL_TOURNAMENT_OLDUNREAL
			#define UTGLR_USES_ALPHABLEND 1
		#endif
#elif defined(UTGLR_DX_BUILD)
	#define UTGLR_VALID_BUILD_CONFIG 1
#elif defined(UTGLR_RUNE_BUILD)
	#define UTGLR_VALID_BUILD_CONFIG 1
	#define UTGLR_USES_ALPHABLEND 1
#elif defined(UTGLR_UNREAL_227_BUILD)
	#define UTGLR_VALID_BUILD_CONFIG 1
	#define UTGLR_USES_ALPHABLEND 1
#else
	#define UTGLR_VALID_BUILD_CONFIG 0
#endif

#if !UTGLR_VALID_BUILD_CONFIG
#error Valid build config not selected.
#endif
#undef UTGLR_VALID_BUILD_CONFIG

#include "D3D9DebugUtils.h"

#include <math.h>
#include <stdio.h>

#include <unordered_map>
#include <unordered_set>
#include <map>
#include <deque>
#pragma warning(disable : 4018)
#include <vector>

#pragma warning(disable : 4245)
#include "c_gclip.h"


//Per-game feature switches
#if defined UNREAL_TOURNAMENT_UTPG
	//The base game was built on Visual Studio 2019 and GCC 4.8
	#if _M_IX86 || _M_X64 || i386 || __x86_64__
		#define UTGLR_X86_SSE2_MINIMUM 1
	#endif
	#define UTGLR_USES_SCENENODEHACK 0
#elif UTGLR_RUNE_BUILD
	#define UTGLR_USES_SCENENODEHACK 1
#else
	#define UTGLR_USES_SCENENODEHACK 1
#endif


#if !UTGLR_USES_ALPHABLEND
	#define UTGLR_USES_ALPHABLEND 0
	#define PF_AlphaBlend 0x020000
#endif

#if !UNREAL_TOURNAMENT_OLDUNREAL
typedef DWORD PTRINT;
#endif

typedef IDirect3D9 * (WINAPI * LPDIRECT3DCREATE9)(UINT SDKVersion);


//Use debug D3D9 DLL
//#define UTD3D9R_USE_DEBUG_D3D9_DLL


//Shader assembly code
//#define UTD3D9R_INCLUDE_SHADER_ASM

#ifdef UTD3D9R_INCLUDE_SHADER_ASM
#include <d3dx9.h>
#endif


#ifdef WIN32

//Optional ASM code
#if !BUILD_64
#define UTGLR_USE_ASM_CODE

//Optional SSE code
#define UTGLR_INCLUDE_SSE_CODE
#endif

#endif

#ifdef UTGLR_INCLUDE_SSE_CODE
#include <xmmintrin.h>
#include <emmintrin.h>
#endif


//Optional opcode patch code
//#define UTGLR_INCLUDE_OPCODE_PATCH_CODE

#ifdef UTGLR_INCLUDE_OPCODE_PATCH_CODE
#include "OpcodePatch.h"
#endif


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
//#define UTGLR_DEBUG_Z_RANGE_HACK_WIREFRAME


#include "c_rbtree.h"


/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

//If exceeds 7, various things will break
#define MAX_TMUNITS			4		// vogel: maximum number of texture mapping units supported

//Must be at least 2000
#define VERTEX_ARRAY_SIZE	75000	// big internal buffer for loading up larger meshes
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

#define CT_MIN_FILTER_POINT					0x00
#define CT_MIN_FILTER_LINEAR				0x01
#define CT_MIN_FILTER_ANISOTROPIC			0x02
#define CT_MIN_FILTER_MASK					0x03

#define CT_MIP_FILTER_NONE					0x00
#define CT_MIP_FILTER_POINT					0x04
#define CT_MIP_FILTER_LINEAR				0x08
#define CT_MIP_FILTER_MASK					0x0C

#define CT_MAG_FILTER_LINEAR_NOT_POINT_BIT	0x10

#define CT_HAS_MIPMAPS_BIT					0x20

#define CT_ADDRESS_CLAMP_NOT_WRAP_BIT		0x40

//Default texture parameters for new D3D texture
const BYTE CT_DEFAULT_TEX_FILTER_PARAMS = CT_MIN_FILTER_POINT | CT_MIP_FILTER_NONE;

struct tex_params_t {
	BYTE filter;
	BYTE reserved1;
	BYTE reserved2;
	BYTE reserved3;
};

//Default texture stage parameters for D3D
const tex_params_t CT_DEFAULT_TEX_PARAMS = { CT_DEFAULT_TEX_FILTER_PARAMS, 0, 0, 0 };

#define DT_NO_SMOOTH_BIT	0x01

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

template <class ClassT> class rbtree_node_pool {
public:
	typedef typename ClassT::node_t node_t;

public:
	rbtree_node_pool() {
		m_pTail = 0;
	}
	~rbtree_node_pool() {
	}

	inline void FASTCALL add(node_t *pNode) {
		pNode->pParent = m_pTail;

		m_pTail = pNode;

		return;
	}

	inline node_t *try_remove(void) {
		node_t *pNode;

		if (m_pTail == 0) {
			return 0;
		}

		pNode = m_pTail;

		m_pTail = pNode->pParent;

		return pNode;
	}

	unsigned int calc_size(void) {
		node_t *pNode;
		unsigned int size;

		pNode = m_pTail;
		size = 0;
		while (pNode != 0) {
			pNode = pNode->pParent;
			size++;
		}

		return size;
	}

private:
	node_t *m_pTail;
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

//Vertex and primary color
struct FGLVertexColor {
	FLOAT x;
	FLOAT y;
	FLOAT z;
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

//Normals
struct FGLNormal {
	FLOAT x;
	FLOAT y;
	FLOAT z;
};

//Tex coords
struct FGLTexCoord {
	FLOAT u;
	FLOAT v;
};

//Secondary color
struct FGLSecondaryColor {
	DWORD specular;
};

struct FGLMapDot {
	FLOAT u;
	FLOAT v;
};


template<>
struct std::hash<FVector> {
	std::size_t operator()(const FVector& t) const {
		std::size_t seed = 0;
		seed ^= std::hash<FLOAT>()(t.X) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<FLOAT>()(t.Y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<FLOAT>()(t.Z) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		return seed;
	}
};

template<>
struct std::hash<FTextureInfo> {
	std::size_t operator()(const FTextureInfo& t) const {
		std::size_t seed = 0;
		seed ^= std::hash<UTexture*>()(t.Texture) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<INT>()(t.LOD) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<FMipmapBase*>()(t.Mips[0]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<INT>()(t.NumMips) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<FVector>()(t.Pan) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<QWORD>()(t.CacheID) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<QWORD>()(t.PaletteCacheID) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<INT>()(t.UClamp) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<INT>()(t.USize) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<FLOAT>()(t.UScale) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<INT>()(t.VClamp) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<INT>()(t.VSize) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<FLOAT>()(t.VScale) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		return seed;
	}
};

bool inline operator==(const FTextureInfo& lhs, const FTextureInfo& rhs) {
	return std::hash<FTextureInfo>()(lhs) == std::hash<FTextureInfo>()(rhs);
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

struct SpecialCoord {
	FCoords coord;
	D3DMATRIX baseMatrix;
	bool exists = false;
	bool enabled = false;
};

#if UNREAL_TOURNAMENT_OLDUNREAL
typedef URenderDeviceOldUnreal469 RENDERDEVICE_SUPER;
#else
typedef URenderDevice RENDERDEVICE_SUPER;
#endif
//
// A D3D9 rendering device attached to a viewport.
//
class UD3D9RenderDevice : public RENDERDEVICE_SUPER {
#if UTGLR_DX_BUILD
	DECLARE_CLASS(UD3D9RenderDevice, URenderDevice, CLASS_Config)
#else
	DECLARE_CLASS(UD3D9RenderDevice, RENDERDEVICE_SUPER, CLASS_Config, D3D9DrvRTX)
#endif

#ifdef UTD3D9R_INCLUDE_SHADER_ASM
	void AssembleShader(void);
#endif

	//Debug bits
	DWORD m_debugBits;
	inline bool FASTCALL DebugBit(DWORD debugBit) {
		return ((m_debugBits & debugBit) != 0);
	}
	enum {
		DEBUG_BIT_BASIC		= 0x00000001,
		DEBUG_BIT_GL_ERROR	= 0x00000002,
		DEBUG_BIT_ANY		= 0xFFFFFFFF
	};

	//Fixed texture cache ids
	#define TEX_CACHE_ID_UNUSED		0xFFFFFFFFFFFFFFFFULL
	#define TEX_CACHE_ID_NO_TEX		0xFFFFFFFF00000010ULL
	#define TEX_CACHE_ID_ALPHA_TEX	0xFFFFFFFF00000020ULL

	//Texture cache id flags
	enum {
		TEX_CACHE_ID_FLAG_MASKED	= 0x1,
		TEX_CACHE_ID_FLAG_16BIT		= 0x2
	};

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
		TEX_TYPE_NORMAL
	};
	#define TEX_FLAG_NO_CLAMP	0x00000001

	struct FTexConvertCtx {
		INT stepBits;
		DWORD texWidthPow2;
		DWORD texHeightPow2;
		const FCachedTexture *pBind;
		D3DLOCKED_RECT lockRect;
	} m_texConvertCtx;

#pragma warning(push)
#pragma warning(disable : 4146)
	inline void * FASTCALL AlignMemPtr(void *ptr, DWORD align) {
		return (void *)(((DWORD)ptr + (align - 1)) & -align);
	}
#pragma warning(pop)
	enum { VERTEX_ARRAY_ALIGN = 64 };	//Must be even multiple of 16B for SSE
	enum { VERTEX_ARRAY_TAIL_PADDING = 72 };	//Must include 8B for half SSE tail

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

	//Vertex declarations
	IDirect3DVertexDeclaration9 *m_oneColorVertexDecl;
	IDirect3DVertexDeclaration9* m_ColorTexVertexDecl;
	IDirect3DVertexDeclaration9 *m_standardNTextureVertexDecl[MAX_TMUNITS];
	IDirect3DVertexDeclaration9 *m_twoColorSingleTextureVertexDecl;

	//Current vertex declaration state tracking
	IDirect3DVertexDeclaration9 *m_curVertexDecl;
	//Vertex and primary color
	FGLVertex m_csVertexArray[VERTEX_ARRAY_SIZE];
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

	FGLMapDot *MapDotArray;
	BYTE m_MapDotArrayMem[(sizeof(FGLMapDot) * VERTEX_ARRAY_SIZE) + VERTEX_ARRAY_ALIGN + VERTEX_ARRAY_TAIL_PADDING];

	//Vertex buffer state flags
	INT m_curVertexBufferPos;
	bool m_vertexColorBufferNeedsDiscard;
	bool m_secondaryColorBufferNeedsDiscard;
	bool m_texCoordBufferNeedsDiscard[MAX_TMUNITS];

	inline void FlushVertexBuffers(void) {
		//dout << L"Vertex buffers flushed" << std::endl;
		m_curVertexBufferPos = 0;
		m_vertexColorBufferNeedsDiscard = true;
		//if (m_d3dTempVertexColorBuffer) {
		//	dout << L"Flush releasing vert buffer of size " << m_vertexTempBufferSize << std::endl;
		//	m_d3dTempVertexColorBuffer->Release();
		//	m_d3dTempVertexColorBuffer = nullptr;
		//	m_vertexTempBufferSize = 0;
		//}
		m_secondaryColorBufferNeedsDiscard = true;
		for (int u = 0; u < MAX_TMUNITS; u++) {
			m_texCoordBufferNeedsDiscard[u] = true;
			//if (m_d3dTempTexCoordBuffer[u]) {
			//	dout << L"Flush releasing tex buffer of size " << m_vertexTempBufferSize << std::endl;
			//	m_d3dTempTexCoordBuffer[u]->Release();
			//	m_d3dTempTexCoordBuffer[u] = nullptr;
			//	m_texTempBufferSize[u] = 0;
			//}
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
					appErrorf(TEXT("CreateVertexBuffer failed"));
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
		if (FAILED(vertBuffer->Lock(0, 0, (VOID **)&pData, lockFlags))) {
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
		FGLSecondaryColor*pData = nullptr;
		if (FAILED(m_d3dSecondaryColorBuffer->Lock(0, 0, (VOID **)&pData, D3DLOCK_NOSYSLOCK))) {
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
					appErrorf(TEXT("CreateVertexBuffer failed"));
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
		if (FAILED(texBuffer->Lock(0, 0, (VOID **)&pData, lockFlags))) {
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

	//IsNear bits for detail texturing
	DWORD DetailTextureIsNearArray[VERTEX_ARRAY_SIZE / 3];

	DWORD m_csPolyCount;
	INT m_csPtCount;

	FLOAT m_csUDot;
	FLOAT m_csVDot;


	// MultiPass rendering information
	struct FGLRenderPass {
		struct FGLSinglePass {
			FTextureInfo* Info;
			DWORD PolyFlags;
			FLOAT PanBias;
		} TMU[MAX_TMUNITS];
	} MultiPass;				// vogel: MULTIPASS!!! ;)

	//Texture state cache information
	BYTE m_texEnableBits;


	struct FByteGammaRamp {
		BYTE red[256];
		BYTE green[256];
		BYTE blue[256];
	};

#ifdef WIN32
	// Permanent variables.
	HWND m_hWnd;
	HDC m_hDC;
#endif

	UBOOL WasFullscreen;

	bool m_frameRateLimitTimerInitialized;

	bool m_prevSwapBuffersStatus;

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

	DWORD_CTTree_t m_localZeroPrefixBindTrees[NUM_CTTree_TREES], *m_zeroPrefixBindTrees;
	QWORD_CTTree_t m_localNonZeroPrefixBindTrees[NUM_CTTree_TREES], *m_nonZeroPrefixBindTrees;
	CCachedTextureChain m_localNonZeroPrefixBindChain, *m_nonZeroPrefixBindChain;
	TexPoolMap_t m_localRGBA8TexPool, *m_RGBA8TexPool;

	DWORD_CTTree_Allocator_t m_DWORD_CTTree_Allocator;
	QWORD_CTTree_Allocator_t m_QWORD_CTTree_Allocator;
	TexPoolMap_Allocator_t m_TexPoolMap_Allocator;

	QWORD_CTTree_NodePool_t m_nonZeroPrefixNodePool;

	TArray<FPlane> Modes;

	//Use UViewport* in URenderDevice
	//UViewport* Viewport;


	// Timing.
	DWORD BindCycles, ImageCycles, ComplexCycles, GouraudCycles, TileCycles;

	DWORD m_vpEnableCount;
	DWORD m_vpSwitchCount;
	DWORD m_fpEnableCount;
	DWORD m_fpSwitchCount;
	DWORD m_AASwitchCount;
#if UTGLR_USES_SCENENODEHACK
	DWORD m_sceneNodeCount;
	DWORD m_sceneNodeHackCount;
#endif
	DWORD m_vbFlushCount;
	DWORD m_stat0Count;
	DWORD m_stat1Count;


	// Hardware constraints.
	struct {
		UBOOL SinglePassDetail;
	} DCV;

	FLOAT LODBias;
	FLOAT GammaOffset;
	FLOAT GammaOffsetRed;
	FLOAT GammaOffsetGreen;
	FLOAT GammaOffsetBlue;
	INT Brightness;
	UBOOL GammaCorrectScreenshots;
	UBOOL OneXBlending;
	INT MaxLogUOverV;
	INT MaxLogVOverU;
	INT MinLogTextureSize;
	INT MaxLogTextureSize;
	INT MaxAnisotropy;
	INT TMUnits;
	INT MaxTMUnits;
	INT RefreshRate;
	UBOOL UsePrecache;
	UBOOL UseTrilinear;
	UBOOL UseVertexSpecular;
	UBOOL UseS3TC;
	UBOOL Use16BitTextures;
	UBOOL Use565Textures;
	UBOOL NoFiltering;
	INT DetailMax;
	UBOOL UseDetailAlpha;
	UBOOL DetailClipping;
	UBOOL ColorizeDetailTextures;
	UBOOL SinglePassFog;
	UBOOL SinglePassDetail;
	UBOOL UseSSE;
	UBOOL UseSSE2;
	UBOOL UseTexIdPool;
	UBOOL UseTexPool;
	INT DynamicTexIdRecycleLevel;
	UBOOL TexDXT1ToDXT3;
	INT SwapInterval;
	INT FrameRateLimit;
#if defined UTGLR_DX_BUILD || defined UTGLR_RUNE_BUILD
	FLOAT m_prevFrameTimestamp;
#else
	FTime m_prevFrameTimestamp;
#endif
	UBOOL SceneNodeHack;
	UBOOL SmoothMaskedTextures;
	UBOOL MaskedTextureHack;

	UBOOL UseTripleBuffering;
	UBOOL UsePureDevice;
	UBOOL UseSoftwareVertexProcessing;
	UBOOL UseAA;
	INT NumAASamples;
	UBOOL NoAATiles;

	FColor SurfaceSelectionColor;

	UBOOL ZRangeHack;
	bool m_useZRangeHack;
	bool m_nearZRangeHackProjectionActive;
	bool m_requestNearZRangeHackProjection;

	UBOOL BufferActorTris;
	UBOOL BufferClippedActorTris;

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


	//Previous lock variables
	//Used to detect changes in settings
	BITFIELD PL_DetailTextures;
	UBOOL PL_OneXBlending;
	INT PL_MaxLogUOverV;
	INT PL_MaxLogVOverU;
	INT PL_MinLogTextureSize;
	INT PL_MaxLogTextureSize;
	UBOOL PL_NoFiltering;
	UBOOL PL_UseTrilinear;
	UBOOL PL_Use16BitTextures;
	UBOOL PL_Use565Textures;
	UBOOL PL_TexDXT1ToDXT3;
	INT PL_MaxAnisotropy;
	UBOOL PL_SmoothMaskedTextures;
	UBOOL PL_MaskedTextureHack;
	FLOAT PL_LODBias;
	UBOOL PL_UseDetailAlpha;
	UBOOL PL_SinglePassDetail;
	UBOOL PL_UseSSE;
	UBOOL PL_UseSSE2;

	bool m_setGammaRampSucceeded;
	FLOAT SavedGammaCorrection;

	DWORD m_numDepthBits;

	INT AllocatedTextures;

	INT m_rpPassCount;
	INT m_rpTMUnits;
	bool m_rpForceSingle;
	bool m_rpMasked;
	bool m_rpSetDepthEqual;
	DWORD m_rpColor;

	// Hit info.
	BYTE* m_HitData;
	INT* m_HitSize;
	INT m_HitBufSize;
	INT m_HitCount;
	CGClip m_gclip;


	DWORD m_currentFrameCount;

	// Lock variables.
	FPlane FlashScale, FlashFog;
	FLOAT m_RProjZ, m_Aspect;
	FLOAT m_RFX2, m_RFY2;
	INT m_sceneNodeX, m_sceneNodeY;

	bool m_usingAA;
	bool m_curAAEnable;
	bool m_defAAEnable;
	INT m_initNumAASamples;

	bool m_isATI;
	bool m_isNVIDIA;

	enum {
		PF2_NEAR_Z_RANGE_HACK	= 0x01
	};
	DWORD m_curBlendFlags;
	DWORD m_smoothMaskedTexturesBit;
	bool m_useAlphaToCoverageForMasked;
	bool m_alphaToCoverageEnabled;
	bool m_alphaTestEnabled;
	DWORD m_curPolyFlags;
	DWORD m_curPolyFlags2;
	INT m_bufferActorTrisCutoff;

	DWORD m_detailTextureColor4ub;

	DWORD m_maskedTextureHackMask;

	enum {
		CF_COLOR_ARRAY		= 0x01,
		CF_FOG_MODE			= 0x02,
		CF_NORMAL_ARRAY		= 0x04
	};
	BYTE m_requestedColorFlags;
#if UTGLR_USES_ALPHABLEND
	BYTE m_gpAlpha;
	bool m_gpFogEnabled;
#endif

	FLOAT m_fsBlendInfo[4];

	DWORD m_curTexEnvFlags[MAX_TMUNITS];
	tex_params_t m_curTexStageParams[MAX_TMUNITS];
	FTexInfo TexInfo[MAX_TMUNITS];

	void (FASTCALL *m_pBuffer3BasicVertsProc)(UD3D9RenderDevice *, FTransTexture **);
	void (FASTCALL *m_pBuffer3ColoredVertsProc)(UD3D9RenderDevice *, FTransTexture **);
	void (FASTCALL *m_pBuffer3FoggedVertsProc)(UD3D9RenderDevice *, FTransTexture **);

	void (FASTCALL *m_pBuffer3VertsProc)(UD3D9RenderDevice *, FTransTexture **);

	IDirect3DTexture9 *m_pNoTexObj;
	IDirect3DTexture9 *m_pAlphaTexObj;

	// Static variables.
	static INT NumDevices;
	static INT LockCount;

	static HMODULE hModuleD3d9;
	static LPDIRECT3DCREATE9 pDirect3DCreate9;

	static bool g_gammaFirstTime;
	static bool g_haveOriginalGammaRamp;


	IDirect3D9 *m_d3d9;
	IDirect3DDevice9 *m_d3dDevice;

	INT m_SetRes_NewX;
	INT m_SetRes_NewY;
	INT m_SetRes_NewColorBytes;
	UBOOL m_SetRes_Fullscreen;
	bool m_SetRes_isDeviceReset;

	D3DCAPS9 m_d3dCaps;
	bool m_dxt1TextureCap;
	bool m_dxt3TextureCap;
	bool m_dxt5TextureCap;
	bool m_16BitTextureCap;
	bool m_565TextureCap;
	bool m_alphaTextureCap;
	bool m_supportsAlphaToCoverage;

	D3DPRESENT_PARAMETERS m_d3dpp;
	bool m_doSoftwareVertexInit;


#ifdef BGRA_MAKE
#undef BGRA_MAKE
#endif
	static inline DWORD BGRA_MAKE(BYTE b, BYTE g, BYTE r, BYTE a) {
		return (a << 24) | (r << 16) | (g << 8) | b;
	}


#if defined WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_BGR_A255(const FPlane *pPlane) {
		static FLOAT f255 = 255.0f;
		INT iR, iG, iB;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [f255]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [f255]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [f255]
			fistp [iB]
		}
		return BGRA_MAKE(iB, iG, iR, 255);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_BGR_A255(const FPlane *pPlane) {
		return BGRA_MAKE(
					appRound(pPlane->Z * 255.0f),
					appRound(pPlane->Y * 255.0f),
					appRound(pPlane->X * 255.0f),
					255);
	}
#endif

#if defined WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_BGRClamped_A255(const FPlane *pPlane) {
		static FLOAT f255 = 255.0f;
		INT iR, iG, iB;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [f255]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [f255]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [f255]
			fistp [iB]
		}
		return BGRA_MAKE(Clamp(iB, 0, 255), Clamp(iG, 0, 255), Clamp(iR, 0, 255), 255);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_BGRClamped_A255(const FPlane *pPlane) {
		return BGRA_MAKE(
					Clamp(appRound(pPlane->Z * 255.0f), 0, 255),
					Clamp(appRound(pPlane->Y * 255.0f), 0, 255),
					Clamp(appRound(pPlane->X * 255.0f), 0, 255),
					255);
	}
#endif

#if defined WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_BGR_A0(const FPlane *pPlane) {
		static FLOAT f255 = 255.0f;
		INT iR, iG, iB;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [f255]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [f255]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [f255]
			fistp [iB]
		}
		return BGRA_MAKE(iB, iG, iR, 0);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_BGR_A0(const FPlane *pPlane) {
		return BGRA_MAKE(
					appRound(pPlane->Z * 255.0f),
					appRound(pPlane->Y * 255.0f),
					appRound(pPlane->X * 255.0f),
					0);
	}
#endif

#if defined WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_BGR_Aub(const FPlane *pPlane, BYTE alpha) {
		static FLOAT f255 = 255.0f;
		INT iR, iG, iB;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [f255]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [f255]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [f255]
			fistp [iB]
		}
		return BGRA_MAKE(iB, iG, iR, alpha);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_BGR_Aub(const FPlane *pPlane, BYTE alpha) {
		return BGRA_MAKE(
					appRound(pPlane->Z * 255.0f),
					appRound(pPlane->Y * 255.0f),
					appRound(pPlane->X * 255.0f),
					alpha);
	}
#endif

#if defined WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_BGRA(const FPlane *pPlane) {
		static FLOAT f255 = 255.0f;
		INT iR, iG, iB, iA;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [f255]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [f255]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [f255]
			fistp [iB]
			fld [eax]FPlane.W
			fmul [f255]
			fistp [iA]
		}
		return BGRA_MAKE(iB, iG, iR, iA);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_BGRA(const FPlane *pPlane) {
		return BGRA_MAKE(
					appRound(pPlane->Z * 255.0f),
					appRound(pPlane->Y * 255.0f),
					appRound(pPlane->X * 255.0f),
					appRound(pPlane->W * 255.0f));
	}
#endif

#if defined WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_BGRAClamped(const FPlane *pPlane) {
		static FLOAT f255 = 255.0f;
		INT iR, iG, iB, iA;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [f255]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [f255]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [f255]
			fistp [iB]
			fld [eax]FPlane.W
			fmul [f255]
			fistp [iA]
		}
		return BGRA_MAKE(Clamp(iB, 0, 255), Clamp(iG, 0, 255), Clamp(iR, 0, 255), Clamp(iA, 0, 255));
	}
#else
	static inline DWORD FASTCALL FPlaneTo_BGRAClamped(const FPlane *pPlane) {
		return BGRA_MAKE(
					Clamp(appRound(pPlane->Z * 255.0f), 0, 255),
					Clamp(appRound(pPlane->Y * 255.0f), 0, 255),
					Clamp(appRound(pPlane->X * 255.0f), 0, 255),
					Clamp(appRound(pPlane->W * 255.0f), 0, 255));
	}
#endif

#if defined WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_BGRScaled_A255(const FPlane *pPlane, FLOAT rgbScale) {
		INT iR, iG, iB;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [rgbScale]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [rgbScale]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [rgbScale]
			fistp [iB]
		}
		return BGRA_MAKE(iB, iG, iR, 255);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_BGRScaled_A255(const FPlane *pPlane, FLOAT rgbScale) {
		return BGRA_MAKE(
					appRound(pPlane->Z * rgbScale),
					appRound(pPlane->Y * rgbScale),
					appRound(pPlane->X * rgbScale),
					255);
	}
#endif


	// UObject interface.
	void StaticConstructor();


	// Implementation.
	void FASTCALL SC_AddBoolConfigParam(DWORD BitMaskOffset, const TCHAR *pName, UBOOL &param, ECppProperty EC_CppProperty, INT InOffset, UBOOL defaultValue);
	void FASTCALL SC_AddIntConfigParam(const TCHAR *pName, INT &param, ECppProperty EC_CppProperty, INT InOffset, INT defaultValue);
	void FASTCALL SC_AddFloatConfigParam(const TCHAR *pName, FLOAT &param, ECppProperty EC_CppProperty, INT InOffset, FLOAT defaultValue);

	void FASTCALL DbgPrintInitParam(const TCHAR *pName, INT value);
	void FASTCALL DbgPrintInitParam(const TCHAR *pName, FLOAT value);

#ifdef UTGLR_INCLUDE_SSE_CODE
	static bool CPU_DetectCPUID(void);
	static bool CPU_DetectSSE(void);
	static bool CPU_DetectSSE2(void);
#endif //UTGLR_INCLUDE_SSE_CODE

	void InitFrameRateLimitTimerSafe(void);
	void ShutdownFrameRateLimitTimer(void);

	void BuildGammaRamp(float redGamma, float greenGamma, float blueGamma, int brightness, D3DGAMMARAMP &ramp);
	void BuildGammaRamp(float redGamma, float greenGamma, float blueGamma, int brightness, FByteGammaRamp &ramp);
	void SetGamma(FLOAT GammaCorrection);
	void ResetGamma(void);

	UBOOL FailedInitf(const TCHAR* Fmt, ...);
	void Exit();
	void ShutdownAfterError();

	UBOOL SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);
	void UnsetRes();
	UBOOL ResetDevice();

	bool FASTCALL CheckDepthFormat(D3DFORMAT adapterFormat, D3DFORMAT backBufferFormat, D3DFORMAT depthBufferFormat);

	void ConfigValidate_RefreshDCV(void);
	void ConfigValidate_RequiredExtensions(void);
	void ConfigValidate_Main(void);

	void InitPermanentResourcesAndRenderingState(void);
	void FreePermanentResources(void);


	UBOOL Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);

	static QSORT_RETURN CDECL CompareRes(const FPlane* A, const FPlane* B) {
		return (QSORT_RETURN) (((A->X - B->X) != 0.0f) ? (A->X - B->X) : (A->Y - B->Y));
	}

	UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar);
	void Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize);
	void SetSceneNode(FSceneNode* Frame);
	void Unlock(UBOOL Blit);
	void Flush(UBOOL AllowPrecache);

	void DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet);
	// Takes a list of faces and draws them in batches
	void drawLevelSurfaces(FSceneNode* frame, FSurfaceInfo& surface, std::vector<FSurfaceFacet>& facets);
#ifdef UTGLR_RUNE_BUILD
	void PreDrawFogSurface();
	void PostDrawFogSurface();
	void DrawFogSurface(FSceneNode* Frame, FFogSurf &FogSurf);
	void PreDrawGouraud(FSceneNode* Frame, FLOAT FogDistance, FPlane FogColor);
	void PostDrawGouraud(FLOAT FogDistance);
#endif
	void DrawGouraudPolygonOld(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span);
	void DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span);
	void DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags);
	void Draw3DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2);
	void Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2);
	void Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z);

	// Render a sprite actor
	void renderSprite(FSceneNode* frame, AActor* actor);
	// Renders a sprite at the given location
	void renderSpriteGeo(FSceneNode* frame, const FVector& location, FLOAT drawScale, FTextureInfo& texInfo, DWORD basePolyFlags, FPlane color);
	// Renders a mesh actor
	void renderMeshActor(FSceneNode* frame, AActor* actor, SpecialCoord* specialCoord = nullptr);
	// Renders a mover brush
	void renderMover(FSceneNode* frame, AMover* mover);
	// Updates and sends the given lights to dx
	void renderLights(std::vector<AActor*> lightActors);

	void ClearZ(FSceneNode* Frame);
	void PushHit(const BYTE* Data, INT Count);
	void PopHit(INT Count, UBOOL bForce);
	void GetStats(TCHAR* Result);
	void ReadPixels(FColor* Pixels);
	void EndFlash();
	void PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags);
#if UNREAL_TOURNAMENT_OLDUNREAL
	UBOOL SupportsTextureFormat(ETextureFormat Format);
#endif

	void InitNoTextureSafe(void);
	void InitAlphaTextureSafe(void);

	void ScanForOldTextures(void);

	inline void FASTCALL SetNoTexture(INT Multi) {
		if (TexInfo[Multi].CurrentCacheID != TEX_CACHE_ID_NO_TEX) {
			SetNoTextureNoCheck(Multi);
		}
	}
	inline void SetAlphaTexture(INT Multi) {
		if (TexInfo[Multi].CurrentCacheID != TEX_CACHE_ID_ALPHA_TEX) {
			SetAlphaTextureNoCheck(Multi);
		}
	}

	void FASTCALL SetNoTextureNoCheck(INT Multi);
	void FASTCALL SetAlphaTextureNoCheck(INT Multi);

	inline void FASTCALL SetTexture(INT Multi, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias) {
		FTexInfo& Tex = TexInfo[Multi];
		QWORD CacheID;
		DWORD DynamicPolyFlags;

		// Set panning.
		Tex.UPan = Info.Pan.X + (PanBias * Info.UScale);
		Tex.VPan = Info.Pan.Y + (PanBias * Info.VScale);

		//PF_Memorized used internally to indicate 16-bit texture
		PolyFlags &= ~PF_Memorized;

		//Load texture cache id
		CacheID = Info.CacheID;

		//Only attempt to alter texture cache id on certain textures
		if ((CacheID & 0xFF) == 0xE0) {
			//Alter texture cache id if masked texture hack is enabled and texture is masked
			CacheID |= ((PolyFlags & PF_Masked) ? TEX_CACHE_ID_FLAG_MASKED : 0) & m_maskedTextureHackMask;

			//Check for 16 bit texture option
			if (Use16BitTextures) {
				if (Info.Palette && (Info.Palette[128].A == 255)) {
					CacheID |= TEX_CACHE_ID_FLAG_16BIT;
					PolyFlags |= PF_Memorized;
				}
			}
		}

		//Get dynamic poly flags
		DynamicPolyFlags = PolyFlags & TEX_DYNAMIC_POLY_FLAGS_MASK;

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
		QWORD CacheID;
		DWORD DynamicPolyFlags;

		// Set panning.
		Tex.UPan = Info.Pan.X;
		Tex.VPan = Info.Pan.Y;

		//PF_Memorized used internally to indicate 16-bit texture
		PolyFlags &= ~PF_Memorized;

		//Load texture cache id
		CacheID = Info.CacheID;

		//Only attempt to alter texture cache id on certain textures
		if ((CacheID & 0xFF) == 0xE0) {
			//Alter texture cache id if masked texture hack is enabled and texture is masked
			CacheID |= ((PolyFlags & PF_Masked) ? TEX_CACHE_ID_FLAG_MASKED : 0) & m_maskedTextureHackMask;

			//Check for 16 bit texture option
			if (Use16BitTextures) {
				if (Info.Palette && (Info.Palette[128].A == 255)) {
					CacheID |= TEX_CACHE_ID_FLAG_16BIT;
					PolyFlags |= PF_Memorized;
				}
			}
		}

		//Get dynamic poly flags
		DynamicPolyFlags = PolyFlags & TEX_DYNAMIC_POLY_FLAGS_MASK;

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

	inline void FASTCALL SetBlend(DWORD PolyFlags) {
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
			SetBlendNoCheck(blendFlags);
		}
	}
	void EnableAlphaToCoverageNoCheck(void);
	void DisableAlphaToCoverageNoCheck(void);
	void FASTCALL SetBlendNoCheck(DWORD blendFlags);

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
	inline void SetDefaultProjectionState(void) {
		//See if non-default projection is active
		if (m_nearZRangeHackProjectionActive) {
			SetProjectionStateNoCheck(false);
		}
	}

	inline void FASTCALL SetProjectionState(bool requestNearZRangeHackProjection) {
		if (requestNearZRangeHackProjection != m_nearZRangeHackProjectionActive) {
			//Set requested projection state
			SetProjectionStateNoCheck(requestNearZRangeHackProjection);
		}
	}

	inline void SetDefaultAAState(void) {
		if (m_defAAEnable != m_curAAEnable) {
			SetAAStateNoCheck(m_defAAEnable);
		}
	}
	inline void SetDisabledAAState(void) {
		if (m_curAAEnable && m_usingAA) {
			SetAAStateNoCheck(false);
		}
	}
	void FASTCALL SetAAStateNoCheck(bool AAEnable);

	void FASTCALL SetProjectionStateNoCheck(bool);
	void SetOrthoProjection(void);

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
	void FASTCALL RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture(FTextureInfo &);

	INT FASTCALL BufferStaticComplexSurfaceGeometry(const FSurfaceFacet& Facet, bool append = false);
	INT FASTCALL BufferTriangleSurfaceGeometry(const std::vector<FTransTexture>& vertices);

	void FASTCALL BufferAdditionalClippedVerts(FTransTexture** Pts, INT NumPts);

	// Sets up the projections ready for drawing in the world
	void startWorldDraw(FSceneNode* frame);
	// Sets up the projections ready for drawing UI elements
	void endWorldDraw(FSceneNode* frame);
};

#if __STATIC_LINK

/* No native execs. */

#define AUTO_INITIALIZE_REGISTRANTS_D3D9DRV \
	UD3D9RenderDevice::StaticClass();

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
