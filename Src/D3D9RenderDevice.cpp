/*=============================================================================
	D3D9.cpp: Unreal Direct3D9 support implementation for Windows.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

	OpenGL renderer by Daniel Vogel <vogel@lokigames.com>
	Loki Software, Inc.

	Other URenderDevice subclasses include:
	* USoftwareRenderDevice: Software renderer.
	* UGlideRenderDevice: 3dfx Glide renderer.
	* UDirect3DRenderDevice: Direct3D renderer.
	* UD3D9RenderDevice: Direct3D9 renderer.

	Revision history:
	* Created by Daniel Vogel based on XMesaGLDrv
	* Changes (John Fulmer, Jeroen Janssen)
	* Major changes (Daniel Vogel)
	* Ported back to Win32 (Fredrik Gustafsson)
	* Unification and addition of vertex arrays (Daniel Vogel)
	* Actor triangle caching (Steve Sinclair)
	* One pass fogging (Daniel Vogel)
	* Windows gamma support (Daniel Vogel)
	* 2X blending support (Daniel Vogel)
	* Better detail texture handling (Daniel Vogel)
	* Scaleability (Daniel Vogel)
	* Texture LOD bias (Daniel Vogel)
	* RefreshRate support on Windows (Jason Dick)
	* Finer control over gamma (Daniel Vogel)
	* (NOT ALWAYS) Fixed Windows bitdepth switching (Daniel Vogel)

	* Various modifications and additions by Chris Dohnal
	* Initial TruForm based on TruForm renderer modifications by NitroGL
	* Additional TruForm and Deus Ex updates by Leonhard Gruenschloss
	* Various modifications and additions by Smirftsch / Oldunreal


	UseTrilinear	whether to use trilinear filtering
	UseS3TC			whether to use compressed textures
	MaxAnisotropy	maximum level of anisotropy used
	MaxTMUnits		maximum number of TMUs UT will try to use
	LODBias			texture lod bias
	RefreshRate		requested refresh rate (Windows only)
	GammaOffset		offset for the gamma correction


TODO:
	- DOCUMENTATION!!! (especially all subtle assumptions)

=============================================================================*/

#include "D3D9RenderDevice.h"
#include <DirectXMath.h>

#ifdef WIN32
#include <mmsystem.h>
#endif


/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

#ifdef UTD3D9R_USE_DEBUG_D3D9_DLL
static const char *g_d3d9DllName = "d3d9d.dll";
#else
static const char *g_d3d9DllName = "d3d9.dll";
#endif

#if UTGLR_DEFINE_HACK_FLAGS
DWORD GUglyHackFlags;
#endif

static const TCHAR *g_pSection = TEXT("D3D9DrvRTX.D3D9RenderDevice");

//Stream definitions
////////////////////

static const D3DVERTEXELEMENT9 g_oneColorStreamDef[] = {
	{ 0, 0,  D3DDECLTYPE_FLOAT3,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,	0 },
	{ 0, 12, D3DDECLTYPE_D3DCOLOR,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,		0 },
	D3DDECL_END()
};

static const D3DVERTEXELEMENT9 g_colorTexStreamDef[] = {
	{ 0, 0,  D3DDECLTYPE_FLOAT3,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,	0 },
	{ 0, 12, D3DDECLTYPE_D3DCOLOR,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,		0 },
	{ 0, 16,  D3DDECLTYPE_FLOAT2,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,	0 },
	D3DDECL_END()
};

static const D3DVERTEXELEMENT9 g_standardSingleTextureStreamDef[] = {
	{ 0, 0,  D3DDECLTYPE_FLOAT3,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,	0 },
	{ 0, 12, D3DDECLTYPE_FLOAT3,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,		0 },
	{ 0, 24, D3DDECLTYPE_D3DCOLOR,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,		0 },
	{ 2, 0,  D3DDECLTYPE_FLOAT2,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,	0 },
	D3DDECL_END()
};

static const D3DVERTEXELEMENT9 g_standardDoubleTextureStreamDef[] = {
	{ 0, 0,  D3DDECLTYPE_FLOAT3,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,	0 },
	{ 0, 12, D3DDECLTYPE_D3DCOLOR,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,		0 },
	{ 2, 0,  D3DDECLTYPE_FLOAT2,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,	0 },
	{ 3, 0,  D3DDECLTYPE_FLOAT2,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,	1 },
	D3DDECL_END()
};

static const D3DVERTEXELEMENT9 g_standardTripleTextureStreamDef[] = {
	{ 0, 0,  D3DDECLTYPE_FLOAT3,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,	0 },
	{ 0, 12, D3DDECLTYPE_D3DCOLOR,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,		0 },
	{ 2, 0,  D3DDECLTYPE_FLOAT2,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,	0 },
	{ 3, 0,  D3DDECLTYPE_FLOAT2,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,	1 },
	{ 4, 0,  D3DDECLTYPE_FLOAT2,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,	2 },
	D3DDECL_END()
};

static const D3DVERTEXELEMENT9 g_standardQuadTextureStreamDef[] = {
	{ 0, 0,  D3DDECLTYPE_FLOAT3,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,	0 },
	{ 0, 12, D3DDECLTYPE_D3DCOLOR,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,		0 },
	{ 2, 0,  D3DDECLTYPE_FLOAT2,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,	0 },
	{ 3, 0,  D3DDECLTYPE_FLOAT2,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,	1 },
	{ 4, 0,  D3DDECLTYPE_FLOAT2,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,	2 },
	{ 5, 0,  D3DDECLTYPE_FLOAT2,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,	3 },
	D3DDECL_END()
};

static const D3DVERTEXELEMENT9 *g_standardNTextureStreamDefs[MAX_TMUNITS] = {
	g_standardSingleTextureStreamDef,
	g_standardDoubleTextureStreamDef,
	g_standardTripleTextureStreamDef,
	g_standardQuadTextureStreamDef
};

static const D3DVERTEXELEMENT9 g_twoColorSingleTextureStreamDef[] = {
	{ 0, 0,  D3DDECLTYPE_FLOAT3,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,	0 },
	{ 0, 12, D3DDECLTYPE_D3DCOLOR,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,		0 },
	{ 1, 0,  D3DDECLTYPE_D3DCOLOR,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,		1 },
	{ 2, 0,  D3DDECLTYPE_FLOAT2,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,	0 },
	D3DDECL_END()
};

// Class to hold multiple unique T objects, accessable by multiple indices
template <typename T>
class UniqueValueArray {
public:
	bool insert(int index, T value) {
		auto result = unique_values.insert(value);
		value_map[index] = const_cast<T*>(&(*result.first));
		return result.second;
	}

	T& at(int index) {
		auto it = value_map.find(index);
		if (it == value_map.end()) {
			throw std::out_of_range("Invalid index");
		} else {
			return *it->second;
		}
	}

	bool has(int index) {
		return value_map.count(index) > 0;
	}

	int getIndex(T value) {
		for (auto pair : value_map) {
			if (*pair.second == value) {
				return pair.first;
			}
		}
		return -1;
	}

	auto begin() {
		return unique_values.begin();
	}

	auto end() {
		return unique_values.end();
	}

private:
	std::unordered_map<int, T*> value_map;
	std::unordered_set<T> unique_values;
};

static D3DCOLORVALUE hsvToRgb(unsigned char h, unsigned char s, unsigned char v) {
	float H = h / 255.0f;
	float S = s / 255.0f;
	float V = v / 255.0f;

	float C = V * S;
	float X = C * (1 - std::abs(fmod(H * 6, 2) - 1));
	float m = V - C;

	D3DCOLORVALUE rgbColor;

	if (0 <= H && H < 1 / 6.0) {
		rgbColor.r = C; rgbColor.g = X; rgbColor.b = 0;
	} else if (1 / 6.0 <= H && H < 2 / 6.0) {
		rgbColor.r = X; rgbColor.g = C; rgbColor.b = 0;
	} else if (2 / 6.0 <= H && H < 3 / 6.0) {
		rgbColor.r = 0; rgbColor.g = C; rgbColor.b = X;
	} else if (3 / 6.0 <= H && H < 4 / 6.0) {
		rgbColor.r = 0; rgbColor.g = X; rgbColor.b = C;
	} else if (4 / 6.0 <= H && H < 5 / 6.0) {
		rgbColor.r = X; rgbColor.g = 0; rgbColor.b = C;
	} else {
		rgbColor.r = C; rgbColor.g = 0; rgbColor.b = X;
	}

	rgbColor.r += m;
	rgbColor.g += m;
	rgbColor.b += m;

	// Set alpha value to maximum
	rgbColor.a = 1.0f;

	return rgbColor;
}

static inline DirectX::XMMATRIX ToXMMATRIX(const D3DMATRIX& d3dMatrix) {
	D3DMATRIX temp = d3dMatrix;
	return reinterpret_cast<DirectX::XMMATRIX&>(temp);
}

static inline D3DMATRIX ToD3DMATRIX(const DirectX::XMMATRIX& xMMatrix) {
	DirectX::XMMATRIX temp = xMMatrix;
	return reinterpret_cast<D3DMATRIX&>(temp);
}

static const D3DMATRIX identityMatrix = ToD3DMATRIX(DirectX::XMMatrixIdentity());

static inline DirectX::XMVECTOR FVecToDXVec(const FVector& vec) {
	return DirectX::XMVectorSet(vec.X, vec.Y, vec.Z, 0.0f);
}

static inline FVector DXVecToFVec(DirectX::XMVECTOR& vec) {
	using namespace DirectX;
	return FVector(XMVectorGetX(vec), XMVectorGetY(vec), XMVectorGetZ(vec));
}

static DirectX::XMMATRIX FCoordToDXMat(const FCoords& coord) {
	using namespace DirectX;
	XMMATRIX mat = {
		coord.XAxis.X, coord.YAxis.X, coord.ZAxis.X, 0,
		coord.XAxis.Y, coord.YAxis.Y, coord.ZAxis.Y, 0,
		coord.XAxis.Z, coord.YAxis.Z, coord.ZAxis.Z, 0,
		coord.Origin.X, coord.Origin.Y, coord.Origin.Z, 1.0f
	};
	return mat;
}

static DirectX::XMMATRIX FRotToDXRotMat(const FRotator& rot) {
	using namespace DirectX;
	FCoords coords = GMath.UnitCoords;
	coords *= rot;
	return FCoordToDXMat(coords);
}

//#define UTGLR_DEBUG_SHOW_CALL_COUNTS

/*-----------------------------------------------------------------------------
	D3D9Drv.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UD3D9RenderDevice);


void UD3D9RenderDevice::StaticConstructor() {
	unsigned int u;

	guard(UD3D9RenderDevice::StaticConstructor);

#ifdef DEUS_EX
	const UBOOL UTGLR_DEFAULT_OneXBlending = 1;
#else
	const UBOOL UTGLR_DEFAULT_OneXBlending = 0;
#endif

#if defined DEUS_EX || defined RUNE
	const UBOOL UTGLR_DEFAULT_UseS3TC = 0;
#else
	const UBOOL UTGLR_DEFAULT_UseS3TC = 1;
#endif

#define CPP_PROPERTY_LOCAL(_name) _name, CPP_PROPERTY(_name)

	//Set parameter defaults and add parameters
	SC_AddFloatConfigParam(TEXT("LODBias"), CPP_PROPERTY_LOCAL(LODBias), 0.0f);
	SC_AddBoolConfigParam(0,  TEXT("OneXBlending"), CPP_PROPERTY_LOCAL(OneXBlending), UTGLR_DEFAULT_OneXBlending);
	SC_AddIntConfigParam(TEXT("MinLogTextureSize"), CPP_PROPERTY_LOCAL(MinLogTextureSize), 0);
	SC_AddIntConfigParam(TEXT("MaxLogTextureSize"), CPP_PROPERTY_LOCAL(MaxLogTextureSize), 8);
	SC_AddBoolConfigParam(2,  TEXT("UsePrecache"), CPP_PROPERTY_LOCAL(UsePrecache), 0);
	SC_AddBoolConfigParam(1,  TEXT("UseTrilinear"), CPP_PROPERTY_LOCAL(UseTrilinear), 0);
	SC_AddBoolConfigParam(0,  TEXT("UseS3TC"), CPP_PROPERTY_LOCAL(UseS3TC), UTGLR_DEFAULT_UseS3TC);
	SC_AddIntConfigParam(TEXT("MaxAnisotropy"), CPP_PROPERTY_LOCAL(MaxAnisotropy), 0);
	SC_AddIntConfigParam(TEXT("RefreshRate"), CPP_PROPERTY_LOCAL(RefreshRate), 0);
	SC_AddBoolConfigParam(3,  TEXT("NoFiltering"), CPP_PROPERTY_LOCAL(NoFiltering), 0);
	SC_AddBoolConfigParam(2,  TEXT("SinglePassFog"), CPP_PROPERTY_LOCAL(SinglePassFog), 1);
	SC_AddBoolConfigParam(1,  TEXT("UseTexIdPool"), CPP_PROPERTY_LOCAL(UseTexIdPool), 1);
	SC_AddBoolConfigParam(0,  TEXT("UseTexPool"), CPP_PROPERTY_LOCAL(UseTexPool), 1);
	SC_AddIntConfigParam(TEXT("DynamicTexIdRecycleLevel"), CPP_PROPERTY_LOCAL(DynamicTexIdRecycleLevel), 100);
	SC_AddBoolConfigParam(0,  TEXT("TexDXT1ToDXT3"), CPP_PROPERTY_LOCAL(TexDXT1ToDXT3), 0);
	SC_AddIntConfigParam(TEXT("FrameRateLimit"), CPP_PROPERTY_LOCAL(FrameRateLimit), 0);
	SC_AddBoolConfigParam(2,  TEXT("SmoothMaskedTextures"), CPP_PROPERTY_LOCAL(SmoothMaskedTextures), 0);
	SC_AddBoolConfigParam(1, TEXT("EnableSkyBoxAnchors"), CPP_PROPERTY_LOCAL(EnableSkyBoxAnchors), 1);
	SC_AddBoolConfigParam(0, TEXT("EnableHashTextures"), CPP_PROPERTY_LOCAL(EnableHashTextures), 1);

	SurfaceSelectionColor = FColor(0, 0, 31, 31);
	//new(GetClass(), TEXT("SurfaceSelectionColor"), RF_Public)UStructProperty(CPP_PROPERTY(SurfaceSelectionColor), TEXT("Options"), CPF_Config, FindObjectChecked<UStruct>(NULL, TEXT("Core.Object.Color"), 1));
	UStruct* ColorStruct = FindObject<UStruct>(UObject::StaticClass(), TEXT("Color"));
	if (!ColorStruct)
		ColorStruct = new(UObject::StaticClass(), TEXT("Color")) UStruct(NULL);
	ColorStruct->PropertiesSize = sizeof(FColor);
	new(GetClass(), TEXT("SurfaceSelectionColor"), RF_Public)UStructProperty(CPP_PROPERTY(SurfaceSelectionColor), TEXT("Options"), CPF_Config, ColorStruct);

#undef CPP_PROPERTY_LOCAL

	//Driver flags
	SpanBased				= 0;
	SupportsFogMaps			= 1;
#ifdef RUNE
	SupportsDistanceFog		= 1;
#else
	SupportsDistanceFog		= 0;
#endif
	FullscreenOnly			= 0;

	SupportsLazyTextures	= 0;
	PrefersDeferredLoad		= 0;

	//Mark device pointers as invalid
	m_d3d9 = NULL;
	m_d3dDevice = NULL;

	//Invalidate fixed texture ids
	m_pNoTexObj = NULL;

	//Mark all vertex buffer objects as invalid
	m_d3dVertexColorBuffer = NULL;
	m_d3dSecondaryColorBuffer = NULL;
	for (u = 0; u < MAX_TMUNITS; u++) {
		m_d3dTexCoordBuffer[u] = NULL;
	}

	//Mark all vertex declarations as not created
	m_oneColorVertexDecl = NULL;
	for (u = 0; u < MAX_TMUNITS; u++) {
		m_standardNTextureVertexDecl[u] = NULL;
	}
	m_twoColorSingleTextureVertexDecl = NULL;

	//Reset TMUnits in case resource cleanup code is ever called before this is initialized
	TMUnits = 0;

	//Clear the SetRes is device reset flag
	m_SetRes_isDeviceReset = false;

	//Frame rate limit timer not yet initialized
	m_frameRateLimitTimerInitialized = false;

	HighDetailActors = true;

#if !UNREAL_GOLD
	DescFlags |= RDDESCF_Certified;
#endif

	unguard;
}


void UD3D9RenderDevice::SC_AddBoolConfigParam(DWORD BitMaskOffset, const TCHAR *pName, UBOOL &param, ECppProperty EC_CppProperty, INT InOffset, UBOOL defaultValue) {
#if !UNREAL_TOURNAMENT_OLDUNREAL
	param = (((defaultValue) != 0) ? 1 : 0) << BitMaskOffset; //Doesn't exactly work like a UBOOL "// Boolean 0 (false) or 1 (true)."
#else
	// stijn: we no longer need the manual bitmask shifting in patch 469
	param = defaultValue;
#endif
	new(GetClass(), pName, RF_Public)UBoolProperty(EC_CppProperty, InOffset, TEXT("Options"), CPF_Config);
}

void UD3D9RenderDevice::SC_AddIntConfigParam(const TCHAR *pName, INT &param, ECppProperty EC_CppProperty, INT InOffset, INT defaultValue) {
	param = defaultValue;
	new(GetClass(), pName, RF_Public)UIntProperty(EC_CppProperty, InOffset, TEXT("Options"), CPF_Config);
}

void UD3D9RenderDevice::SC_AddFloatConfigParam(const TCHAR *pName, FLOAT &param, ECppProperty EC_CppProperty, INT InOffset, FLOAT defaultValue) {
	param = defaultValue;
	new(GetClass(), pName, RF_Public)UFloatProperty(EC_CppProperty, InOffset, TEXT("Options"), CPF_Config);
}


void UD3D9RenderDevice::DbgPrintInitParam(const TCHAR *pName, INT value) {
	dout << TEXT("utd3d9r: ") << pName << TEXT(" = ") << value << std::endl;
	return;
}

void UD3D9RenderDevice::DbgPrintInitParam(const TCHAR *pName, FLOAT value) {
	dout << TEXT("utd3d9r: ") << pName << TEXT(" = ") << value << std::endl;
	return;
}


void UD3D9RenderDevice::InitFrameRateLimitTimerSafe(void) {
	//Only initialize once
	if (m_frameRateLimitTimerInitialized) {
		return;
	}
	m_frameRateLimitTimerInitialized = true;

#ifdef WIN32
	//Request high resolution timer
	timeBeginPeriod(1);
#endif

	return;
}

void UD3D9RenderDevice::ShutdownFrameRateLimitTimer(void) {
	//Only shutdown once
	if (!m_frameRateLimitTimerInitialized) {
		return;
	}
	m_frameRateLimitTimerInitialized = false;

#ifdef WIN32
	//Release high resolution timer
	timeEndPeriod(1);
#endif

	return;
}

#if UTGLR_USES_ALPHABLEND
static void FASTCALL Buffer3Verts(UD3D9RenderDevice *pRD, FTransTexture** Pts) {
	FGLTexCoord *pTexCoordArray = &pRD->m_pTexCoordArray[0][pRD->m_bufferedVerts];
	FGLVertexColor *pVertexColorArray = &pRD->m_pVertexColorArray[pRD->m_bufferedVerts];
	FGLSecondaryColor *pSecondaryColorArray = &pRD->m_pSecondaryColorArray[pRD->m_bufferedVerts];
	pRD->m_bufferedVerts += 3;
	for (INT i = 0; i < 3; i++) {
		const FTransTexture* P = *Pts++;

		pTexCoordArray->u = P->U * pRD->TexInfo[0].UMult;
		pTexCoordArray->v = P->V * pRD->TexInfo[0].VMult;
		pTexCoordArray++;

		pVertexColorArray->x = P->Point.X;
		pVertexColorArray->y = P->Point.Y;
		pVertexColorArray->z = P->Point.Z;
		if (pRD->m_requestedColorFlags & UD3D9RenderDevice::CF_FOG_MODE) {
			FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - P->Fog.W);

			pVertexColorArray->color = UD3D9RenderDevice::FPlaneTo_BGRScaled_A255(&P->Light, f255_Times_One_Minus_FogW);

			pSecondaryColorArray->specular = UD3D9RenderDevice::FPlaneTo_BGR_A0(&P->Fog);
			pSecondaryColorArray++;
		}
		else if (pRD->m_requestedColorFlags & UD3D9RenderDevice::CF_COLOR_ARRAY) {
			if (pRD->m_gpAlpha > 0)
				pVertexColorArray->color = UD3D9RenderDevice::FPlaneTo_BGR_Aub(&P->Light, pRD->m_gpAlpha);
			else
				pVertexColorArray->color = UD3D9RenderDevice::FPlaneTo_BGR_A255(&P->Light);
		}
		else {
			pVertexColorArray->color = 0xFFFFFFFF;
		}
		pVertexColorArray++;
	}
}
#endif

static void FASTCALL Buffer3BasicVerts(UD3D9RenderDevice *pRD, FTransTexture** Pts) {
	FGLTexCoord *pTexCoordArray = &pRD->m_pTexCoordArray[0][pRD->m_bufferedVerts];
	FGLVertexColor *pVertexColorArray = &pRD->m_pVertexColorArray[pRD->m_bufferedVerts];
	pRD->m_bufferedVerts += 3;
	FLOAT UMult = pRD->TexInfo[0].UMult;
	FLOAT VMult = pRD->TexInfo[0].VMult;
	for (INT i = 0; i < 3; i++) {
		const FTransTexture* P = *Pts++;

		pTexCoordArray->u = P->U * UMult;
		pTexCoordArray->v = P->V * VMult;
		pTexCoordArray++;

		pVertexColorArray->x = P->Point.X;
		pVertexColorArray->y = P->Point.Y;
		pVertexColorArray->z = P->Point.Z;
		pVertexColorArray->color = 0xFFFFFFFF;
		pVertexColorArray++;
	}
}

static void FASTCALL Buffer3ColoredVerts(UD3D9RenderDevice *pRD, FTransTexture** Pts) {
	FGLTexCoord *pTexCoordArray = &pRD->m_pTexCoordArray[0][pRD->m_bufferedVerts];
	FGLVertexColor *pVertexColorArray = &pRD->m_pVertexColorArray[pRD->m_bufferedVerts];
	pRD->m_bufferedVerts += 3;
	for (INT i = 0; i < 3; i++) {
		const FTransTexture* P = *Pts++;

		pTexCoordArray->u = P->U * pRD->TexInfo[0].UMult;
		pTexCoordArray->v = P->V * pRD->TexInfo[0].VMult;
		pTexCoordArray++;

		pVertexColorArray->x = P->Point.X;
		pVertexColorArray->y = P->Point.Y;
		pVertexColorArray->z = P->Point.Z;
		// stijn: needed clamping in 64-bit because Actors with AmbientGlow==0 often had RGBA values above 1
		pVertexColorArray->color = UD3D9RenderDevice::FPlaneTo_BGRClamped_A255(&P->Light);
		//pVertexColorArray->color = UD3D9RenderDevice::FPlaneTo_BGR_A255(&P->Light);
		pVertexColorArray++;
	}
}

static void FASTCALL Buffer3FoggedVerts(UD3D9RenderDevice *pRD, FTransTexture** Pts) {
	FGLTexCoord *pTexCoordArray = &pRD->m_pTexCoordArray[0][pRD->m_bufferedVerts];
	FGLVertexColor *pVertexColorArray = &pRD->m_pVertexColorArray[pRD->m_bufferedVerts];
	FGLSecondaryColor *pSecondaryColorArray = &pRD->m_pSecondaryColorArray[pRD->m_bufferedVerts];
	pRD->m_bufferedVerts += 3;
	for (INT i = 0; i < 3; i++) {
		const FTransTexture* P = *Pts++;

		pTexCoordArray->u = P->U * pRD->TexInfo[0].UMult;
		pTexCoordArray->v = P->V * pRD->TexInfo[0].VMult;
		pTexCoordArray++;

		pVertexColorArray->x = P->Point.X;
		pVertexColorArray->y = P->Point.Y;
		pVertexColorArray->z = P->Point.Z;
		FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - P->Fog.W);
		pVertexColorArray->color = UD3D9RenderDevice::FPlaneTo_BGRScaled_A255(&P->Light, f255_Times_One_Minus_FogW);
		pVertexColorArray++;

		pSecondaryColorArray->specular = UD3D9RenderDevice::FPlaneTo_BGR_A0(&P->Fog);
		pSecondaryColorArray++;
	}
}

//Must be called with (NumPts > 3)
void UD3D9RenderDevice::BufferAdditionalClippedVerts(FTransTexture** Pts, INT NumPts) {
	INT i;

	i = 3;
	do {
		const FTransTexture* P;
		FGLTexCoord *pTexCoordArray;
		FGLVertexColor *pVertexColorArray;

		P = Pts[0];
		pTexCoordArray = &m_pTexCoordArray[0][m_bufferedVerts];
		pTexCoordArray->u = P->U * TexInfo[0].UMult;
		pTexCoordArray->v = P->V * TexInfo[0].VMult;
		pVertexColorArray = &m_pVertexColorArray[m_bufferedVerts];
		pVertexColorArray->x = P->Point.X;
		pVertexColorArray->y = P->Point.Y;
		pVertexColorArray->z = P->Point.Z;
		if (m_requestedColorFlags & CF_FOG_MODE) {
			FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - P->Fog.W);
			pVertexColorArray->color = FPlaneTo_BGRScaled_A255(&P->Light, f255_Times_One_Minus_FogW);

			m_pSecondaryColorArray[m_bufferedVerts].specular = FPlaneTo_BGR_A0(&P->Fog);
		}
		else if (m_requestedColorFlags & CF_COLOR_ARRAY) {
#ifdef RUNE
			pVertexColorArray->color = FPlaneTo_BGR_Aub(&P->Light, m_gpAlpha);
#else
			pVertexColorArray->color = FPlaneTo_BGRClamped_A255(&P->Light);
#endif
		}
		else {
			pVertexColorArray->color = 0xFFFFFFFF;
		}
		m_bufferedVerts++;

		P = Pts[i - 1];
		pTexCoordArray = &m_pTexCoordArray[0][m_bufferedVerts];
		pTexCoordArray->u = P->U * TexInfo[0].UMult;
		pTexCoordArray->v = P->V * TexInfo[0].VMult;
		pVertexColorArray = &m_pVertexColorArray[m_bufferedVerts];
		pVertexColorArray->x = P->Point.X;
		pVertexColorArray->y = P->Point.Y;
		pVertexColorArray->z = P->Point.Z;
		if (m_requestedColorFlags & CF_FOG_MODE) {
			FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - P->Fog.W);
			pVertexColorArray->color = FPlaneTo_BGRScaled_A255(&P->Light, f255_Times_One_Minus_FogW);

			m_pSecondaryColorArray[m_bufferedVerts].specular = FPlaneTo_BGR_A0(&P->Fog);
		}
		else if (m_requestedColorFlags & CF_COLOR_ARRAY) {
#ifdef RUNE
			pVertexColorArray->color = FPlaneTo_BGR_Aub(&P->Light, m_gpAlpha);
#else
			pVertexColorArray->color = FPlaneTo_BGRClamped_A255(&P->Light);
#endif
		}
		else {
			pVertexColorArray->color = 0xFFFFFFFF;
		}
		m_bufferedVerts++;

		P = Pts[i];
		pTexCoordArray = &m_pTexCoordArray[0][m_bufferedVerts];
		pTexCoordArray->u = P->U * TexInfo[0].UMult;
		pTexCoordArray->v = P->V * TexInfo[0].VMult;
		pVertexColorArray = &m_pVertexColorArray[m_bufferedVerts];
		pVertexColorArray->x = P->Point.X;
		pVertexColorArray->y = P->Point.Y;
		pVertexColorArray->z = P->Point.Z;
		if (m_requestedColorFlags & CF_FOG_MODE) {
			FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - P->Fog.W);
			pVertexColorArray->color = FPlaneTo_BGRScaled_A255(&P->Light, f255_Times_One_Minus_FogW);

			m_pSecondaryColorArray[m_bufferedVerts].specular = FPlaneTo_BGR_A0(&P->Fog);
		}
		else if (m_requestedColorFlags & CF_COLOR_ARRAY) {
#ifdef RUNE
			pVertexColorArray->color = FPlaneTo_BGR_Aub(&P->Light, m_gpAlpha);
#else
			pVertexColorArray->color = FPlaneTo_BGRClamped_A255(&P->Light);
#endif
		}
		else {
			pVertexColorArray->color = 0xFFFFFFFF;
		}
		m_bufferedVerts++;
	} while (++i < NumPts);

	return;
}


UBOOL UD3D9RenderDevice::FailedInitf(const TCHAR* Fmt, ...) {
	TCHAR TempStr[4096];
	GET_VARARGS(TempStr, ARRAY_COUNT(TempStr), Fmt);
	debugf(NAME_Init, TempStr);
	Exit();
	return 0;
}

void UD3D9RenderDevice::Exit() {
	guard(UD3D9RenderDevice::Exit);
	check(NumDevices > 0);

	//Shutdown D3D
	if (m_d3d9) {
		UnsetRes();
	}

	//Timer shutdown
	ShutdownFrameRateLimitTimer();

	//Shut down global D3D
	if (--NumDevices == 0) {
#if 0
		//Free modules
		if (hModuleD3d9) {
			verify(FreeLibrary(hModuleD3d9));
			hModuleD3d9 = NULL;
		}
#endif
	}

	unguard;
}

void UD3D9RenderDevice::ShutdownAfterError() {
	guard(UD3D9RenderDevice::ShutdownAfterError);

	debugf(NAME_Exit, TEXT("UD3D9RenderDevice::ShutdownAfterError"));

	if (DebugBit(DEBUG_BIT_BASIC)) {
		dout << TEXT("utd3d9r: ShutdownAfterError") << std::endl;
	}

	//ChangeDisplaySettings(NULL, 0);

	unguard;
}

UBOOL UD3D9RenderDevice::ResetDevice()
{
	//Free old resources if they exist
	FreePermanentResources();

	//Get real viewport size
	INT NewX = Viewport->SizeX;
	INT NewY = Viewport->SizeY;

	//Don't break editor and tiny windowed mode
	if (NewX < 16) NewX = 16;
	if (NewY < 16) NewY = 16;

	//Set new size
	m_d3dpp.BackBufferWidth = NewX;
	m_d3dpp.BackBufferHeight = NewY;

	//Reset device
	HRESULT hResult = m_d3dDevice->Reset(&m_d3dpp);
	if (FAILED(hResult)) {
		appErrorf(TEXT("Failed to create D3D device for new window size"));
	}

	//Initialize permanent rendering state, including allocation of some resources
	InitPermanentResourcesAndRenderingState();

	//Set viewport
	D3DVIEWPORT9 d3dViewport;
	d3dViewport.X = 0;
	d3dViewport.Y = 0;
	d3dViewport.Width = NewX;
	d3dViewport.Height = NewY;
	d3dViewport.MinZ = 0.0f;
	d3dViewport.MaxZ = 1.0f;
	m_d3dDevice->SetViewport(&d3dViewport);
	
	return TRUE;
}

UBOOL UD3D9RenderDevice::SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) {
	guard(UD3D9RenderDevice::SetRes);

	HRESULT hResult;
	bool saved_SetRes_isDeviceReset;

	//Get debug bits
	{
		INT i = 0;
		if (!GConfig->GetInt(g_pSection, TEXT("DebugBits"), i)) i = 0;
		m_debugBits = i;
	}
	//Display debug bits
	if (DebugBit(DEBUG_BIT_ANY)) dout << TEXT("utd3d9r: DebugBits = ") << m_debugBits << std::endl;


	debugf(TEXT("Enter SetRes()"));

	//Save parameters in case need to reset device
	m_SetRes_NewX = NewX;
	m_SetRes_NewY = NewY;
	m_SetRes_NewColorBytes = NewColorBytes;
	m_SetRes_Fullscreen = Fullscreen;

	//Save copy of SetRes is device reset flag
	saved_SetRes_isDeviceReset = m_SetRes_isDeviceReset;
	//Reset SetRes is device reset flag
	m_SetRes_isDeviceReset = false;

	// If not fullscreen, and color bytes hasn't changed, do nothing.
	//If SetRes called due to device reset, do full destroy/recreate
	if (m_d3dDevice && !saved_SetRes_isDeviceReset && !Fullscreen && !WasFullscreen && (NewColorBytes == static_cast<INT>(Viewport->ColorBytes))) {
		//Resize viewport
		if (!Viewport->ResizeViewport(BLIT_HardwarePaint | BLIT_Direct3D, NewX, NewY, NewColorBytes)) {
			return 0;
		}

		ResetDevice();
		return 1;
	}


	// Exit res.
	if (m_d3d9) {
		debugf(TEXT("UnSetRes() -> m_d3d9 != NULL"));
		UnsetRes();
	}

	//Search for closest resolution match if fullscreen requested
	//No longer changing resolution here
	if (Fullscreen) {
		INT FindX = NewX, FindY = NewY, BestError = MAXINT;
		for (INT i = 0; i < Modes.Num(); i++) {
			if (Modes(i).Z==NewColorBytes*8) {
				INT Error
				=	(Modes(i).X-FindX)*(Modes(i).X-FindX)
				+	(Modes(i).Y-FindY)*(Modes(i).Y-FindY);
				if (Error < BestError) {
					NewX      = Modes(i).X;
					NewY      = Modes(i).Y;
					BestError = Error;
				}
			}
		}
	}

	// Change window size.
	UBOOL Result = Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | BLIT_Direct3D) : (BLIT_HardwarePaint | BLIT_Direct3D), NewX, NewY, NewColorBytes);
	if (!Result) {
		return 0;
	}


	//Create main D3D9 object
	m_d3d9 = pDirect3DCreate9(D3D_SDK_VERSION);
	if (!m_d3d9) {
		appErrorf(TEXT("Direct3DCreate9 failed"));
	}


	//Get D3D caps
	hResult = m_d3d9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &m_d3dCaps);
	if (FAILED(hResult)) {
		appErrorf(TEXT("GetDeviceCaps failed (0x%08X)"), hResult);
	}


	//Create D3D device

	//Get current display mode
	D3DDISPLAYMODE d3ddm;
	hResult = m_d3d9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);
	if (FAILED(hResult)) {
		appErrorf(TEXT("Failed to get current display mode (0x%08X)"), hResult);
	}

	//Check if SetRes device reset
	//If so, get current bit depth
	//But don't check if was fullscreen
	if (saved_SetRes_isDeviceReset && !Fullscreen) {
		switch (d3ddm.Format) {
		case D3DFMT_R5G6B5: NewColorBytes = 2; break;
		case D3DFMT_X1R5G5B5: NewColorBytes = 2; break;
		case D3DFMT_A1R5G5B5: NewColorBytes = 2; break;
		default:
			NewColorBytes = 4;
		}
	}
	//Update saved NewColorBytes
	m_SetRes_NewColorBytes = NewColorBytes;

	//Don't break editor and tiny windowed mode
	if (NewX < 16) NewX = 16;
	if (NewY < 16) NewY = 16;

	//Set presentation parameters
	appMemzero(&m_d3dpp, sizeof(m_d3dpp));
	m_d3dpp.Windowed = TRUE;
	m_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	m_d3dpp.BackBufferWidth = NewX;
	m_d3dpp.BackBufferHeight = NewY;
	m_d3dpp.BackBufferFormat = d3ddm.Format;
	m_d3dpp.EnableAutoDepthStencil = TRUE;

	//Check if should be full screen
	if (Fullscreen) {
		m_d3dpp.Windowed = FALSE;
		m_d3dpp.BackBufferFormat = (NewColorBytes <= 2) ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
	}

	//Choose initial depth buffer format
	m_d3dpp.AutoDepthStencilFormat = D3DFMT_D32;
	m_numDepthBits = 32;

	//Reduce depth buffer format if necessary based on what's supported
	if (m_d3dpp.AutoDepthStencilFormat == D3DFMT_D32) {
		if (!CheckDepthFormat(d3ddm.Format, m_d3dpp.BackBufferFormat, D3DFMT_D32)) {
			m_d3dpp.AutoDepthStencilFormat = D3DFMT_D24X8;
			m_numDepthBits = 24;
		}
	}
	if (m_d3dpp.AutoDepthStencilFormat == D3DFMT_D24X8) {
		if (!CheckDepthFormat(d3ddm.Format, m_d3dpp.BackBufferFormat, D3DFMT_D24X8)) {
			m_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
			m_numDepthBits = 16;
		}
	}

	bool doSoftwareVertexInit = false;
	//Try HW vertex init if not forcing SW vertex init
	if (!doSoftwareVertexInit) {
		bool tryDefaultRefreshRate = true;
		DWORD behaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;

		behaviorFlags |= D3DCREATE_PUREDEVICE;

		//Possibly attempt to set refresh rate if fullscreen
		if (!m_d3dpp.Windowed && (RefreshRate > 0)) {
			//Attempt to create with specific refresh rate
			m_d3dpp.FullScreen_RefreshRateInHz = RefreshRate;
			hResult = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, behaviorFlags, &m_d3dpp, &m_d3dDevice);
			if (FAILED(hResult)) {
			}
			else {
				tryDefaultRefreshRate = false;
			}
		}

		if (tryDefaultRefreshRate) {
			//Attempt to create with default refresh rate
			m_d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
			hResult = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, behaviorFlags, &m_d3dpp, &m_d3dDevice);
			if (FAILED(hResult)) {
				debugf(NAME_Init, TEXT("Failed to create D3D device with hardware vertex processing (0x%08X)"), hResult);
				doSoftwareVertexInit = true;
			}
		}
	}
	//Try SW vertex init if forced earlier or if HW vertex init failed
	if (doSoftwareVertexInit) {
		bool tryDefaultRefreshRate = true;
		DWORD behaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

		//Possibly attempt to set refresh rate if fullscreen
		if (!m_d3dpp.Windowed && (RefreshRate > 0)) {
			//Attempt to create with specific refresh rate
			m_d3dpp.FullScreen_RefreshRateInHz = RefreshRate;
			hResult = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, behaviorFlags, &m_d3dpp, &m_d3dDevice);
			if (FAILED(hResult)) {
				debugf(NAME_Init, TEXT("Failed to create D3D device with software vertex processing (0x%08X)"), hResult);
			}
			else {
				tryDefaultRefreshRate = false;
			}
		}

		if (tryDefaultRefreshRate) {
			//Attempt to create with default refresh rate
			m_d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
			hResult = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, behaviorFlags, &m_d3dpp, &m_d3dDevice);
			if (FAILED(hResult)) {
				// stijn: on win10, we also see devices getting lost here...
				if (hResult == D3DERR_DEVICELOST) {
					debugf(NAME_Init, TEXT("Failed to create D3D device with default refresh rate (0x%08X)"), hResult);
					return 0;
				}
				else {
					appErrorf(TEXT("Failed to create D3D device (0x%08X)"), hResult);
				}
			}
		}
	}


	//Reset previous SwapBuffers status to okay
	m_prevSwapBuffersStatus = true;

	//Display depth buffer bit depth
	debugf(NAME_Init, TEXT("Depth bits: %u"), m_numDepthBits);

	//Debug parameter listing
	if (DebugBit(DEBUG_BIT_BASIC)) {
		#define UTGLR_DEBUG_SHOW_PARAM_REG(_name) DbgPrintInitParam(TEXT(#_name), _name)

		UTGLR_DEBUG_SHOW_PARAM_REG(LODBias);
		UTGLR_DEBUG_SHOW_PARAM_REG(OneXBlending);
		UTGLR_DEBUG_SHOW_PARAM_REG(MinLogTextureSize);
		UTGLR_DEBUG_SHOW_PARAM_REG(MaxLogTextureSize);
		UTGLR_DEBUG_SHOW_PARAM_REG(MaxAnisotropy);
		UTGLR_DEBUG_SHOW_PARAM_REG(TMUnits);
		UTGLR_DEBUG_SHOW_PARAM_REG(RefreshRate);
		UTGLR_DEBUG_SHOW_PARAM_REG(UsePrecache);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseTrilinear);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseS3TC);
		UTGLR_DEBUG_SHOW_PARAM_REG(NoFiltering);
		UTGLR_DEBUG_SHOW_PARAM_REG(SinglePassFog);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseTexIdPool);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseTexPool);
		UTGLR_DEBUG_SHOW_PARAM_REG(DynamicTexIdRecycleLevel);
		UTGLR_DEBUG_SHOW_PARAM_REG(TexDXT1ToDXT3);
		UTGLR_DEBUG_SHOW_PARAM_REG(FrameRateLimit);
		UTGLR_DEBUG_SHOW_PARAM_REG(SmoothMaskedTextures);

		#undef UTGLR_DEBUG_SHOW_PARAM_REG
	}

	//Restrict dynamic tex id recycle level range
	if (DynamicTexIdRecycleLevel < 10) DynamicTexIdRecycleLevel = 10;

	//Always use vertex specular unless caps check fails later
	UseVertexSpecular = 1;

	SupportsTC = UseS3TC;

	//Set DXT texture capability flags
	//Check for DXT1 support
	m_dxt1TextureCap = true;
	hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_DXT1);
	if (FAILED(hResult)) {
		m_dxt1TextureCap = false;
	}
	//Check for DXT3 support
	m_dxt3TextureCap = true;
	hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_DXT3);
	if (FAILED(hResult)) {
		m_dxt3TextureCap = false;
	}
	//Check for DXT5 support
	m_dxt5TextureCap = true;
	hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_DXT5);
	if (FAILED(hResult)) {
		m_dxt5TextureCap = false;
	}

	// Validate flags.

	//Special extensions validation for init only config pass
	if (!m_dxt1TextureCap) SupportsTC = 0;
#ifdef UTGLR_UNREAL_227_BUILD
	if (!m_dxt3TextureCap) SupportsTC = 0;
	if (!m_dxt5TextureCap) SupportsTC = 0;
#endif;

	//Required extensions config validation pass
	ConfigValidate_RequiredExtensions();

	TMUnits = m_d3dCaps.MaxSimultaneousTextures;
	debugf(TEXT("%i Texture Mapping Units found"), TMUnits);
	if (TMUnits > MAX_TMUNITS) {
		TMUnits = MAX_TMUNITS;
	}


	if (MaxAnisotropy < 0) {
		MaxAnisotropy = 0;
	}
	if (MaxAnisotropy) {
		int iMaxAnisotropyLimit;
		iMaxAnisotropyLimit = m_d3dCaps.MaxAnisotropy;
		debugf(TEXT("MaxAnisotropy = %i"), iMaxAnisotropyLimit); 
		if (MaxAnisotropy > iMaxAnisotropyLimit) {
			MaxAnisotropy = iMaxAnisotropyLimit;
		}
	}

	if (SupportsTC) {
		debugf(TEXT("Trying to use S3TC extension."));
	}

	//Use default if MaxLogTextureSize <= 0
	if (MaxLogTextureSize <= 0) MaxLogTextureSize = 12;

	INT MaxTextureSize = Min(m_d3dCaps.MaxTextureWidth, m_d3dCaps.MaxTextureHeight);
	INT Dummy = -1;
	while (MaxTextureSize > 0) {
		MaxTextureSize >>= 1;
		Dummy++;
	}

	if ((MaxLogTextureSize > Dummy) || (SupportsTC)) MaxLogTextureSize = Dummy;
	if ((MinLogTextureSize < 2) || (SupportsTC)) MinLogTextureSize = 2;

	MaxLogUOverV = MaxLogTextureSize;
	MaxLogVOverU = MaxLogTextureSize;
	if (SupportsTC) {
		//Current texture scaling code might not work well with compressed textures in certain cases
		//Hopefully no odd restrictions on hardware that supports compressed textures
	}
	else {
		INT MaxTextureAspectRatio = m_d3dCaps.MaxTextureAspectRatio;
		if (MaxTextureAspectRatio > 0) {
			INT MaxLogTextureAspectRatio = -1;
			while (MaxTextureAspectRatio > 0) {
				MaxTextureAspectRatio >>= 1;
				MaxLogTextureAspectRatio++;
			}
			if (MaxLogTextureAspectRatio < MaxLogUOverV) MaxLogUOverV = MaxLogTextureAspectRatio;
			if (MaxLogTextureAspectRatio < MaxLogVOverU) MaxLogVOverU = MaxLogTextureAspectRatio;
		}
	}

	debugf(TEXT("MinLogTextureSize = %i"), MinLogTextureSize);
	debugf(TEXT("MaxLogTextureSize = %i"), MaxLogTextureSize);


	// Verify hardware defaults.
	check(MinLogTextureSize >= 0);
	check(MaxLogTextureSize >= 0);
	check(MinLogTextureSize <= MaxLogTextureSize);

	// Flush textures.
#if UTGLR_ALT_FLUSH
	Flush();
#else
	Flush(1);
#endif

	//Invalidate fixed texture ids
	m_pNoTexObj = NULL;

	//Initialize permanent rendering state, including allocation of some resources
	InitPermanentResourcesAndRenderingState();


	//Initialize previous lock variables
	PL_OneXBlending = OneXBlending;
	PL_MaxLogUOverV = MaxLogUOverV;
	PL_MaxLogVOverU = MaxLogVOverU;
	PL_MinLogTextureSize = MinLogTextureSize;
	PL_MaxLogTextureSize = MaxLogTextureSize;
	PL_NoFiltering = NoFiltering;
	PL_UseTrilinear = UseTrilinear;
	PL_TexDXT1ToDXT3 = TexDXT1ToDXT3;
	PL_MaxAnisotropy = MaxAnisotropy;
	PL_SmoothMaskedTextures = SmoothMaskedTextures;
	PL_LODBias = LODBias;


	//Reset current frame count
	m_currentFrameCount = 0;

	// Remember fullscreenness.
	WasFullscreen = Fullscreen;

	return 1;

	unguard;
}

void UD3D9RenderDevice::UnsetRes() {
	guard(UD3D9RenderDevice::UnsetRes);

	if (!m_d3d9 || !m_d3dDevice)
		return;

	check(m_d3d9);
	check(m_d3dDevice);

	//Flush textures
#if UTGLR_ALT_FLUSH
	Flush();
#else
	Flush(1);
#endif

	//Free fixed textures if they were allocated
	if (m_pNoTexObj) {
		m_pNoTexObj->Release();
		m_pNoTexObj = NULL;
	}

	//Free permanent resources
	FreePermanentResources();

	//Release D3D device
	m_d3dDevice->Release();
	m_d3dDevice = NULL;

	//Release main D3D9 object
	m_d3d9->Release();
	m_d3d9 = NULL;

	unguard;
}


bool UD3D9RenderDevice::CheckDepthFormat(D3DFORMAT adapterFormat, D3DFORMAT backBufferFormat, D3DFORMAT depthBufferFormat) {
	HRESULT hResult;

	//Check depth format
	hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, adapterFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, depthBufferFormat);
	if (FAILED(hResult)) {
		return false;
	}

	//Check depth format compatibility
	hResult = m_d3d9->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, adapterFormat, backBufferFormat, depthBufferFormat);
	if (FAILED(hResult)) {
		return false;
	}

	return true;
}

void UD3D9RenderDevice::ConfigValidate_RequiredExtensions(void) {
#if !UTGLR_NO_DETAIL_TEX
	if (!(m_d3dCaps.TextureOpCaps & D3DTEXOPCAPS_BLENDCURRENTALPHA)) DetailTextures = 0;
#endif
	if (!(m_d3dCaps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC)) MaxAnisotropy = 0;
	if (!(m_d3dCaps.RasterCaps & D3DPRASTERCAPS_MIPMAPLODBIAS)) LODBias = 0;
	if (!m_dxt3TextureCap) TexDXT1ToDXT3 = 0;

	if (!(m_d3dCaps.TextureOpCaps & D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR)) SinglePassFog = 0;

	//Force 1X blending if no 2X modulation support
	if (!(m_d3dCaps.TextureOpCaps & D3DTEXOPCAPS_MODULATE2X)) OneXBlending = 0x1;	//Must use proper bit offset for Bool param

	return;
}


void UD3D9RenderDevice::InitPermanentResourcesAndRenderingState(void) {
	guard(InitPermanentResourcesAndRenderingState);

	unsigned int u;
	HRESULT hResult;

	//Set view matrix
	//D3DMATRIX d3dView = { +1.0f,  0.0f,  0.0f,  0.0f,
	//					   0.0f, -1.0f,  0.0f,  0.0f,
	//					   0.0f,  0.0f, -1.0f,  0.0f,
	//					   0.0f,  0.0f,  0.0f, +1.0f };
	//m_d3dDevice->SetTransform(D3DTS_VIEW, &d3dView);

	//Little white texture for no texture operations
	InitNoTextureSafe();

	//Clear fsBlendInfo to set any unused values to known state
	m_fsBlendInfo[0] = 0.0f;
	m_fsBlendInfo[1] = 0.0f;
	m_fsBlendInfo[2] = 0.0f;
	m_fsBlendInfo[3] = 0.0f;

	//Set alpha test disabled
	m_fsBlendInfo[0] = 0.0f;

	m_d3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
	m_d3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	m_d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);

	m_d3dDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
	m_d3dDevice->SetRenderState(D3DRS_ALPHAREF, 127);

	m_d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
	m_d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);

	m_d3dDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
	m_d3dDevice->SetRenderState(D3DRS_DITHERENABLE, TRUE);

#ifdef RUNE
	m_d3dDevice->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
	FLOAT fFogStart = 0.0f;
	m_d3dDevice->SetRenderState(D3DRS_FOGSTART, *(DWORD *)&fFogStart);
	m_gpFogEnabled = false;
#endif

	m_d3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	m_d3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);

	//Color and alpha modulation on texEnv0
	m_d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	m_d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

	//Set default texture stage tracking values
	for (u = 0; u < MAX_TMUNITS; u++) {
		m_curTexStageParams[u] = CT_DEFAULT_TEX_PARAMS;
	}

	if (LODBias) {
		SetTexLODBiasState(TMUnits);
	}

	if (MaxAnisotropy) {
		SetTexMaxAnisotropyState(TMUnits);
	}

	//Initialize texture environment state
	InitOrInvalidateTexEnvState();

	//Reset current texture ids to hopefully unused values
	for (u = 0; u < MAX_TMUNITS; u++) {
		TexInfo[u].CurrentCacheID = TEX_CACHE_ID_UNUSED;
		TexInfo[u].pBind = NULL;
	}


	//Create vertex buffers
	D3DPOOL vertexBufferPool = D3DPOOL_DEFAULT;

	m_d3dTempVertexColorBuffer = nullptr;
	m_vertexTempBufferSize = 0;
	m_csVertexArray.clear();
	//Vertex and primary color
	hResult = m_d3dDevice->CreateVertexBuffer(sizeof(FGLVertexColor) * VERTEX_BUFFER_SIZE, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, vertexBufferPool, &m_d3dVertexColorBuffer, NULL);
	if (FAILED(hResult)) {
		appErrorf(TEXT("CreateVertexBuffer failed"));
	}

	//Secondary color
	//hResult = m_d3dDevice->CreateVertexBuffer(sizeof(FGLSecondaryColor) * VERTEX_ARRAY_SIZE, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, vertexBufferPool, &m_d3dSecondaryColorBuffer, NULL);
	//if (FAILED(hResult)) {
	//	appErrorf(TEXT("CreateVertexBuffer failed"));
	//}

	//TexCoord
	for (u = 0; u < TMUnits; u++) {
		hResult = m_d3dDevice->CreateVertexBuffer(sizeof(FGLTexCoord) * VERTEX_BUFFER_SIZE, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, vertexBufferPool, &m_d3dTexCoordBuffer[u], NULL);
		if (FAILED(hResult)) {
			appErrorf(TEXT("CreateVertexBuffer failed"));
		}
		m_d3dTempTexCoordBuffer[u] = nullptr;
		m_texTempBufferSize[u] = 0;
	}

	//For sprite quad
	hResult = m_d3dDevice->CreateVertexBuffer(sizeof(FGLVertexColorTex) * 4, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, vertexBufferPool, &m_d3dQuadBuffer, NULL);
	if (FAILED(hResult)) {
		appErrorf(TEXT("CreateVertexBuffer failed"));
	}

	updateQuadBuffer(0xDEADBEEF);

	//Create stream definitions

	//Stream definition with vertices and color
	hResult = m_d3dDevice->CreateVertexDeclaration(g_oneColorStreamDef, &m_oneColorVertexDecl);
	if (FAILED(hResult)) {
		appErrorf(TEXT("CreateVertexDeclaration failed"));
	}

	//Standard stream definitions with vertices, color, and a variable number of tex coords
	for (u = 0; u < TMUnits; u++) {
		hResult = m_d3dDevice->CreateVertexDeclaration(g_standardNTextureStreamDefs[u], &m_standardNTextureVertexDecl[u]);
		if (FAILED(hResult)) {
			appErrorf(TEXT("CreateVertexDeclaration failed"));
		}
	}

	//Stream definition with vertices, two colors, and one tex coord
	hResult = m_d3dDevice->CreateVertexDeclaration(g_twoColorSingleTextureStreamDef, &m_twoColorSingleTextureVertexDecl);
	if (FAILED(hResult)) {
		appErrorf(TEXT("CreateVertexDeclaration failed"));
	}

	//For sprite quad
	hResult = m_d3dDevice->CreateVertexDeclaration(g_colorTexStreamDef, &m_ColorTexVertexDecl);
	if (FAILED(hResult)) {
		appErrorf(TEXT("CreateVertexDeclaration failed"));
	}


	//Initialize vertex buffer state tracking information
	m_curVertexBufferPos = 0;
	m_vertexColorBufferNeedsDiscard = false;
	m_secondaryColorBufferNeedsDiscard = false;
	for (u = 0; u < MAX_TMUNITS; u++) {
		m_texCoordBufferNeedsDiscard[u] = false;
	}


	//Set stream sources Now set in the lock functions
	//Vertex and primary color
	//hResult = m_d3dDevice->SetStreamSource(0, m_d3dVertexColorBuffer, 0, sizeof(FGLVertexColor));
	//if (FAILED(hResult)) {
	//	appErrorf(TEXT("SetStreamSource failed"));
	//}

	//Secondary Color
	//hResult = m_d3dDevice->SetStreamSource(1, m_d3dSecondaryColorBuffer, 0, sizeof(FGLSecondaryColor));
	//if (FAILED(hResult)) {
	//	appErrorf(TEXT("SetStreamSource failed"));
	//}

	//TexCoord
	//for (u = 0; u < TMUnits; u++) {
	//	hResult = m_d3dDevice->SetStreamSource(2 + u, m_d3dTexCoordBuffer[u], 0, sizeof(FGLTexCoord));
	//	if (FAILED(hResult)) {
	//		appErrorf(TEXT("SetStreamSource failed"));
	//	}
	//}


	//Set default stream definition
	hResult = m_d3dDevice->SetVertexDeclaration(m_standardNTextureVertexDecl[0]);
	if (FAILED(hResult)) {
		appErrorf(TEXT("SetVertexDeclaration failed"));
	}
	m_curVertexDecl = m_standardNTextureVertexDecl[0];

	//Initialize texture state cache information
	m_texEnableBits = 0x1;

	// Init variables.
	m_bufferedVertsType = BV_TYPE_NONE;
	m_bufferedVerts = 0;

	m_curBlendFlags = PF_Occlude;
	m_smoothMaskedTexturesBit = 0;
	m_alphaTestEnabled = false;
	m_curPolyFlags = 0;

	//Initialize color flags
	m_requestedColorFlags = 0;

	lightSlots = new LightSlots(m_d3dCaps.MaxActiveLights);

	unguard;
}

void UD3D9RenderDevice::FreePermanentResources(void) {
	guard(FreePermanentResources);

	delete lightSlots;
	lightSlots = nullptr;

	unsigned int u;
	HRESULT hResult;


	//Unset stream sources
	//Vertex
	hResult = m_d3dDevice->SetStreamSource(0, NULL, 0, 0);
	if (FAILED(hResult)) {
		appErrorf(TEXT("SetStreamSource failed"));
	}

	//Secondary Color
	hResult = m_d3dDevice->SetStreamSource(1, NULL, 0, 0);
	if (FAILED(hResult)) {
		appErrorf(TEXT("SetStreamSource failed"));
	}

	//TexCoord
	for (u = 0; u < TMUnits; u++) {
		hResult = m_d3dDevice->SetStreamSource(2 + u, NULL, 0, 0);
		if (FAILED(hResult)) {
			appErrorf(TEXT("SetStreamSource failed"));
		}
	}


	//Free vertex buffers
	if (m_d3dVertexColorBuffer) {
		m_d3dVertexColorBuffer->Release();
		m_d3dVertexColorBuffer = NULL;
	}
	if (m_d3dTempVertexColorBuffer) {
		m_d3dTempVertexColorBuffer->Release();
		m_d3dTempVertexColorBuffer = NULL;
	}
	if (m_d3dSecondaryColorBuffer) {
		m_d3dSecondaryColorBuffer->Release();
		m_d3dSecondaryColorBuffer = NULL;
	}
	for (u = 0; u < TMUnits; u++) {
		if (m_d3dTexCoordBuffer[u]) {
			m_d3dTexCoordBuffer[u]->Release();
			m_d3dTexCoordBuffer[u] = NULL;
		}
		if (m_d3dTempTexCoordBuffer[u]) {
			m_d3dTempTexCoordBuffer[u]->Release();
			m_d3dTempTexCoordBuffer[u] = NULL;
		}
	}


	//Set vertex declaration to something else so that it isn't using a current vertex declaration
	m_d3dDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);

	//Free stream definitions
	//Standard stream definition with vertices and color
	if (m_oneColorVertexDecl) {
		m_oneColorVertexDecl->Release();
		m_oneColorVertexDecl = NULL;
	}
	//Standard stream definitions with vertices, color, and a variable number of tex coords
	for (u = 0; u < TMUnits; u++) {
		if (m_standardNTextureVertexDecl[u]) {
			m_standardNTextureVertexDecl[u]->Release();
			m_standardNTextureVertexDecl[u] = NULL;
		}
	}
	//Stream definition with vertices, two colors, and one tex coord
	if (m_twoColorSingleTextureVertexDecl) {
		m_twoColorSingleTextureVertexDecl->Release();
		m_twoColorSingleTextureVertexDecl = NULL;
	}

	unguard;
}


UBOOL UD3D9RenderDevice::Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) {
	guard(UD3D9RenderDevice::Init);

	debugf(TEXT("Initializing D3D9Drv..."));

	// Get list of device modes.
	for (INT i = 0; ; i++) {
		DEVMODEW Tmp;
		appMemzero(&Tmp, sizeof(Tmp));
		Tmp.dmSize = sizeof(Tmp);
		if (!EnumDisplaySettingsW(NULL, i, &Tmp)) {
			break;
		}
		Modes.AddUniqueItem(FPlane(Tmp.dmPelsWidth, Tmp.dmPelsHeight, Tmp.dmBitsPerPel, Tmp.dmDisplayFrequency));
	}

	//Load D3D9 library
	if (!hModuleD3d9) {
		hModuleD3d9 = LoadLibraryA(g_d3d9DllName);
		if (!hModuleD3d9) {
			debugf(NAME_Init, TEXT("Failed to load %s"), appFromAnsi(g_d3d9DllName));
			return 0;
		}
		pDirect3DCreate9 = (LPDIRECT3DCREATE9)GetProcAddress(hModuleD3d9, "Direct3DCreate9");
		if (!pDirect3DCreate9) {
			debugf(NAME_Init, TEXT("Failed to load function from %s"), appFromAnsi(g_d3d9DllName));
			return 0;
		}
	}

	NumDevices++;

	// Init this rendering context.
	m_zeroPrefixBindTrees = m_localZeroPrefixBindTrees;
	m_nonZeroPrefixBindTrees = m_localNonZeroPrefixBindTrees;
	m_nonZeroPrefixBindChain = &m_localNonZeroPrefixBindChain;
	m_RGBA8TexPool = &m_localRGBA8TexPool;

	Viewport = InViewport;


	//Remember main window handle and get its DC
	m_hWnd = (HWND)InViewport->GetWindow();
	check(m_hWnd);
	m_hDC = GetDC(m_hWnd);
	check(m_hDC);

	if (!SetRes(NewX, NewY, NewColorBytes, Fullscreen)) {
		return FailedInitf(LocalizeError("ResFailed"));
	}

	return 1;
	unguard;
}

UBOOL UD3D9RenderDevice::Exec(const TCHAR* Cmd, FOutputDevice& Ar) {
	guard(UD3D9RenderDevice::Exec);

#if !UTGLR_NO_SUPER_EXEC
	if (Super::Exec(Cmd, Ar)) {
		return 1;
	}
#endif
	if (ParseCommand(&Cmd, TEXT("GetRes"))) {
		TArray<FPlane> Relevant;
		INT i;
		for (i = 0; i < Modes.Num(); i++) {
			if (Modes(i).Z == (Viewport->ColorBytes * 8))
				if
				(	(Modes(i).X!=320 || Modes(i).Y!=200)
				&&	(Modes(i).X!=640 || Modes(i).Y!=400) )
				Relevant.AddUniqueItem(FPlane(Modes(i).X, Modes(i).Y, 0, 0));
		}
		appQsort(&Relevant(0), Relevant.Num(), sizeof(FPlane), (QSORT_COMPARE)CompareRes);
		FString Str;
		for (i = 0; i < Relevant.Num(); i++) {
			Str += FString::Printf(TEXT("%ix%i "), (INT)Relevant(i).X, (INT)Relevant(i).Y);
		}
		Ar.Log(*Str.LeftChop(1));
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("GetColorDepths")))
	{
		Ar.Logf(TEXT("32"));
		return 1;
	}

	return 0;
	unguard;
}

void UD3D9RenderDevice::Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: Lock = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::Lock);
	check(LockCount == 0);
	++LockCount;


	//Reset stats
	BindCycles = ImageCycles = ComplexCycles = GouraudCycles = TileCycles = 0;

#ifdef D3D9_DEBUG
	m_vpEnableCount = 0;
	m_vpSwitchCount = 0;
	m_fpEnableCount = 0;
	m_fpSwitchCount = 0;
	m_AASwitchCount = 0;
	m_sceneNodeCount = 0;
	m_vbFlushCount = 0;
	m_stat0Count = 0;
	m_stat1Count = 0;
#endif

	HRESULT hResult;

	//Check for lost device
	hResult = m_d3dDevice->TestCooperativeLevel();
	if (FAILED(hResult)) {
#if 0
{
	dout << L"utd3d9r: Device Lost" << std::endl;
}
#endif
		//Wait for device to become available again
		while (1) {
			//Check if device can be reset and restored
			if (hResult == D3DERR_DEVICENOTRESET) {
				//Set new resolution
				m_SetRes_isDeviceReset = true;
				if (!SetRes(m_SetRes_NewX, m_SetRes_NewY, m_SetRes_NewColorBytes, m_SetRes_Fullscreen)) {
					appErrorf(TEXT("Failed to reset lost D3D device"));
				}

				//Exit wait loop
				break;
			}
			//If not lost and not ready to be restored, error
			else if (hResult != D3DERR_DEVICELOST) {
				appErrorf(TEXT("Error checking for lost D3D device"));
			}
			//Otherwise, device is lost and cannot be restored yet

			//Wait
			Sleep(100);

			//Don't wait for device to become available here to prevent deadlock
			break;
		}
	}

	//D3D begin scene
	if (FAILED(m_d3dDevice->BeginScene())) {
		appErrorf(TEXT("BeginScene failed"));
	}

	m_d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(1, 0, 0, 0), 0, 0);

	//Clear the Z-buffer
	if (1 || GIsEditor || (RenderLockFlags & LOCKR_ClearScreen)) {
		SetBlend(PF_Occlude);
		m_d3dDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER | ((RenderLockFlags & LOCKR_ClearScreen) ? D3DCLEAR_TARGET : 0), (DWORD)FColor(ScreenClear).TrueColor(), 1.0f, 0);
	}
	m_d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);


	bool flushTextures = false;

	//Required extensions config validation pass
	ConfigValidate_RequiredExtensions();

	//Detect changes in 1X blending setting and force tex env flush if necessary
	if (OneXBlending != PL_OneXBlending) {
		PL_OneXBlending = OneXBlending;
		InitOrInvalidateTexEnvState();
	}

	//Prevent changes to these parameters
	MaxLogUOverV = PL_MaxLogUOverV;
	MaxLogVOverU = PL_MaxLogVOverU;
	MinLogTextureSize = PL_MinLogTextureSize;
	MaxLogTextureSize = PL_MaxLogTextureSize;

	//Detect changes in various texture related options and force texture flush if necessary
	if (NoFiltering != PL_NoFiltering) {
		PL_NoFiltering = NoFiltering;
		flushTextures = true;
	}
	if (UseTrilinear != PL_UseTrilinear) {
		PL_UseTrilinear = UseTrilinear;
		flushTextures = true;
	}
	if (TexDXT1ToDXT3 != PL_TexDXT1ToDXT3) {
		PL_TexDXT1ToDXT3 = TexDXT1ToDXT3;
		flushTextures = true;
	}
	//MaxAnisotropy cannot be negative
	if (MaxAnisotropy < 0) {
		MaxAnisotropy = 0;
	}
	if (MaxAnisotropy > m_d3dCaps.MaxAnisotropy) {
		MaxAnisotropy = m_d3dCaps.MaxAnisotropy;
	}
	if (MaxAnisotropy != PL_MaxAnisotropy) {
		PL_MaxAnisotropy = MaxAnisotropy;
		flushTextures = true;

		SetTexMaxAnisotropyState(TMUnits);
	}

	if (SmoothMaskedTextures != PL_SmoothMaskedTextures) {
		PL_SmoothMaskedTextures = SmoothMaskedTextures;

		//Clear masked blending state if set before adjusting smooth masked textures bit
		SetBlend(PF_Occlude);
	}

	if (LODBias != PL_LODBias) {
		PL_LODBias = LODBias;
		SetTexLODBiasState(TMUnits);
	}

	//Smooth masked textures bit controls alpha blend for masked textures
	m_smoothMaskedTexturesBit = 0;
	if (SmoothMaskedTextures) {
		//Use alpha to coverage if using AA and enough samples, and if supported at all
		//Also requiring fragment program mode for alpha to coverage with D3D9
		m_smoothMaskedTexturesBit = PF_Masked;
	}

	//Initialize buffer verts proc pointers
	m_pBuffer3BasicVertsProc = Buffer3BasicVerts;
	m_pBuffer3ColoredVertsProc = Buffer3ColoredVerts;
	m_pBuffer3FoggedVertsProc = Buffer3FoggedVerts;

	m_pBuffer3VertsProc = NULL;

	// Remember stuff.
	FlashScale = InFlashScale;
	FlashFog   = InFlashFog;

	//Selection setup
	m_HitData = InHitData;
	m_HitSize = InHitSize;
	m_HitCount = 0;
	if (m_HitData) {
		m_HitBufSize = *m_HitSize;
		*m_HitSize = 0;

		//Start selection
		m_gclip.SelectModeStart();
	}

	//Flush textures if necessary due to config change
	if (flushTextures) {
#if UTGLR_ALT_FLUSH
		Flush();
#else
		Flush(1);
#endif
	}

	unguard;
}

void UD3D9RenderDevice::SetSceneNode(FSceneNode* Frame) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: SetSceneNode = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::SetSceneNode);

	EndBuffering();		// Flush vertex array before changing the projection matrix!

#ifdef D3D9_DEBUG
	m_sceneNodeCount++;
#endif

	//No need to set default AA state here
	//No need to set default projection state as this function always sets/initializes it
	SetDefaultStreamState();
	SetDefaultTextureState();

	// Precompute stuff.
	FLOAT rcpFrameFX = 1.0f / Frame->FX;
	m_Aspect = Frame->FY * rcpFrameFX;
	m_RProjZ = appTan(Viewport->Actor->FovAngle * PI / 360.0);
	m_RFX2 = 2.0f * m_RProjZ * rcpFrameFX;
	m_RFY2 = 2.0f * m_RProjZ * rcpFrameFX;

	//Remember Frame->X and Frame->Y
	m_sceneNodeX = Frame->X;
	m_sceneNodeY = Frame->Y;

	// Set viewport.
	D3DVIEWPORT9 d3dViewport;
	d3dViewport.X = Frame->XB;
	d3dViewport.Y = Frame->YB;
	d3dViewport.Width = Frame->X;
	d3dViewport.Height = Frame->Y;
	d3dViewport.MinZ = 0.0f;
	d3dViewport.MaxZ = 1.0f;
	m_d3dDevice->SetViewport(&d3dViewport);

	//Set clip planes if doing selection
	if (m_HitData) {
		if (Frame->Viewport->IsOrtho()) {
			float cp[4];
			FLOAT nX = Viewport->HitX - Frame->FX2;
			FLOAT pX = nX + Viewport->HitXL;
			FLOAT nY = Viewport->HitY - Frame->FY2;
			FLOAT pY = nY + Viewport->HitYL;

			nX *= m_RFX2;
			pX *= m_RFX2;
			nY *= m_RFY2;
			pY *= m_RFY2;

			cp[0] = +1.0f; cp[1] = 0.0f; cp[2] = 0.0f; cp[3] = -nX;
			m_gclip.SetCp(0, cp);
			m_gclip.SetCpEnable(0, true);

			cp[0] = 0.0f; cp[1] = +1.0f; /*cp[2] = 0.0f;*/ cp[3] = -nY;
			m_gclip.SetCp(1, cp);
			m_gclip.SetCpEnable(1, true);

			cp[0] = -1.0f; cp[1] = 0.0f; /*cp[2] = 0.0f;*/ cp[3] = +pX;
			m_gclip.SetCp(2, cp);
			m_gclip.SetCpEnable(2, true);

			cp[0] = 0.0f; cp[1] = -1.0f; /*cp[2] = 0.0f;*/ cp[3] = +pY;
			m_gclip.SetCp(3, cp);
			m_gclip.SetCpEnable(3, true);

			//Near clip plane
			/*cp[0] = 0.0f;*/ cp[1] = 0.0f; cp[2] = 1.0f; cp[3] = -0.5f;
			m_gclip.SetCp(4, cp);
			m_gclip.SetCpEnable(4, true);
		}
		else {
			FVector N[4];
			float cp[4];
			INT i;

			FLOAT nX = Viewport->HitX - Frame->FX2;
			FLOAT pX = nX + Viewport->HitXL;
			FLOAT nY = Viewport->HitY - Frame->FY2;
			FLOAT pY = nY + Viewport->HitYL;

			N[0] = (FVector(nX * Frame->RProj.Z, 0, 1) ^ FVector(0, -1, 0)).SafeNormal();
			N[1] = (FVector(pX * Frame->RProj.Z, 0, 1) ^ FVector(0, +1, 0)).SafeNormal();
			N[2] = (FVector(0, nY * Frame->RProj.Z, 1) ^ FVector(+1, 0, 0)).SafeNormal();
			N[3] = (FVector(0, pY * Frame->RProj.Z, 1) ^ FVector(-1, 0, 0)).SafeNormal();
			
			cp[3] = 0.0f;
			for (i = 0; i < 4; i++) {
				cp[0] = N[i].X;
				cp[1] = N[i].Y;
				cp[2] = N[i].Z;
				m_gclip.SetCp(i, cp);
				m_gclip.SetCpEnable(i, true);
			}

			//Near clip plane
			cp[0] = 0.0f; cp[1] = 0.0f; cp[2] = 1.0f; cp[3] = -0.5f;
			m_gclip.SetCp(4, cp);
			m_gclip.SetCpEnable(4, true);
		}
	}

	unguard;
}

void UD3D9RenderDevice::Unlock(UBOOL Blit) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: Unlock = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::Unlock);

	EndBuffering();

	SetDefaultStreamState();
	SetDefaultTextureState();

	// Unlock and render.
	check(LockCount == 1);

	//D3D end scene
	if (FAILED(m_d3dDevice->EndScene())) {
		appErrorf(TEXT("EndScene failed"));
	}

	if (Blit) {
		HRESULT hResult;
		bool swapBuffersStatus;

		//Present
		hResult = m_d3dDevice->Present(NULL, NULL, NULL, NULL);
		swapBuffersStatus = (FAILED(hResult)) ? false : true;
		//Don't signal error if device is lost
		if (hResult == D3DERR_DEVICELOST) swapBuffersStatus = true;

		check(swapBuffersStatus);
		if (!m_prevSwapBuffersStatus) {
			check(swapBuffersStatus);
		}
		m_prevSwapBuffersStatus = swapBuffersStatus;
	}

	--LockCount;

	//If doing selection, end and return hits
	if (m_HitData) {
		INT i;

		//End selection
		m_gclip.SelectModeEnd();

		*m_HitSize = m_HitCount;

		//Disable clip planes
		for (i = 0; i < 5; i++) {
			m_gclip.SetCpEnable(i, false);
		}
	}

	//Scan for old textures
	if (UseTexIdPool) {
		//Scan for old textures
		ScanForOldTextures();
	}

	//Increment current frame count
	m_currentFrameCount++;

	//Check for optional frame rate limit
#if !UNREAL_TOURNAMENT_OLDUNREAL
	if (FrameRateLimit >= 20) {
		FTime curFrameTimestamp;
		DOUBLE timeDiff;
		DOUBLE rcpFrameRateLimit;

		//First time timer init if necessary
		InitFrameRateLimitTimerSafe();

		curFrameTimestamp = appSeconds();
		timeDiff = curFrameTimestamp - m_prevFrameTimestamp;
		m_prevFrameTimestamp = curFrameTimestamp;

		rcpFrameRateLimit = 1.0f / FrameRateLimit;
		if (timeDiff < rcpFrameRateLimit) {
			float sleepTime = rcpFrameRateLimit - timeDiff;
			appSleep(sleepTime);

			m_prevFrameTimestamp = appSeconds();
		}
	}
#endif

#ifdef D3D9_DEBUG
	dout << TEXT("VP enable count = ") << m_vpEnableCount << std::endl;
	dout << TEXT("VP switch count = ") << m_vpSwitchCount << std::endl;
	dout << TEXT("FP enable count = ") << m_fpEnableCount << std::endl;
	dout << TEXT("FP switch count = ") << m_fpSwitchCount << std::endl;
	dout << TEXT("AA switch count = ") << m_AASwitchCount << std::endl;
	dout << TEXT("Scene node count = ") << m_sceneNodeCount << std::endl;
	dout << TEXT("VB flush count = ") << m_vbFlushCount << std::endl;
	dout << TEXT("Stat 0 count = ") << m_stat0Count << std::endl;
	dout << TEXT("Stat 1 count = ") << m_stat1Count << std::endl;
#endif


	unguard;
}

#if UTGLR_ALT_FLUSH
void UD3D9RenderDevice::Flush() {
#else
void UD3D9RenderDevice::Flush(UBOOL AllowPrecache) {
#endif
	guard(UD3D9RenderDevice::Flush);
	unsigned int u;

	if (!m_d3dDevice) {
		return;
	}

	for (u = 0; u < TMUnits; u++) {
		m_d3dDevice->SetTexture(u, NULL);
	}

	for (u = 0; u < NUM_CTTree_TREES; u++) {
		DWORD_CTTree_t *zeroPrefixBindTree = &m_zeroPrefixBindTrees[u];
		for (DWORD_CTTree_t::node_t *zpbmPtr = zeroPrefixBindTree->begin(); zpbmPtr != zeroPrefixBindTree->end(); zpbmPtr = zeroPrefixBindTree->next_node(zpbmPtr)) {
			zpbmPtr->data.pTexObj->Release();
		}
		zeroPrefixBindTree->clear(&m_DWORD_CTTree_Allocator);
	}

	for (u = 0; u < NUM_CTTree_TREES; u++) {
		QWORD_CTTree_t *nonZeroPrefixBindTree = &m_nonZeroPrefixBindTrees[u];
		for (QWORD_CTTree_t::node_t *nzpbmPtr = nonZeroPrefixBindTree->begin(); nzpbmPtr != nonZeroPrefixBindTree->end(); nzpbmPtr = nonZeroPrefixBindTree->next_node(nzpbmPtr)) {
			nzpbmPtr->data.pTexObj->Release();
		}
		nonZeroPrefixBindTree->clear(&m_QWORD_CTTree_Allocator);
	}

	m_nonZeroPrefixBindChain->mark_as_clear();

	for (TexPoolMap_t::node_t *RGBA8TpPtr = m_RGBA8TexPool->begin(); RGBA8TpPtr != m_RGBA8TexPool->end(); RGBA8TpPtr = m_RGBA8TexPool->next_node(RGBA8TpPtr)) {
		while (QWORD_CTTree_NodePool_t::node_t *texPoolNodePtr = RGBA8TpPtr->data.try_remove()) {
			texPoolNodePtr->data.pTexObj->Release();
			m_QWORD_CTTree_Allocator.free_node(texPoolNodePtr);
		}
	}
	m_RGBA8TexPool->clear(&m_TexPoolMap_Allocator);

	while (QWORD_CTTree_NodePool_t::node_t *nzpnpPtr = m_nonZeroPrefixNodePool.try_remove()) {
		m_QWORD_CTTree_Allocator.free_node(nzpnpPtr);
	}

	AllocatedTextures = 0;

	//Reset current texture ids to hopefully unused values
	for (u = 0; u < MAX_TMUNITS; u++) {
		TexInfo[u].CurrentCacheID = TEX_CACHE_ID_UNUSED;
		TexInfo[u].pBind = NULL;
	}

#if UTGLR_NO_ALLOW_PRECACHE
	if (UsePrecache && !GIsEditor) {
#else
	if (AllowPrecache && UsePrecache && !GIsEditor) {
#endif
		PrecacheOnFlip = 1;
	}

	unguard;
}


void UD3D9RenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: DrawComplexSurface = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::DrawComplexSurface);

	EndBuffering();

	//This function uses cached stream state information
	//This function uses cached texture state information

	check(Surface.Texture);

	//Hit select path
	if (m_HitData) {
		for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next) {
			INT NumPts = Poly->NumPts;
			CGClip::vec3_t triPts[3];
			INT i;
			const FTransform* Pt;

			Pt = Poly->Pts[0];
			triPts[0].x = Pt->Point.X;
			triPts[0].y = Pt->Point.Y;
			triPts[0].z = Pt->Point.Z;

			for (i = 2; i < NumPts; i++) {
				Pt = Poly->Pts[i - 1];
				triPts[1].x = Pt->Point.X;
				triPts[1].y = Pt->Point.Y;
				triPts[1].z = Pt->Point.Z;

				Pt = Poly->Pts[i];
				triPts[2].x = Pt->Point.X;
				triPts[2].y = Pt->Point.Y;
				triPts[2].z = Pt->Point.Z;

				m_gclip.SelectDrawTri(Frame, triPts);
			}
		}

		return;
	}

	clockFast(ComplexCycles);

	//Calculate UDot and VDot intermediates for complex surface
	m_csUDot = Facet.MapCoords.XAxis | Facet.MapCoords.Origin;
	m_csVDot = Facet.MapCoords.YAxis | Facet.MapCoords.Origin;

	//Buffer static geometry
	INT numVerts = BufferStaticComplexSurfaceGeometry(Facet);

	//Reject invalid surfaces early
	if (numVerts == 0) {
		return;
	}

	DWORD PolyFlags = Surface.PolyFlags;

	//Initialize render passes state information
	m_rpPassCount = 0;
	m_rpTMUnits = TMUnits;
	m_rpForceSingle = false;
	m_rpMasked = ((PolyFlags & PF_Masked) == 0) ? false : true;
	m_rpSetDepthEqual = false;
	m_rpColor = 0xFFFFFFFF;

	AddRenderPass(Surface.Texture, PolyFlags & ~PF_FlatShaded, 0.0f);

	if (Surface.MacroTexture) {
		AddRenderPass(Surface.MacroTexture, PF_Modulated, -0.5f);
	}

	if (Surface.LightMap) {
		AddRenderPass(Surface.LightMap, PF_Modulated, -0.5f);
	}

	if (Surface.FogMap) {
		//Check for single pass fog mode
		if (!SinglePassFog) {
			RenderPasses();
		}

		AddRenderPass(Surface.FogMap, PF_Highlighted, -0.5f);
	}

	RenderPasses();

	// UnrealEd selection.
	if (GIsEditor && (PolyFlags & (PF_Selected | PF_FlatShaded))) {
		DWORD polyColor;

		//No need to set default AA state here as it is always set on entry to DrawComplexSurface
		//No need to set default projection state here as it is always set on entry to DrawComplexSurface
		SetDefaultStreamState();
		SetDefaultTextureState();

		SetNoTexture(0);
		SetBlend(PF_Highlighted);

		if (PolyFlags & PF_FlatShaded) {
			FPlane Color;

			Color.X = Surface.FlatColor.R / 255.0f;
			Color.Y = Surface.FlatColor.G / 255.0f;
			Color.Z = Surface.FlatColor.B / 255.0f;
			Color.W = 0.85f;
			if (PolyFlags & PF_Selected) {
				Color.X *= 1.5f;
				Color.Y *= 1.5f;
				Color.Z *= 1.5f;
				Color.W = 1.0f;
			}

			polyColor = FPlaneTo_BGRAClamped(&Color);
		}
		else {
			polyColor = SurfaceSelectionColor.TrueColor() | (SurfaceSelectionColor.A << 24); //0x1F00003F;
		}

		for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next) {
			INT NumPts = Poly->NumPts;

			//Make sure at least NumPts entries are left in the vertex buffers
			if ((m_curVertexBufferPos + NumPts) >= VERTEX_BUFFER_SIZE) {
				FlushVertexBuffers();
			}

			//Lock vertexColor and texCoord0 buffers
			LockVertexColorBuffer(NumPts);
			LockTexCoordBuffer(0, NumPts);

			FGLTexCoord *pTexCoordArray = m_pTexCoordArray[0];
			FGLVertexColor *pVertexColorArray = m_pVertexColorArray;

			for (INT i = 0; i < Poly->NumPts; i++) {
				pTexCoordArray[i].u = 0.5f;
				pTexCoordArray[i].v = 0.5f;

				pVertexColorArray[i].x = Poly->Pts[i]->Point.X;
				pVertexColorArray[i].y = Poly->Pts[i]->Point.Y;
				pVertexColorArray[i].z = Poly->Pts[i]->Point.Z;
				pVertexColorArray[i].color = polyColor;
			}

			//Unlock vertexColor and texCoord0 buffers
			UnlockVertexColorBuffer();
			UnlockTexCoordBuffer(0);

			//Draw the triangle fan
			m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, getVertBufferPos(NumPts), NumPts - 2);
		}
	}

	if (m_rpSetDepthEqual == true) {
		m_d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	}

	unclockFast(ComplexCycles);
	unguard;
}

void UD3D9RenderDevice::drawLevelSurfaces(FSceneNode* frame, FSurfaceInfo& surface, std::vector<FSurfaceFacet>& facets) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: drawLevelSurfaces = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::drawLevelSurfaces);

	EndBuffering();
	StartBuffering(BV_TYPE_NONE);

	//This function uses cached stream state information
	//This function uses cached texture state information

	m_d3dDevice->SetTransform(D3DTS_WORLD, &identityMatrix);

	check(surface.Texture);

	clockFast(ComplexCycles);

	INT numVerts = 0;
	for (const FSurfaceFacet& facet : facets) {
		//Calculate UDot and VDot intermediates for complex surface
		m_csUDot = facet.MapCoords.XAxis | facet.MapCoords.Origin;
		m_csVDot = facet.MapCoords.YAxis | facet.MapCoords.Origin;

		if (facet.Span) {
			// Unpack our hidden treasure, shit it onto the cs UDot stuff
			FVector* realPan= (FVector*)facet.Span;
			m_csUDot -= realPan->X;
			m_csVDot -= realPan->Y;
		}

		//Buffer static geometry
		numVerts = BufferStaticComplexSurfaceGeometry(facet, numVerts > 0);
	}

	//Reject invalid surfaces early
	if (numVerts == 0) {
		return;
	}

	DWORD PolyFlags = surface.PolyFlags;

	// Make mirrored surfaces opaque to stop peering into the void
	if (PolyFlags & PF_Mirrored) {
		PolyFlags &= ~PF_NoOcclude;
	}

	//Initialize render passes state information
	m_rpPassCount = 0;
	m_rpTMUnits = TMUnits;
	m_rpForceSingle = false;
	m_rpMasked = ((PolyFlags & PF_Masked) == 0) ? false : true;
	m_rpSetDepthEqual = false;
	m_rpColor = 0xFFFFFFFF;

	AddRenderPass(surface.Texture, PolyFlags & ~PF_FlatShaded, 0.0f);

	if (surface.MacroTexture) {
		AddRenderPass(surface.MacroTexture, PF_Modulated, -0.5f);
	}

	if (surface.LightMap) {
		AddRenderPass(surface.LightMap, PF_Modulated, -0.5f);
	}

	if (surface.FogMap) {
		//Check for single pass fog mode
		if (!SinglePassFog) {
			RenderPasses();
		}

		AddRenderPass(surface.FogMap, PF_Highlighted, -0.5f);
	}

	RenderPasses();

	if (m_rpSetDepthEqual == true) {
		m_d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	}

	unclockFast(ComplexCycles);
	unguard;
}

#ifdef RUNE
void UD3D9RenderDevice::PreDrawFogSurface() {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: PreDrawFogSurface = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::PreDrawFogSurface);

	EndBuffering();

	SetDefaultStreamState();
	SetDefaultTextureState();

	SetBlend(PF_AlphaBlend);

	SetNoTexture(0);

	unguard;
}

void UD3D9RenderDevice::PostDrawFogSurface() {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: PostDrawFogSurface = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::PostDrawFogSurface);

	SetBlend(0);

	unguard;
}

void UD3D9RenderDevice::DrawFogSurface(FSceneNode* Frame, FFogSurf &FogSurf) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: DrawFogSurface = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::DrawFogSurface);

	FPlane Modulate(Clamp(FogSurf.FogColor.X, 0.0f, 1.0f), Clamp(FogSurf.FogColor.Y, 0.0f, 1.0f), Clamp(FogSurf.FogColor.Z, 0.0f, 1.0f), 0.0f);

	FLOAT RFogDistance = 1.0f / FogSurf.FogDistance;

	if (FogSurf.PolyFlags & PF_Masked) {
		m_d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_EQUAL);
	}

	//Set stream state
	SetDefaultStreamState();

	for (FSavedPoly* Poly = FogSurf.Polys; Poly; Poly = Poly->Next) {
		INT NumPts = Poly->NumPts;

		//Make sure at least NumPts entries are left in the vertex buffers
		if ((m_curVertexBufferPos + NumPts) >= VERTEX_BUFFER_SIZE) {
			FlushVertexBuffers();
		}

		//Lock vertexColor and texCoord0 buffers
		LockVertexColorBuffer(NumPts);
		LockTexCoordBuffer(0, NumPts);

		INT Index = 0;
		for (INT i = 0; i < NumPts; i++) {
			FTransform* P = Poly->Pts[i];

			Modulate.W = P->Point.Z * RFogDistance;
			if (Modulate.W > 1.0f) {
				Modulate.W = 1.0f;
			}
			else if (Modulate.W < 0.0f) {
				Modulate.W = 0.0f;
			}

			FGLVertexColor &destVertexColor = m_pVertexColorArray[Index];
			destVertexColor.x = P->Point.X;
			destVertexColor.y = P->Point.Y;
			destVertexColor.z = P->Point.Z;
			destVertexColor.color = FPlaneTo_BGRA(&Modulate);

			FGLTexCoord &destTexCoord = m_pTexCoordArray[0][Index];
			destTexCoord.u = 0.0f;
			destTexCoord.v = 0.0f;

			Index++;
		}

		//Unlock vertexColor and texCoord0 buffers
		UnlockVertexColorBuffer();
		UnlockTexCoordBuffer(0);

		//Draw the triangles
		m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos, NumPts - 2);

		//Advance vertex buffer position
		m_curVertexBufferPos += NumPts;
	}

	if (FogSurf.PolyFlags & PF_Masked) {
		m_d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	}

	unguard;
}

void UD3D9RenderDevice::PreDrawGouraud(FSceneNode* Frame, FLOAT FogDistance, FPlane FogColor) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: PreDrawGouraud = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::PreDrawGouraud);

	if (FogDistance > 0.0f) {
		EndBuffering();

		//Enable fog
		m_gpFogEnabled = true;
		//Enable fog
		m_d3dDevice->SetRenderState(D3DRS_FOGENABLE, TRUE);

		//Default fog mode is LINEAR
		//Default fog start is 0.0f
		m_d3dDevice->SetRenderState(D3DRS_FOGCOLOR, FPlaneTo_BGRAClamped(&FogColor));
		FLOAT fFogDistance = FogDistance;
		m_d3dDevice->SetRenderState(D3DRS_FOGEND, *(DWORD *)&fFogDistance);
	}

	unguard;
}

void UD3D9RenderDevice::PostDrawGouraud(FLOAT FogDistance) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: PostDrawGouraud = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::PostDrawGouraud);

	if (FogDistance > 0.0f) {
		EndBuffering();

		//Disable fog
		m_gpFogEnabled = false;
		//Disable fog
		m_d3dDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
	}

	unguard;
}
#endif

void UD3D9RenderDevice::DrawGouraudPolygonOld(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: DrawGouraudPolygonOld = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::DrawGouraudPolygonOld);
	clockFast(GouraudCycles);

	//Check if should render fog and if vertex specular is supported
	bool drawFog = (((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated | PF_AlphaBlend)) == PF_RenderFog) && UseVertexSpecular) ? true : false;

	//If not drawing fog, disable the PF_RenderFog flag
	if (!drawFog) {
		PolyFlags &= ~PF_RenderFog;
	}

	SetBlend(PolyFlags);
	SetTextureNoPanBias(0, Info, PolyFlags);

#if UTGLR_USES_ALPHABLEND
	BYTE alpha = 255;
	if (PolyFlags & PF_AlphaBlend) {
		alpha = appRound(Info.Texture->Alpha * 255.0f);
	}
#endif

	{
		IDirect3DVertexDeclaration9 *vertexDecl = (drawFog) ? m_twoColorSingleTextureVertexDecl : m_standardNTextureVertexDecl[0];
		//Set stream state
		SetStreamState(vertexDecl);
	}

	//Make sure at least NumPts entries are left in the vertex buffers
	if ((m_curVertexBufferPos + NumPts) >= VERTEX_BUFFER_SIZE) {
		FlushVertexBuffers();
	}

	//Lock vertexColor and texCoord0 buffers
	//Lock secondary color buffer if fog
	LockVertexColorBuffer(NumPts);
	if (drawFog) {
		LockSecondaryColorBuffer(NumPts);
	}
	LockTexCoordBuffer(0, NumPts);

	INT Index = 0;
	for (INT i = 0; i < NumPts; i++) {
		FTransTexture* P = Pts[i];

		FGLTexCoord &destTexCoord = m_pTexCoordArray[0][Index];
		destTexCoord.u = P->U * TexInfo[0].UMult;
		destTexCoord.v = P->V * TexInfo[0].VMult;

		FGLVertexColor &destVertexColor = m_pVertexColorArray[Index];
		destVertexColor.x = P->Point.X;
		destVertexColor.y = P->Point.Y;
		destVertexColor.z = P->Point.Z;

		if (PolyFlags & PF_Modulated) {
			destVertexColor.color = 0xFFFFFFFF;
		}
		else if (drawFog) {
			FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - P->Fog.W);
			destVertexColor.color = FPlaneTo_BGRScaled_A255(&P->Light, f255_Times_One_Minus_FogW);
			m_pSecondaryColorArray[Index].specular = FPlaneTo_BGR_A0(&P->Fog);
		}
		else {
#ifdef RUNE
			destVertexColor.color = FPlaneTo_BGR_Aub(&P->Light, alpha);
#else
			destVertexColor.color = FPlaneTo_BGRClamped_A255(&P->Light);
#endif
		}

		Index++;
	}

	//Unlock vertexColor and texCoord0 buffers
	//Unlock secondary color buffer if fog
	UnlockVertexColorBuffer();
	if (drawFog) {
		UnlockSecondaryColorBuffer();
	}
	UnlockTexCoordBuffer(0);

#ifdef UTGLR_DEBUG_ACTOR_WIREFRAME
	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
#endif

	//Draw the triangles
	m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, getVertBufferPos(NumPts), NumPts - 2);

#ifdef UTGLR_DEBUG_ACTOR_WIREFRAME
	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
#endif

	unclockFast(GouraudCycles);
	unguard;
}

void UD3D9RenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: DrawGouraudPolygon = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::DrawGouraudPolygon);

	EndBufferingExcept(BV_TYPE_GOURAUD_POLYS);

	//Reject invalid polygons early so that other parts of the code do not have to deal with them
	if (NumPts < 3) {
		return;
	}

	//Hit select path
	if (m_HitData) {
		CGClip::vec3_t triPts[3];
		const FTransTexture* Pt;
		INT i;

		Pt = Pts[0];
		triPts[0].x = Pt->Point.X;
		triPts[0].y = Pt->Point.Y;
		triPts[0].z = Pt->Point.Z;

		for (i = 2; i < NumPts; i++) {
			Pt = Pts[i - 1];
			triPts[1].x = Pt->Point.X;
			triPts[1].y = Pt->Point.Y;
			triPts[1].z = Pt->Point.Z;

			Pt = Pts[i];
			triPts[2].x = Pt->Point.X;
			triPts[2].y = Pt->Point.Y;
			triPts[2].z = Pt->Point.Z;

			m_gclip.SelectDrawTri(Frame, triPts);
		}

		return;
	}

	if (NumPts > 10) {
		EndBuffering();

		//No need to set default projection state here as DrawGouraudPolygonOld will set its own projection state
		//No need to set default stream state here as DrawGouraudPolygonOld will set its own stream state
		SetDefaultTextureState();

		DrawGouraudPolygonOld(Frame, Info, Pts, NumPts, PolyFlags, Span);

		return;
	}

	//Check if need to start new poly buffering
	//Make sure enough entries are left in the vertex buffers
	//based on the current position when it was locked
	if ((m_curPolyFlags != PolyFlags) ||
		(TexInfo[0].CurrentCacheID != calcCacheID(Info, PolyFlags)) ||
		((m_curVertexBufferPos + m_bufferedVerts + NumPts) >= (VERTEX_BUFFER_SIZE - 14)) ||
		(m_bufferedVerts == 0))
	{
		//Flush any previously buffered gouraud polys
		EndBuffering();

		//Check if vertex buffer flush is required
		if ((m_curVertexBufferPos + m_bufferedVerts + NumPts) >= (VERTEX_BUFFER_SIZE - 14)) {
			FlushVertexBuffers();
		}

		//Start gouraud polygon buffering
		StartBuffering(BV_TYPE_GOURAUD_POLYS);

		//Check if should render fog and if vertex specular is supported
		//Also set other color flags
		if (PolyFlags & PF_Modulated) {
			m_requestedColorFlags = 0;
		}
		else {
			m_requestedColorFlags = CF_COLOR_ARRAY;

			if (((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated | PF_AlphaBlend)) == PF_RenderFog) && UseVertexSpecular) {
				m_requestedColorFlags = CF_COLOR_ARRAY | CF_FOG_MODE;
			}
		}

		//If not drawing fog, disable the PF_RenderFog flag
		if (!(m_requestedColorFlags & CF_FOG_MODE)) {
			PolyFlags &= ~PF_RenderFog;
		}

		//Update current poly flags
		m_curPolyFlags = PolyFlags;


		//Set default texture state
		SetDefaultTextureState();

		SetBlend(PolyFlags);

		// stijn: Support alphablended decal drawing. This is a backport from 227
#if UTGLR_USES_ALPHABLEND
		if ((PolyFlags & (PF_AlphaBlend)) && (Info.Texture->PolyFlags & PF_Modulated))
			SetBlend(Info.Texture->PolyFlags);
#endif
		
		SetTextureNoPanBias(0, Info, PolyFlags);

		//Lock vertexColor and texCoord0 buffers
		//Lock secondary color buffer if fog
		LockVertexColorBuffer(NumPts);
		if (m_requestedColorFlags & CF_FOG_MODE) {
			LockSecondaryColorBuffer(NumPts);
		}
		LockTexCoordBuffer(0, NumPts);

		{
			IDirect3DVertexDeclaration9 *vertexDecl = (m_requestedColorFlags & CF_FOG_MODE) ? m_twoColorSingleTextureVertexDecl : m_standardNTextureVertexDecl[0];

			//Set stream state
			SetStreamState(vertexDecl);
		}

		//Select a buffer verts proc
		if (m_requestedColorFlags & CF_FOG_MODE) {
			m_pBuffer3VertsProc = m_pBuffer3FoggedVertsProc;
		}
		else if (m_requestedColorFlags & CF_COLOR_ARRAY) {
			m_pBuffer3VertsProc = m_pBuffer3ColoredVertsProc;
		}
		else {
			m_pBuffer3VertsProc = m_pBuffer3BasicVertsProc;
		}
#if UTGLR_USES_ALPHABLEND
		m_gpAlpha = 255;
		if (PolyFlags & PF_AlphaBlend) {
			m_gpAlpha = appRound(Info.Texture->Alpha * 255.0f);
			m_pBuffer3VertsProc = Buffer3Verts;
		}
#endif
	}

	//Buffer 3 vertices from the first (and perhaps only) triangle
	(m_pBuffer3VertsProc)(this, Pts);

	if (NumPts > 3) {
		//Buffer additional vertices from a clipped triangle
		BufferAdditionalClippedVerts(Pts, NumPts);
	}

	unguard;
}

void UD3D9RenderDevice::DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: DrawTile = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::DrawTile);

	// stijn: fix for invisible actor icons in ortho viewports
	if (GIsEditor &&
		Frame->Viewport->Actor &&
		(Frame->Viewport->IsOrtho() || Abs(Z) <= SMALL_NUMBER))
	{
		Z = 1.f;
	}

	EndBufferingExcept(BV_TYPE_TILES);

	FLOAT RPX1 = X;
	FLOAT RPX2 = X + XL;
	FLOAT RPY1 = Y;
	FLOAT RPY2 = Y + YL;

	Z = 0.5f;

	//Hit select path
	if (m_HitData) {
		CGClip::vec3_t triPts[3];

		triPts[0].x = RPX1;
		triPts[0].y = RPY1;
		triPts[0].z = Z;

		triPts[1].x = RPX2;
		triPts[1].y = RPY1;
		triPts[1].z = Z;

		triPts[2].x = RPX2;
		triPts[2].y = RPY2;
		triPts[2].z = Z;

		m_gclip.SelectDrawTri(Frame, triPts);

		triPts[1].y = RPY2;
		triPts[2].x = RPX1;
		m_gclip.SelectDrawTri(Frame, triPts);

		return;
	}

	if (1) {
		//Check if need to start new tile buffering
		if ((m_curPolyFlags != PolyFlags) ||
			(TexInfo[0].CurrentCacheID != calcCacheID(Info, PolyFlags)) ||
			((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_BUFFER_SIZE - 6)) ||
			(m_bufferedVerts == 0))
		{
			//Flush any previously buffered tiles
			EndBuffering();

			//Check if vertex buffer flush is required
			if ((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_BUFFER_SIZE - 6)) {
				FlushVertexBuffers();
			}

			//Start tile buffering
			StartBuffering(BV_TYPE_TILES);

			//Update current poly flags (before possible local modification)
			m_curPolyFlags = PolyFlags;

			//Set default texture state
			SetDefaultTextureState();

			SetBlend(PolyFlags, true);
			SetTextureNoPanBias(0, Info, PolyFlags);

			if (PolyFlags & PF_Modulated) {
				m_requestedColorFlags = 0;
			}
			else {
				m_requestedColorFlags = CF_COLOR_ARRAY;
			}

			//Lock vertexColor and texCoord0 buffers
			LockVertexColorBuffer(6);
			LockTexCoordBuffer(0, 6);

			//Set stream state
			SetDefaultStreamState();
		}

		//Get tile color
		DWORD tileColor;
		tileColor = 0xFFFFFFFF;
		if (!(PolyFlags & PF_Modulated)) {
#if UTGLR_USES_ALPHABLEND
			if (PolyFlags & PF_AlphaBlend) {
				if (Info.Texture->Alpha > 0.f)
					Color.W = Info.Texture->Alpha;
				tileColor = FPlaneTo_BGRAClamped(&Color);
			}
			else {
				tileColor = FPlaneTo_BGRClamped_A255(&Color);
			}
#else
			tileColor = FPlaneTo_BGRClamped_A255(&Color);
#endif
		}

		//Buffer the tile
		FGLVertexColor *pVertexColorArray = &m_pVertexColorArray[m_bufferedVerts];
		FGLTexCoord *pTexCoordArray = &m_pTexCoordArray[0][m_bufferedVerts];

		pVertexColorArray[0].x = RPX1;
		pVertexColorArray[0].y = RPY1;
		pVertexColorArray[0].z = Z;
		pVertexColorArray[0].color = tileColor;

		pVertexColorArray[1].x = RPX2;
		pVertexColorArray[1].y = RPY1;
		pVertexColorArray[1].z = Z;
		pVertexColorArray[1].color = tileColor;

		pVertexColorArray[2].x = RPX2;
		pVertexColorArray[2].y = RPY2;
		pVertexColorArray[2].z = Z;
		pVertexColorArray[2].color = tileColor;

		pVertexColorArray[3].x = RPX1;
		pVertexColorArray[3].y = RPY1;
		pVertexColorArray[3].z = Z;
		pVertexColorArray[3].color = tileColor;

		pVertexColorArray[4].x = RPX2;
		pVertexColorArray[4].y = RPY2;
		pVertexColorArray[4].z = Z;
		pVertexColorArray[4].color = tileColor;

		pVertexColorArray[5].x = RPX1;
		pVertexColorArray[5].y = RPY2;
		pVertexColorArray[5].z = Z;
		pVertexColorArray[5].color = tileColor;

		FLOAT TexInfoUMult = TexInfo[0].UMult;
		FLOAT TexInfoVMult = TexInfo[0].VMult;

		FLOAT SU1 = (U) * TexInfoUMult;
		FLOAT SU2 = (U + UL) * TexInfoUMult;
		FLOAT SV1 = (V) * TexInfoVMult;
		FLOAT SV2 = (V + VL) * TexInfoVMult;

		pTexCoordArray[0].u = SU1;
		pTexCoordArray[0].v = SV1;

		pTexCoordArray[1].u = SU2;
		pTexCoordArray[1].v = SV1;

		pTexCoordArray[2].u = SU2;
		pTexCoordArray[2].v = SV2;

		pTexCoordArray[3].u = SU1;
		pTexCoordArray[3].v = SV1;

		pTexCoordArray[4].u = SU2;
		pTexCoordArray[4].v = SV2;

		pTexCoordArray[5].u = SU1;
		pTexCoordArray[5].v = SV2;

		m_bufferedVerts += 6;
	}

	unguard;
}

void UD3D9RenderDevice::Draw3DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: Draw3DLine = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::Draw3DLine);

	P1 = P1.TransformPointBy(Frame->Coords);
	P2 = P2.TransformPointBy(Frame->Coords);
	if (Frame->Viewport->IsOrtho()) {
		// Zoom.
		FLOAT rcpZoom = 1.0f / Frame->Zoom;
		P1.X = (P1.X * rcpZoom) + Frame->FX2;
		P1.Y = (P1.Y * rcpZoom) + Frame->FY2;
		P2.X = (P2.X * rcpZoom) + Frame->FX2;
		P2.Y = (P2.Y * rcpZoom) + Frame->FY2;
		P1.Z = P2.Z = 1;

		// See if points form a line parallel to our line of sight (i.e. line appears as a dot).
		if (Abs(P2.X - P1.X) + Abs(P2.Y - P1.Y) >= 0.2f) {
			Draw2DLine(Frame, Color, LineFlags, P1, P2);
		}
		else {
			Draw2DPoint(Frame, Color, LINE_None, P1.X - 1.0f, P1.Y - 1.0f, P1.X + 1.0f, P1.Y + 1.0f, P1.Z);
		}
	}
	else {
		EndBufferingExcept(BV_TYPE_LINES);

		//Hit select path
		if (m_HitData) {
			CGClip::vec3_t lnPts[2];

			lnPts[0].x = P1.X;
			lnPts[0].y = P1.Y;
			lnPts[0].z = P1.Z;

			lnPts[1].x = P2.X;
			lnPts[1].y = P2.Y;
			lnPts[1].z = P2.Z;

			m_gclip.SelectDrawLine(Frame, lnPts);

			return;
		}

		//Draw3DLine does not use PolyFlags2
		const DWORD PolyFlags = PF_Highlighted | PF_Occlude;

		//Check if need to start new line buffering
		if ((m_curPolyFlags != PolyFlags) ||
			(TexInfo[0].CurrentCacheID != TEX_CACHE_ID_NO_TEX) ||
			((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_BUFFER_SIZE - 2)) ||
			(m_bufferedVerts == 0))
		{
			//Flush any previously buffered lines
			EndBuffering();

			//Check if vertex buffer flush is required
			if ((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_BUFFER_SIZE - 2)) {
				FlushVertexBuffers();
			}

			//Start line buffering
			StartBuffering(BV_TYPE_LINES);

			SetDefaultStreamState();
			SetDefaultTextureState();

			//Update current poly flags
			m_curPolyFlags = PolyFlags;

			//Set blending and no texture for lines
			SetBlend(PolyFlags);
			SetNoTexture(0);

			//Lock vertexColor and texCoord0 buffers
			LockVertexColorBuffer(2);
			LockTexCoordBuffer(0, 2);
		}

		//Get line color
		DWORD lineColor = FPlaneTo_BGRClamped_A255(&Color);

		//Buffer the line
		FGLVertexColor *pVertexColorArray = &m_pVertexColorArray[m_bufferedVerts];
		FGLTexCoord *pTexCoordArray = &m_pTexCoordArray[0][m_bufferedVerts];

		pVertexColorArray[0].x = P1.X;
		pVertexColorArray[0].y = P1.Y;
		pVertexColorArray[0].z = P1.Z;
		pVertexColorArray[0].color = lineColor;

		pVertexColorArray[1].x = P2.X;
		pVertexColorArray[1].y = P2.Y;
		pVertexColorArray[1].z = P2.Z;
		pVertexColorArray[1].color = lineColor;

		pTexCoordArray[0].u = 0.0f;
		pTexCoordArray[0].v = 0.0f;

		pTexCoordArray[1].u = 1.0f;
		pTexCoordArray[1].v = 0.0f;

		m_bufferedVerts += 2;
	}
	unguard;
}

void UD3D9RenderDevice::Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: Draw2DLine = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::Draw2DLine);

	EndBufferingExcept(BV_TYPE_LINES);

	//Get line coordinates back in 3D
	FLOAT X1Pos = m_RFX2 * (P1.X - Frame->FX2);
	FLOAT Y1Pos = m_RFY2 * (P1.Y - Frame->FY2);
	FLOAT X2Pos = m_RFX2 * (P2.X - Frame->FX2);
	FLOAT Y2Pos = m_RFY2 * (P2.Y - Frame->FY2);
	if (!Frame->Viewport->IsOrtho()) {
		X1Pos *= P1.Z;
		Y1Pos *= P1.Z;
		X2Pos *= P2.Z;
		Y2Pos *= P2.Z;
	}

	//Hit select path
	if (m_HitData) {
		CGClip::vec3_t lnPts[2];

		lnPts[0].x = X1Pos;
		lnPts[0].y = Y1Pos;
		lnPts[0].z = P1.Z;

		lnPts[1].x = X2Pos;
		lnPts[1].y = Y2Pos;
		lnPts[1].z = P2.Z;

		m_gclip.SelectDrawLine(Frame, lnPts);

		return;
	}

	//Draw2DLine does not use PolyFlags2
	const DWORD PolyFlags = PF_Highlighted | PF_Occlude;

	//Check if need to start new line buffering
	if ((m_curPolyFlags != PolyFlags) ||
		(TexInfo[0].CurrentCacheID != TEX_CACHE_ID_NO_TEX) ||
		((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_BUFFER_SIZE - 2)) ||
		(m_bufferedVerts == 0))
	{
		//Flush any previously buffered lines
		EndBuffering();

		//Check if vertex buffer flush is required
		if ((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_BUFFER_SIZE - 2)) {
			FlushVertexBuffers();
		}

		//Start line buffering
		StartBuffering(BV_TYPE_LINES);

		SetDefaultStreamState();
		SetDefaultTextureState();

		//Update current poly flags
		m_curPolyFlags = PolyFlags;

		//Set blending and no texture for lines
		SetBlend(PolyFlags);
		SetNoTexture(0);

		//Lock vertexColor and texCoord0 buffers
		LockVertexColorBuffer(2);
		LockTexCoordBuffer(0, 2);
	}

	//Get line color
	DWORD lineColor = FPlaneTo_BGRClamped_A255(&Color);

	//Buffer the line
	FGLVertexColor *pVertexColorArray = &m_pVertexColorArray[m_bufferedVerts];
	FGLTexCoord *pTexCoordArray = &m_pTexCoordArray[0][m_bufferedVerts];

	pVertexColorArray[0].x = X1Pos;
	pVertexColorArray[0].y = Y1Pos;
	pVertexColorArray[0].z = P1.Z;
	pVertexColorArray[0].color = lineColor;

	pVertexColorArray[1].x = X2Pos;
	pVertexColorArray[1].y = Y2Pos;
	pVertexColorArray[1].z = P2.Z;
	pVertexColorArray[1].color = lineColor;

	pTexCoordArray[0].u = 0.0f;
	pTexCoordArray[0].v = 0.0f;

	pTexCoordArray[1].u = 1.0f;
	pTexCoordArray[1].v = 0.0f;

	m_bufferedVerts += 2;

	unguard;
}

void UD3D9RenderDevice::Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: Draw2DPoint = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::Draw2DPoint);

	EndBufferingExcept(BV_TYPE_POINTS);

	// Hack to fix UED selection problem with selection brush
	if (GIsEditor) {
		Z = 1.0f;
	}
 
	//Get point coordinates back in 3D
	FLOAT X1Pos = m_RFX2 * (X1 - Frame->FX2 - 0.5f);
	FLOAT Y1Pos = m_RFY2 * (Y1 - Frame->FY2 - 0.5f);
	FLOAT X2Pos = m_RFX2 * (X2 - Frame->FX2 + 0.5f);
	FLOAT Y2Pos = m_RFY2 * (Y2 - Frame->FY2 + 0.5f);
	if (!Frame->Viewport->IsOrtho()) {
		X1Pos *= Z;
		Y1Pos *= Z;
		X2Pos *= Z;
		Y2Pos *= Z;
	}

	//Hit select path
	if (m_HitData) {
		CGClip::vec3_t triPts[3];

		triPts[0].x = X1Pos;
		triPts[0].y = Y1Pos;
		triPts[0].z = Z;

		triPts[1].x = X2Pos;
		triPts[1].y = Y1Pos;
		triPts[1].z = Z;

		triPts[2].x = X2Pos;
		triPts[2].y = Y2Pos;
		triPts[2].z = Z;

		m_gclip.SelectDrawTri(Frame, triPts);
		/*
		triPts[0].x = X1Pos;
		triPts[0].y = Y1Pos;
		triPts[0].z = Z;

		triPts[1].x = X2Pos;*/
		triPts[1].y = Y2Pos;/*
		triPts[1].z = Z;
		*/
		triPts[2].x = X1Pos;/*
		triPts[2].y = Y2Pos;
		triPts[2].z = Z;
		*/
		m_gclip.SelectDrawTri(Frame, triPts);

		return;
	}

	//Draw2DPoint does not use PolyFlags2
	const DWORD PolyFlags = PF_Highlighted | PF_Occlude;

	//Check if need to start new point buffering
	if ((m_curPolyFlags != PolyFlags) ||
		(TexInfo[0].CurrentCacheID != TEX_CACHE_ID_NO_TEX) ||
		((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_BUFFER_SIZE - 6)) ||
		(m_bufferedVerts == 0))
	{
		//Flush any previously buffered points
		EndBuffering();

		//Check if vertex buffer flush is required
		if ((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_BUFFER_SIZE - 6)) {
			FlushVertexBuffers();
		}

		//Start point buffering
		StartBuffering(BV_TYPE_POINTS);

		SetDefaultStreamState();
		SetDefaultTextureState();

		//Update current poly flags
		m_curPolyFlags = PolyFlags;

		//Set blending and no texture for points
		SetBlend(PolyFlags);
		SetNoTexture(0);

		//Lock vertexColor and texCoord0 buffers
		LockVertexColorBuffer(6);
		LockTexCoordBuffer(0, 6);
	}

	//Get point color
	DWORD pointColor = FPlaneTo_BGRClamped_A255(&Color);

	//Buffer the point
	FGLVertexColor *pVertexColorArray = &m_pVertexColorArray[m_bufferedVerts];
	FGLTexCoord *pTexCoordArray = &m_pTexCoordArray[0][m_bufferedVerts];

	pVertexColorArray[0].x = X1Pos;
	pVertexColorArray[0].y = Y1Pos;
	pVertexColorArray[0].z = Z;
	pVertexColorArray[0].color = pointColor;

	pVertexColorArray[1].x = X2Pos;
	pVertexColorArray[1].y = Y1Pos;
	pVertexColorArray[1].z = Z;
	pVertexColorArray[1].color = pointColor;

	pVertexColorArray[2].x = X2Pos;
	pVertexColorArray[2].y = Y2Pos;
	pVertexColorArray[2].z = Z;
	pVertexColorArray[2].color = pointColor;

	pVertexColorArray[3].x = X1Pos;
	pVertexColorArray[3].y = Y1Pos;
	pVertexColorArray[3].z = Z;
	pVertexColorArray[3].color = pointColor;

	pVertexColorArray[4].x = X2Pos;
	pVertexColorArray[4].y = Y2Pos;
	pVertexColorArray[4].z = Z;
	pVertexColorArray[4].color = pointColor;

	pVertexColorArray[5].x = X1Pos;
	pVertexColorArray[5].y = Y2Pos;
	pVertexColorArray[5].z = Z;
	pVertexColorArray[5].color = pointColor;

	FLOAT SU1 = 0.0f;
	FLOAT SU2 = 1.0f;
	FLOAT SV1 = 0.0f;
	FLOAT SV2 = 1.0f;

	pTexCoordArray[0].u = SU1;
	pTexCoordArray[0].v = SV1;

	pTexCoordArray[1].u = SU2;
	pTexCoordArray[1].v = SV1;

	pTexCoordArray[2].u = SU2;
	pTexCoordArray[2].v = SV2;

	pTexCoordArray[3].u = SU1;
	pTexCoordArray[3].v = SV1;

	pTexCoordArray[4].u = SU2;
	pTexCoordArray[4].v = SV2;

	pTexCoordArray[5].u = SU1;
	pTexCoordArray[5].v = SV2;

	m_bufferedVerts += 6;

	unguard;
}

static UTexture* getTextureWithoutNext(UTexture* texture, FTime time, FLOAT fraction) {
	INT count = 1;
	for (UTexture* next = texture->AnimNext; next && next != texture; next = next->AnimNext)
		count++;
	INT index = Clamp(appFloor(fraction * count), 0, count - 1);
	while (index-- > 0)
		texture = texture->AnimNext;
	UTexture* oldNext = texture->AnimNext;
	UTexture* oldCur = texture->AnimCur;
	texture->AnimNext = NULL;
	texture->AnimCur = NULL;

	UTexture* renderTexture = texture->Get(time);

	texture->AnimNext = oldNext;
	texture->AnimCur = oldCur;

	return renderTexture;
}

static inline DWORD getBasePolyFlags(AActor* actor) {
	DWORD basePolyFlags = 0;
	if (actor->Style == STY_Masked) {
		basePolyFlags |= PF_Masked;
	} else if (actor->Style == STY_Translucent) {
		basePolyFlags |= PF_Translucent;
	} else if (actor->Style == STY_Modulated) {
		basePolyFlags |= PF_Modulated;
	}

	if (actor->bNoSmooth) basePolyFlags |= PF_NoSmooth;
	if (actor->bSelected) basePolyFlags |= PF_Selected;
	if (actor->bMeshEnviroMap) basePolyFlags |= PF_Environment;

	return basePolyFlags;
}

void UD3D9RenderDevice::renderSprite(FSceneNode* frame, AActor* actor) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: renderSprite = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::renderSprite);
	EndBuffering();

	FPlane color = (GIsEditor && actor->bSelected) ? FPlane(.5, .9, .5, 0) : FPlane(1, 1, 1, 0);
	if (actor->ScaleGlow != 1.0) {
		color *= actor->ScaleGlow;
	}

	UTexture* texture = actor->Texture;
	FLOAT drawScale = actor->DrawScale;
	//if (frame->Viewport->Actor->ShowFlags & SHOW_ActorIcons) {
	//	drawScale = 1.0;
	//	if (!texture)
	//		texture = GetDefault<AActor>()->Texture;
	//}
	FTime currTime = frame->Viewport->CurrentTime;
	UTexture* renderTexture;
	if (actor->DrawType == DT_SpriteAnimOnce) {
		renderTexture = getTextureWithoutNext(texture, currTime, actor->LifeFraction());
	} else {
		renderTexture = texture->Get(currTime);
	}

	FTextureInfo texInfo;
	renderTexture->Lock(texInfo, currTime, -1, this);
	renderSpriteGeo(frame, actor->Location + actor->PrePivot, drawScale, texInfo, getBasePolyFlags(actor), color);
	renderTexture->Unlock(texInfo);
	unguard;
}

void UD3D9RenderDevice::updateQuadBuffer(DWORD color) {
	if (m_QuadBufferColor == color) {
		return;
	}

	//dout << L"Updating quad buffer to color " << std::hex << color << std::endl;

	FGLVertexColorTex* buffer = nullptr;
	if (FAILED(m_d3dQuadBuffer->Lock(0, 0, (void**)&buffer, D3DLOCK_NOSYSLOCK | D3DLOCK_DISCARD))) {
		appErrorf(TEXT("Vertex buffer lock failed"));
	}

	buffer[0].x = 0.0f;
	buffer[0].y = -0.5f;
	buffer[0].z = -0.5f;
	buffer[0].color = color;
	buffer[0].u = 0.0f;
	buffer[0].v = 0.0f;

	buffer[1].x = 0.0f;
	buffer[1].y = 0.5f;
	buffer[1].z = -0.5f;
	buffer[1].color = color;
	buffer[1].u = 1.0f;
	buffer[1].v = 0.0f;

	buffer[2].x = 0.0f;
	buffer[2].y = 0.5f;
	buffer[2].z = 0.5f;
	buffer[2].color = color;
	buffer[2].u = 1.0f;
	buffer[2].v = 1.0f;

	buffer[3].x = 0.0f;
	buffer[3].y = -0.5f;
	buffer[3].z = 0.5f;
	buffer[3].color = color;
	buffer[3].u = 0.0f;
	buffer[3].v = 1.0f;

	if (FAILED(m_d3dQuadBuffer->Unlock())) {
		appErrorf(TEXT("Vertex buffer unlock failed"));
	}
	m_QuadBufferColor = color;
}

void UD3D9RenderDevice::renderSpriteGeo(FSceneNode* frame, const FVector& location, FLOAT drawScale, FTextureInfo& texInfo, DWORD basePolyFlags, FPlane color) {
	guard(UD3D9RenderDevice::renderSpriteGeo);
	using namespace DirectX;

	FLOAT XScale = drawScale * texInfo.USize;
	FLOAT YScale = drawScale * texInfo.VSize;

	FVector camLoc = frame->Coords.Origin;
	FVector camUp = FVector(0.0f, -1.0f, 0.0f).TransformVectorBy(frame->Uncoords);
	XMVECTOR direction = XMVector3Normalize(FVecToDXVec(camLoc - location));

	XMMATRIX matLoc = XMMatrixTranslationFromVector(FVecToDXVec(location));
	XMMATRIX matRot = XMMatrixIdentity();
	matRot *= XMMatrixRotationY(-PI / 2);// rotate card 90 on Y since LookAt expects -z to be forward.
	matRot *= XMMatrixRotationZ(PI / 2);// rotate card 90 on Z because that's the way it's oriented aparently?.
	matRot *= XMMatrixInverse(nullptr, XMMatrixLookAtLH(XMVectorSet(0, 0, 0, 0), direction, FVecToDXVec(camUp)));
	XMMATRIX matScale = XMMatrixScaling(drawScale, XScale, YScale);

	XMMATRIX mat = XMMatrixIdentity();
	mat *= matScale;
	mat *= matRot;
	mat *= matLoc;
	D3DMATRIX actorMatrix = ToD3DMATRIX(mat);

	m_d3dDevice->SetTransform(D3DTS_WORLD, &actorMatrix);

	if (color.X > 1.0) color.X = 1.0;
	if (color.Y > 1.0) color.Y = 1.0;
	if (color.Z > 1.0) color.Z = 1.0;

	updateQuadBuffer(D3DCOLOR_COLORVALUE(color.X, color.Y, color.Z, 1.0f));

	DWORD flags = basePolyFlags | PF_TwoSided | (texInfo.Texture->PolyFlags & PF_Masked);//PF_Modulated;

	//Initialize render passes state information
	m_rpPassCount = 0;
	m_rpTMUnits = TMUnits;
	m_rpForceSingle = false;
	m_rpMasked = ((flags & PF_Masked) == 0) ? false : true;
	m_rpColor = 0xFFFFFFFF;

	flags &= ~PF_FlatShaded;
	SetBlend(flags);
	SetTexture(0, texInfo, flags, 0.0f);
	SetStreamState(m_ColorTexVertexDecl);
	DisableSubsequentTextures(1);

	if (m_currentVertexColorBuffer != m_d3dQuadBuffer) {
		HRESULT hResult = m_d3dDevice->SetStreamSource(0, m_d3dQuadBuffer, 0, sizeof(FGLVertexColorTex));
		if (FAILED(hResult)) {
			appErrorf(TEXT("SetStreamSource failed"));
		}
		m_currentVertexColorBuffer = m_d3dQuadBuffer;
	}

	m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);

	unguard;
}

void UD3D9RenderDevice::renderMeshActor(FSceneNode* frame, AActor* actor, SpecialCoord* specialCoord) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: renderMeshActor = " << si++ << std::endl;
	}
#endif
	using namespace DirectX;
	guard(UD3D9RenderDevice::renderMeshActor);
	EndBuffering();
	UMesh* mesh = actor->Mesh;
	if (!mesh)
		return;

	//dout << "rendering actor " << actor->GetName() << std::endl;
	XMMATRIX actorMatrix = XMMatrixIdentity();

	if (!actor->bParticles) {
		XMMATRIX matScale = XMMatrixScaling(actor->DrawScale, actor->DrawScale, actor->DrawScale);
		actorMatrix *= matScale;
	}
	if (specialCoord && specialCoord->enabled) {
		FCoords special = specialCoord->coord;
		actorMatrix *= FCoordToDXMat(specialCoord->coord);
		actorMatrix *= ToXMMATRIX(specialCoord->baseMatrix);
	} else {
		XMMATRIX matLoc = XMMatrixTranslationFromVector(FVecToDXVec(actor->Location + actor->PrePivot));
		XMMATRIX matRot = FRotToDXRotMat(actor->Rotation);
		actorMatrix *= matRot;
		actorMatrix *= matLoc;
	}

	// The old switcheroo, trick the game to not transform the mesh verts to object position
	FVector origLoc = actor->Location;
	FVector origPrePiv = actor->PrePivot;
	FRotator origRot = actor->Rotation;
	FLOAT origScale = actor->DrawScale;
	actor->Location = FVector(0, 0, 0);
	actor->PrePivot = FVector(0, 0, 0);
	actor->Rotation = FRotator(0, 0, 0);
	actor->DrawScale = 1.0f;

	int numVerts;
	int numTris;
	FTransTexture* samples;
	bool isLod;

	if (mesh->IsA(ULodMesh::StaticClass())) {
		isLod = true;
		ULodMesh* meshLod = (ULodMesh*)mesh;
		FTransTexture* allSamples = NewZeroed<FTransTexture>(GMem, meshLod->ModelVerts + meshLod->SpecialVerts);
		// First samples are special coordinates
		samples = &allSamples[meshLod->SpecialVerts];
		numVerts = meshLod->ModelVerts;
		meshLod->GetFrame(&allSamples->Point, sizeof(samples[0]), GMath.UnitCoords, actor, numVerts);
		numTris = meshLod->Faces.Num();
		if (specialCoord && !specialCoord->enabled && meshLod->SpecialFaces.Num() > 0) {
			// Setup special coordinate (attachment point)
			FTransTexture& v0 = allSamples[0];
			FTransTexture& v1 = allSamples[1];
			FTransTexture& v2 = allSamples[2];
			FCoords coord;
			coord.Origin = FVector(0, 0, 0);
			coord.XAxis = (v1.Point - v0.Point).SafeNormal();
			coord.YAxis = ((v0.Point - v2.Point) ^ coord.XAxis).SafeNormal();
			coord.ZAxis = coord.XAxis ^ coord.YAxis;
			FVector mid = (v0.Point + v2.Point) * 0.5f;
			specialCoord->coord = GMath.UnitCoords * coord;
			specialCoord->coord.Origin = mid;
			specialCoord->baseMatrix = ToD3DMATRIX(actorMatrix);
			specialCoord->exists = true;
		}
	} else {
		isLod = false;
		numVerts = mesh->FrameVerts;
		samples = NewZeroed<FTransTexture>(GMem, numVerts);
		mesh->GetFrame(&samples->Point, sizeof(samples[0]), GMath.UnitCoords, actor);
		numTris = mesh->Tris.Num();
	}

	actor->Location = origLoc;
	actor->PrePivot = origPrePiv;
	actor->Rotation = origRot;
	actor->DrawScale = origScale;

	FTime currentTime = frame->Viewport->CurrentTime;

	if (actor->bParticles) {
		for (INT i = 0; i < numVerts; i++) {
			FTransTexture& sample = samples[i];
			UTexture* tex = actor->Texture->Get(currentTime);
			if (actor->bRandomFrame) {
				tex = actor->MultiSkins[appCeil((&sample - samples) / 3.f) % 8];
				if (tex) {
					tex = getTextureWithoutNext(tex, currentTime, actor->LifeFraction());
				}
			}
			if (tex) {
				DWORD baseFlags = getBasePolyFlags(actor);
				FLOAT lux = Clamp(actor->ScaleGlow * 0.5f + actor->AmbientGlow / 256.f, 0.f, 1.f);
				FPlane color = FVector(lux, lux, lux);
				if (GIsEditor && (baseFlags & PF_Selected)) {
					color = color * 0.4 + FVector(0.0, 0.6, 0.0);
				}

				// Transform the local-space point into world-space
				FVector point = sample.Point;
				XMVECTOR xpoint = FVecToDXVec(point);
				xpoint = XMVector3Transform(xpoint, actorMatrix);
				point = DXVecToFVec(xpoint);

				FTextureInfo texInfo;
				tex->Lock(texInfo, currentTime, -1, this);
				renderSpriteGeo(frame, point, actor->DrawScale, texInfo, baseFlags, color);
				tex->Unlock(texInfo);
			}
		}
		return;
	}

	UniqueValueArray<UTexture*> textures;
	UniqueValueArray<FTextureInfo> texInfos;
	UTexture* envTex = nullptr;
	FTextureInfo envTexInfo;

	// Lock all mesh textures
	for (int i = 0; i < mesh->Textures.Num(); i++) {
		UTexture* tex = mesh->GetTexture(i, actor);
		if (!tex && actor->Texture) {
			tex = actor->Texture;
		} else if (!tex) {
			continue;
		}
		tex = tex->Get(currentTime);
		if (textures.insert(i, tex)) {
			FTextureInfo texInfo{};
			textures.at(i)->Lock(texInfo, currentTime, -1, this);
			texInfos.insert(i, texInfo);
		} else {
			texInfos.insert(i, texInfos.at(textures.getIndex(tex)));
		}
		envTex = tex;
	}
	if (actor->Texture) {
		envTex = actor->Texture;
	} else if (actor->Region.Zone && actor->Region.Zone->EnvironmentMap) {
		envTex = actor->Region.Zone->EnvironmentMap;
	} else if (actor->Level->EnvironmentMap) {
		envTex = actor->Level->EnvironmentMap;
	}
	if (!envTex) {
		return;
	}
	envTex->Lock(envTexInfo, currentTime, -1, this);

	bool fatten = actor->Fatness != 128;
	FLOAT fatness = (actor->Fatness / 16.0) - 8.0;

	// Calculate normals
	for (int i = 0; i < numVerts; i++) {
		samples[i].Normal = FPlane(0, 0, 0, 0);
	}
	for (int i = 0; i < numTris; i++) {
		FTransTexture* points[3];
		if (isLod) {
			ULodMesh* meshLod = (ULodMesh*)mesh;
			FMeshFace& face = meshLod->Faces(i);
			for (int j = 0; j < 3; j++) {
				FMeshWedge& wedge = meshLod->Wedges(face.iWedge[j]);
				points[j] = &samples[wedge.iVertex];
			}
		} else {
			FMeshTri& tri = mesh->Tris(i);
			for (int j = 0; j < 3; j++) {
				points[j] = &samples[tri.iVertex[j]];
			}
		}
		FVector fNorm = (points[1]->Point - points[0]->Point) ^ (points[2]->Point - points[0]->Point);
		for (int j = 0; j < 3; j++) {
			points[j]->Normal += fNorm;
		}
	}
	for (int i = 0; i < numVerts; i++) {
		XMVECTOR normal = FVecToDXVec(samples[i].Normal);
		normal = XMVector3Normalize(normal);
		samples[i].Normal = DXVecToFVec(normal);
	}

	SurfKeyMap<std::vector<FTransTexture>> surfaceMap;
	surfaceMap.reserve(numTris);

	// Process all triangles on the mesh
	for (INT i = 0; i < numTris; i++) {
		FTransTexture* points[3];
		DWORD polyFlags;
		INT texIdx;
		FMeshUV triUV[3];
		if (isLod) {
			ULodMesh* meshLod = (ULodMesh*)mesh;
			FMeshFace& face = meshLod->Faces(i);
			for (int j = 0; j < 3; j++) {
				FMeshWedge& wedge = meshLod->Wedges(face.iWedge[j]);
				points[j] = &samples[wedge.iVertex];
				triUV[j] = wedge.TexUV;
			}
			FMeshMaterial& mat = meshLod->Materials(face.MaterialIndex);
			texIdx = mat.TextureIndex;
			polyFlags = mat.PolyFlags;
		} else {
			FMeshTri& tri = mesh->Tris(i);
			for (int j = 0; j < 3; j++) {
				points[j] = &samples[tri.iVertex[j]];
				triUV[j] = tri.Tex[j];
			}
			texIdx = tri.TextureIndex;
			polyFlags = tri.PolyFlags;
		}

		polyFlags |= getBasePolyFlags(actor);

		bool environMapped = polyFlags & PF_Environment;
		FTextureInfo* texInfo = &envTexInfo;
		if (!environMapped && texInfos.has(texIdx)) {
			texInfo = &texInfos.at(texIdx);
		}
		if (!texInfo->Texture) {
			continue;
		}
		float scaleU = texInfo->UScale * texInfo->USize / 256.0;
		float scaleV = texInfo->VScale * texInfo->VSize / 256.0;

		// Sort triangles into surface/flag groups
		std::vector<FTransTexture>& pointsVec = surfaceMap[SurfKey(texInfo, polyFlags)];
		pointsVec.reserve(numTris*3);
		for (INT j = 0; j < 3; j++) {
			FTransTexture& vert = pointsVec.emplace_back(*points[j]);
			vert.U = triUV[j].U * scaleU;
			vert.V = triUV[j].V * scaleV;
			if (fatten) {
				vert.Point += vert.Normal * fatness;
			}

			// Calculate the environment UV mapping
			if (environMapped) {
				XMVECTOR ptNorm = XMVector3TransformNormal(FVecToDXVec(vert.Normal), actorMatrix);
				XMVECTOR envNorm = XMVector3TransformNormal(FVecToDXVec(vert.Point), actorMatrix);
				envNorm = XMVector3Normalize(envNorm);
				envNorm = XMVector3Reflect(envNorm, ptNorm);
				vert.U = (XMVectorGetX(envNorm) + 1.0) * 0.5 * 256.0 * scaleU;
				vert.V = (XMVectorGetY(envNorm) + 1.0) * 0.5 * 256.0 * scaleV;
			}
		}
	}

	D3DMATRIX d3dMatrix = ToD3DMATRIX(actorMatrix);
	m_d3dDevice->SetTransform(D3DTS_WORLD, &d3dMatrix);

	bool isViewModel = GUglyHackFlags & 0x1;
	D3DVIEWPORT9 vpPrev;
	if (isViewModel) {
		D3DVIEWPORT9 vp;
		m_d3dDevice->GetViewport(&vp);
		vpPrev = vp;
		vp.MaxZ = 0.1f;// Remix can pick this up for view model detection
		m_d3dDevice->SetViewport(&vp);
	}
	
	// Batch render each group of tris
	for (const std::pair<const SurfKey, std::vector<FTransTexture>>& entry : surfaceMap) {
		FTextureInfo* texInfo = entry.first.first;
		DWORD polyFlags = entry.first.second;

		BufferTriangleSurfaceGeometry(entry.second);

		//Initialize render passes state information
		m_rpPassCount = 0;
		m_rpTMUnits = TMUnits;
		m_rpForceSingle = false;
		m_rpMasked = ((polyFlags & PF_Masked) == 0) ? false : true;
		m_rpColor = 0xFFFFFFFF;

		AddRenderPass(texInfo, polyFlags & ~PF_FlatShaded, 0.0f);

		RenderPasses();
	}

	if (isViewModel) {
		m_d3dDevice->SetViewport(&vpPrev);
		m_d3dDevice->SetTransform(D3DTS_WORLD, &identityMatrix);
	}

	for (UTexture* tex : textures) {
		if (!tex)
			continue;
		tex->Unlock(texInfos.at(textures.getIndex(tex)));
	}
	envTex->Unlock(envTexInfo);

	unguard;
}

void UD3D9RenderDevice::renderMover(FSceneNode* frame, AMover* mover) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: renderMover = " << si++ << std::endl;
	}
#endif
	using namespace DirectX;
	guard(UD3D9RenderDevice::renderMover);
	EndBuffering();

	XMMATRIX matPrePiv = XMMatrixTranslationFromVector(FVecToDXVec(-mover->PrePivot));
	XMMATRIX matLoc = XMMatrixTranslationFromVector(FVecToDXVec(mover->Location));
	XMMATRIX matRot = FRotToDXRotMat(mover->Rotation);
	XMMATRIX matMainScale = FCoordToDXMat(GMath.UnitCoords * mover->MainScale);
	XMMATRIX matPostScale = FCoordToDXMat(GMath.UnitCoords * mover->PostScale);

	XMMATRIX mat = XMMatrixIdentity();
	mat *= matPrePiv;
	mat *= matMainScale;
	mat *= matRot;
	mat *= matPostScale;
	mat *= matLoc;
	D3DMATRIX actorMatrix = reinterpret_cast<D3DMATRIX&>(mat);

	m_d3dDevice->SetTransform(D3DTS_WORLD, &actorMatrix);

	// Calculate if the mover has been inversely scaled and needs the normals correcting.
	XMVECTOR overallScaleDX, _unused;
	XMMatrixDecompose(&overallScaleDX, &_unused, &_unused, mat);
	FVector overallScale = DXVecToFVec(overallScaleDX);
	int numNeg = (overallScale.X < 0) + (overallScale.Y < 0) + (overallScale.Z < 0);
	bool invertFaces = (numNeg == 1) || (numNeg == 3); // Only if inverted once or thrice

	UViewport* viewport = frame->Viewport;
	UModel* model = mover->Brush;

	std::unordered_map<UTexture*, FTextureInfo> textureInfos;
	textureInfos.reserve(model->Polys->Element.Num());
	SurfKeyMap<std::vector<FPoly*>> polys;
	polys.reserve(model->Polys->Element.Num());
	// Sort faces into surface/flag groups
	for (int i = 0; i < model->Polys->Element.Num(); i++) {
		FPoly* poly = &model->Polys->Element(i);
		UTexture* tex = poly->Texture ? poly->Texture->Get(viewport->CurrentTime) : viewport->Actor->Level->DefaultTexture;
		if (!tex) continue;
		DWORD flags = poly->PolyFlags;
		FTextureInfo* texInfo;
		if (!textureInfos.count(tex)) {
			texInfo = &textureInfos[tex];
			tex->Lock(*texInfo, frame->Viewport->CurrentTime, -1, this);
		} else {
			texInfo = &textureInfos[tex];
		}

		flags |= texInfo->Texture->PolyFlags;
		flags &= ~PF_FlatShaded;// Ignore this for poly matching, meaningless for rendering
		polys[SurfKey(texInfo, flags)].push_back(poly);
	}

	// Batch draw each group of faces
	for (std::pair<const SurfKey, std::vector<FPoly*>>& entry : polys) {
		FTextureInfo* texInfo = entry.first.first;
		DWORD flags = entry.first.second;

		bool append = false;
		for (FPoly* poly : entry.second) {
			FSavedPoly* sPoly = (FSavedPoly*)New<BYTE>(GDynMem, sizeof(FSavedPoly) + poly->NumVertices * sizeof(FTransform*));
			sPoly->NumPts = poly->NumVertices;
			sPoly->Next = NULL;
			for (int i = 0; i < sPoly->NumPts; i++) {
				int iDest = invertFaces ? (sPoly->NumPts - 1) - i : i;
				FTransform* trans = new(GDynMem)FTransform;
				trans->Point = poly->Vertex[i];
				sPoly->Pts[iDest] = trans;
			}

			FSurfaceFacet facet{};
			facet.Polys = sPoly;
			facet.MapCoords = FCoords(poly->Base, poly->TextureU, poly->TextureV, poly->Normal);

			//Calculate UDot and VDot intermediates for complex surface
			m_csUDot = facet.MapCoords.XAxis | facet.MapCoords.Origin;
			m_csVDot = facet.MapCoords.YAxis | facet.MapCoords.Origin;

			m_csUDot -= poly->PanU;
			m_csVDot -= poly->PanV;
			
			BufferStaticComplexSurfaceGeometry(facet, append);
			append = true;
		}

		//Initialize render passes state information
		m_rpPassCount = 0;
		m_rpTMUnits = TMUnits;
		m_rpForceSingle = false;
		m_rpMasked = ((flags & PF_Masked) == 0) ? false : true;
		m_rpSetDepthEqual = false;
		m_rpColor = 0xFFFFFFFF;

		AddRenderPass(texInfo, flags & ~PF_FlatShaded, 0.0f);

		RenderPasses();
	}

	for (std::pair<UTexture* const, FTextureInfo>& entry : textureInfos) {
		entry.first->Unlock(entry.second);
	}
	unguard;
}

std::unordered_set<int> UD3D9RenderDevice::LightSlots::updateActors(const std::vector<AActor*>& actors) {
	std::unordered_set<int> unsetSlots;
	// First, deactivate the slots of any actors that have been removed
	for (auto it = actorSlots.begin(); it != actorSlots.end(); ) {
		if (!std::count(actors.begin(), actors.end(), it->first)) {
			// This actor has been removed
			const int slot = it->second;
			availableSlots.push_front(slot);
			unsetSlots.insert(slot);
			//dout << L"Slot " << slot << L" actor deleted" << std::endl;
			it = actorSlots.erase(it);
		} else {
			++it;
		}
	}

	// Now, add any new actors
	for (AActor* actor : actors) {
		if (!actorSlots.count(actor)) {
			// This is a new actor
			if (availableSlots.empty()) {
				dout << "No light slots left! Needed " << actors.size() << " lights" << std::endl;
				break;
				//throw std::runtime_error("No available slots");
			}
			int slot = availableSlots.front();
			availableSlots.pop_front();
			actorSlots[actor] = slot;
			unsetSlots.erase(slot);
			//dout << L"Slot " << slot << L" actor added " << actor->GetName() << std::endl;
		}
	}
	return unsetSlots;
}

void UD3D9RenderDevice::renderLights(std::vector<AActor*> lightActors) {
	guard(UD3D9RenderDevice::renderLights);

	//m_d3dDevice->SetRenderState(D3DRS_LIGHTING, TRUE);

	std::unordered_set<int> nowEmptySlots = lightSlots->updateActors(lightActors);

	// Disable lights that no longer exist
	for (int slot : nowEmptySlots) {
		//dout << L"Disabling slot " << slot << std::endl;
		D3DLIGHT9 lightInfo{ D3DLIGHT_POINT };
		HRESULT res = m_d3dDevice->SetLight(slot, &lightInfo);
		assert(res == D3D_OK);
		res = m_d3dDevice->LightEnable(slot, false);
		assert(res == D3D_OK);
	}

	for (auto pair : lightSlots->slotMap()) {
		AActor* actor = pair.first;
		int slot = pair.second;
		D3DLIGHT9 lightInfo = D3DLIGHT9();
		lightInfo.Type = D3DLIGHT_POINT;
		lightInfo.Position = D3DVECTOR{ actor->Location.X, actor->Location.Y, actor->Location.Z };
		lightInfo.Diffuse = hsvToRgb(actor->LightHue, 0xFFu - actor->LightSaturation, actor->LightBrightness);
		lightInfo.Specular = lightInfo.Diffuse;
		lightInfo.Range = 1000;
		HRESULT res = m_d3dDevice->SetLight(slot, &lightInfo);
		assert(res == D3D_OK);
		res = m_d3dDevice->LightEnable(slot, true);
		assert(res == D3D_OK);
	}

	//m_d3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	unguard;
}

// Helper function to convert a hash to a float in the range [-1, 1]
static inline float hashToFloat(size_t hash, size_t max_value) {
	return (static_cast<float>(hash % (2 * max_value)) / static_cast<float>(max_value)) - 1.0f;
}

static FVector hashToNormalVector(size_t hash) {
	size_t max_value = std::numeric_limits<size_t>::max();

	FVector norm;
	norm.X = hashToFloat(hash & 0xFFF, max_value); // Use the lower 12 bits
	norm.Y = hashToFloat((hash >> 12) & 0xFFF, max_value); // Use the next 12 bits
	norm.Z = hashToFloat((hash >> 24) & 0xFF, max_value); // Use the remaining 8 bits

	norm.Normalize();

	return norm;
}

void UD3D9RenderDevice::renderSkyZoneAnchor(ASkyZoneInfo* zone, const FVector* location) {
	guard(UD3D9RenderDevice::renderSkyZoneAnchor);
	using namespace DirectX;

	if (!EnableSkyBoxAnchors) {
		return;
	}

	XMMATRIX actorMatrix = XMMatrixIdentity();

	XMMATRIX matLoc = XMMatrixTranslationFromVector(FVecToDXVec(location ? *location : zone->Location));
	XMMATRIX matRot = XMMatrixInverse(nullptr, FRotToDXRotMat(zone->Rotation));
	actorMatrix *= matRot;
	actorMatrix *= matLoc;

	D3DMATRIX d3dMatrix = ToD3DMATRIX(actorMatrix);
	m_d3dDevice->SetTransform(D3DTS_WORLD, &d3dMatrix);

	size_t locHash = std::hash<FVector>()(zone->Location);
	size_t rotHash = std::hash<FRotator>()(zone->RotationRate);

	UTexture* tex = zone->Level->DefaultTexture;
	FTextureInfo texInfo;
	tex->Lock(texInfo, 0.0, -1, this);

	FTransTexture v1{};
	v1.Point = FVector(0, 0, 5) + hashToNormalVector(locHash);
	v1.U = 0.5 * texInfo.USize;
	v1.V = 1.0 * texInfo.VSize;
	FTransTexture v2{};
	v2.Point = FVector(5, 0, 0);
	v2.U = 0.5 * texInfo.USize;
	v2.V = 0.25 * texInfo.VSize;
	FTransTexture v3{};
	v3.Point = FVector(0, 5, 0) + hashToNormalVector(rotHash);
	v3.U = 1.0 * texInfo.USize;
	v3.V = 0.0 * texInfo.VSize;
	FTransTexture v4{};
	v4.Point = FVector(0, -5, 0);
	v4.U = 0.0 * texInfo.USize;
	v4.V = 0.0 * texInfo.VSize;

	std::vector<FTransTexture> verts;
	verts.push_back(v1);
	verts.push_back(v2);
	verts.push_back(v3);

	verts.push_back(v1);
	verts.push_back(v4);
	verts.push_back(v2);

	verts.push_back(v2);
	verts.push_back(v4);
	verts.push_back(v3);

	DWORD polyFlags = PF_Occlude;

	BufferTriangleSurfaceGeometry(verts);

	//Initialize render passes state information
	m_rpPassCount = 0;
	m_rpTMUnits = TMUnits;
	m_rpForceSingle = false;
	m_rpMasked = ((polyFlags & PF_Masked) == 0) ? false : true;
	m_rpColor = 0xFFFFFFFF;

	AddRenderPass(&texInfo, polyFlags & ~PF_FlatShaded, 0.0f);

	RenderPasses();

	tex->Unlock(texInfo);

	unguard;
}

INT UD3D9RenderDevice::BufferTriangleSurfaceGeometry(const std::vector<FTransTexture>& vertices) {
	// Buffer "static" geometry.
	m_csVertexArray.clear();
	m_csVertexArray.reserve(vertices.size());

	// I was promised to be given triangles
	assert(vertices.size() % 3 == 0);
	int tris = vertices.size() / 3;

	for (int i = 0; i < tris; i++) {
		INT numPts = 3;
		for (int j = 0; j < numPts; j++) {
			const FTransTexture& vert = vertices[(i * 3) + j];
			FGLVertexNormTex* bufVert = &m_csVertexArray.emplace_back();
			bufVert->vert.x = vert.Point.X;
			bufVert->vert.y = vert.Point.Y;
			bufVert->vert.z = vert.Point.Z;
			bufVert->norm.x = vert.Normal.X;
			bufVert->norm.y = vert.Normal.Y;
			bufVert->norm.z = vert.Normal.Z;
			bufVert->tex.u = vert.U;
			bufVert->tex.v = vert.V;
		};
	}

	return m_csVertexArray.size();
}

void UD3D9RenderDevice::ClearZ(FSceneNode* Frame) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: ClearZ = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::ClearZ);

	EndBuffering();

	//Default AA state not required for glClear
	//Default projection state not required for glClear
	//Default stream state not required for glClear
	//Default texture state not required for glClear

	SetBlend(PF_Occlude);
	m_d3dDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);

	unguard;
}

void UD3D9RenderDevice::PushHit(const BYTE* Data, INT Count) {
	guard(UD3D9RenderDevice::PushHit);

	INT i;

	EndBuffering();

	//Add to stack
	for (i = 0; i < Count; i += 4) {
		DWORD hitName = *(DWORD *)(Data + i);
		m_gclip.PushHitName(hitName);
	}

	unguard;
}

void UD3D9RenderDevice::PopHit(INT Count, UBOOL bForce) {
	guard(UD3D9RenderDevice::PopHit);

	EndBuffering();

	INT i;
	bool selHit;

	//Handle hit
	selHit = m_gclip.CheckNewSelectHit();
	if (selHit || bForce) {
		DWORD nHitNameBytes;

		nHitNameBytes = m_gclip.GetHitNameStackSize() * 4;
		if (nHitNameBytes <= m_HitBufSize) {
			m_gclip.GetHitNameStackValues((unsigned int *)m_HitData, nHitNameBytes / 4);
			m_HitCount = nHitNameBytes;
		}
		else {
			m_HitCount = 0;
		}
	}

	//Remove from stack
	for (i = 0; i < Count; i += 4) {
		m_gclip.PopHitName();
	}

	unguard;
}

void UD3D9RenderDevice::GetStats(TCHAR* Result) {
	guard(UD3D9RenderDevice::GetStats);

	double msPerCycle = GSecondsPerCycle * 1000.0f;
	appSprintf( // stijn: mem safety NOT OK
		Result,
		TEXT("D3D9 stats: Bind=%04.1f Image=%04.1f Complex=%04.1f Gouraud=%04.1f Tile=%04.1f"),
		msPerCycle * BindCycles,
		msPerCycle * ImageCycles,
		msPerCycle * ComplexCycles,
		msPerCycle * GouraudCycles,
		msPerCycle * TileCycles
	);

	unguard;
}

void UD3D9RenderDevice::ReadPixels(FColor* Pixels) {
	guard(UD3D9RenderDevice::ReadPixels);

	INT x, y;
	INT SizeX, SizeY;
	INT StartX = 0, StartY = 0;
	HRESULT hResult;
	IDirect3DSurface9 *d3dsFrontBuffer = NULL;
	D3DDISPLAYMODE d3ddm;
	HDC hDibDC = 0;
	HBITMAP hDib = 0;
	LPVOID pDibData = 0;
	DWORD *pScreenshot = 0;
	INT screenshotPitch;

	SizeX = Viewport->SizeX;
	SizeY = Viewport->SizeY;

	//Get current display mode
	hResult = m_d3dDevice->GetDisplayMode(0, &d3ddm);
	if (FAILED(hResult)) {
		return;
	}

	//Allocate resources and get screen data
	if (m_d3dpp.Windowed) { //Windowed
		struct {
			BITMAPINFOHEADER bmiHeader; 
			DWORD bmiColors[3];
		} bmi;
		HBITMAP hOldBitmap;

		//Create memory DC
		hDibDC = CreateCompatibleDC(m_hDC);
		if (!hDibDC) {
			return;
		}

		//DIB format
		appMemzero(&bmi.bmiHeader, sizeof(bmi.bmiHeader));
		bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
		bmi.bmiHeader.biWidth = SizeX;
		bmi.bmiHeader.biHeight = -SizeY;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_BITFIELDS;
		bmi.bmiHeader.biSizeImage = SizeX * SizeY * 4;
		bmi.bmiHeader.biXPelsPerMeter = 0;
		bmi.bmiHeader.biYPelsPerMeter = 0;
		bmi.bmiHeader.biClrUsed = 0;
		bmi.bmiHeader.biClrImportant = 0;
		bmi.bmiColors[0] = 0x00FF0000;
		bmi.bmiColors[1] = 0x0000FF00;
		bmi.bmiColors[2] = 0x000000FF;

		//Create DIB
		hDib = CreateDIBSection(
			hDibDC,
			(BITMAPINFO *)&bmi,
			DIB_RGB_COLORS,
			&pDibData,
			NULL,
			0);
		if (!hDib) {
			DeleteDC(hDibDC);
			return;
		}

		//Get copy of window contents
		hOldBitmap = (HBITMAP)SelectObject(hDibDC, hDib);
		BitBlt(hDibDC, 0, 0, SizeX, SizeY, m_hDC, 0, 0, SRCCOPY);
		SelectObject(hDibDC, hOldBitmap);

		//Set pointer to screenshot data and pitch
		pScreenshot = (DWORD *)pDibData;
		screenshotPitch = bmi.bmiHeader.biWidth * 4;
	}
	else { //Fullscreen
		//Create surface to hold screenshot
		hResult = m_d3dDevice->CreateOffscreenPlainSurface(d3ddm.Width, d3ddm.Height, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &d3dsFrontBuffer, NULL);
		if (FAILED(hResult)) {
			return;
		}

		//Get copy of front buffer
		hResult = m_d3dDevice->GetFrontBufferData(0, d3dsFrontBuffer);
		if (FAILED(hResult)) {
			//Release surface to hold screenshot
			d3dsFrontBuffer->Release();

			return;
		}

		//Clamp size just in case
		if (SizeX > d3ddm.Width) SizeX = d3ddm.Width;
		if (SizeY > d3ddm.Height) SizeY = d3ddm.Height;

		//Lock screenshot surface
		D3DLOCKED_RECT lockRect;
		hResult = d3dsFrontBuffer->LockRect(&lockRect, NULL, D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY);
		if (FAILED(hResult)) {
			//Release surface to hold screenshot
			d3dsFrontBuffer->Release();

			return;
		}

		//Set pointer to screenshot data and pitch
		pScreenshot = (DWORD *)lockRect.pBits;
		screenshotPitch = lockRect.Pitch;
	}


	//Copy screenshot data
	if (pScreenshot) {
		INT DestSizeX = Viewport->SizeX;
		pScreenshot = (DWORD *)((BYTE *)pScreenshot + (StartY * screenshotPitch));
		for (y = 0; y < SizeY; y++) {
			for (x = 0; x < SizeX; x++) {
				DWORD dwPixel = pScreenshot[StartX + x];
				Pixels[(y * DestSizeX) + x] = FColor(((dwPixel >> 0) & 0xFF), ((dwPixel >> 8) & 0xFF), ((dwPixel >> 16) & 0xFF), 0xFF);
			}
			pScreenshot = (DWORD *)((BYTE *)pScreenshot + screenshotPitch);
		}
	}


	//Free resources
	if (m_d3dpp.Windowed) { //Windowed
		if (hDib) {
			DeleteObject(hDib);
		}
		if (hDibDC) {
			DeleteDC(hDibDC);
		}
	}
	else { //Fullscreen
		//Unlock screenshot surface
		d3dsFrontBuffer->UnlockRect();

		//Release surface to hold screenshot
		d3dsFrontBuffer->Release();
	}

	unguard;
}

void UD3D9RenderDevice::EndFlash() {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: EndFlash = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::EndFlash);
	if ((FlashScale != FPlane(0.5f, 0.5f, 0.5f, 0.0f)) || (FlashFog != FPlane(0.0f, 0.0f, 0.0f, 0.0f))) {
		EndBuffering();

		SetDefaultStreamState();
		SetDefaultTextureState();

		SetBlend(PF_Highlighted);
		SetNoTexture(0);

		FPlane tempPlane = FPlane(FlashFog.X, FlashFog.Y, FlashFog.Z, 1.0f - Min(FlashScale.X * 2.0f, 1.0f));
		DWORD flashColor = FPlaneTo_BGRA(&tempPlane);

		FLOAT RPX1 = 0;
		FLOAT RPX2 = RPX1 + m_sceneNodeX;
		FLOAT RPY1 = 0;
		FLOAT RPY2 = RPY1 + m_sceneNodeY;

		FLOAT ZCoord = 0.5f;

		//Make sure at least 4 entries are left in the vertex buffers
		if ((m_curVertexBufferPos + 4) >= VERTEX_BUFFER_SIZE) {
			FlushVertexBuffers();
		}

		//Lock vertexColor and texCoord0 buffers
		LockVertexColorBuffer(4);
		LockTexCoordBuffer(0, 4);

		FGLTexCoord *pTexCoordArray = m_pTexCoordArray[0];
		FGLVertexColor *pVertexColorArray = m_pVertexColorArray;

		pTexCoordArray[0].u = 0.0f;
		pTexCoordArray[0].v = 0.0f;

		pTexCoordArray[1].u = 1.0f;
		pTexCoordArray[1].v = 0.0f;

		pTexCoordArray[2].u = 1.0f;
		pTexCoordArray[2].v = 1.0f;

		pTexCoordArray[3].u = 0.0f;
		pTexCoordArray[3].v = 1.0f;

		pVertexColorArray[0].x = RPX1;
		pVertexColorArray[0].y = RPY1;
		pVertexColorArray[0].z = ZCoord;
		pVertexColorArray[0].color = flashColor;

		pVertexColorArray[1].x = RPX2;
		pVertexColorArray[1].y = RPY1;
		pVertexColorArray[1].z = ZCoord;
		pVertexColorArray[1].color = flashColor;

		pVertexColorArray[2].x = RPX2;
		pVertexColorArray[2].y = RPY2;
		pVertexColorArray[2].z = ZCoord;
		pVertexColorArray[2].color = flashColor;

		pVertexColorArray[3].x = RPX1;
		pVertexColorArray[3].y = RPY2;
		pVertexColorArray[3].z = ZCoord;
		pVertexColorArray[3].color = flashColor;

		//Unlock vertexColor and texCoord0 buffers
		UnlockVertexColorBuffer();
		UnlockTexCoordBuffer(0);

		//Draw the square
		m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos, 2);

		//Advance vertex buffer position
		m_curVertexBufferPos += 4;
	}
	unguard;
}

void UD3D9RenderDevice::PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags) {
	guard(UD3D9RenderDevice::PrecacheTexture);
	SetTextureNoPanBias(0, Info, PolyFlags);
	unguard;
}

#if UNREAL_TOURNAMENT_OLDUNREAL
UBOOL UD3D9RenderDevice::SupportsTextureFormat(ETextureFormat Format)
{
	switch ( Format )
	{
	case TEXF_P8:     return true;
	case TEXF_BGRA8:  return true;
	case TEXF_RGBA8_: return true;
	case TEXF_BC1:    return SupportsTC && m_dxt1TextureCap;
	case TEXF_BC2:    return SupportsTC && m_dxt3TextureCap;
	case TEXF_BC3:    return SupportsTC && m_dxt5TextureCap;
	default:          return false;
	}
}
#endif

//This function is safe to call multiple times to initialize once
void UD3D9RenderDevice::InitNoTextureSafe(void) {
	guard(UD3D9RenderDevice::InitNoTexture);
	unsigned int u, v;
	HRESULT hResult;
	D3DLOCKED_RECT lockRect;
	DWORD *pTex;

	//Return early if already initialized
	if (m_pNoTexObj != 0) {
		return;
	}

	//Create the texture
	hResult = m_d3dDevice->CreateTexture(4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &m_pNoTexObj, NULL);
	if (FAILED(hResult)) {
		appErrorf(TEXT("CreateTexture (basic RGBA8) failed"));
	}

	//Lock texture level 0
	if (FAILED(m_pNoTexObj->LockRect(0, &lockRect, NULL, D3DLOCK_NOSYSLOCK))) {
		appErrorf(TEXT("Texture lock failed"));
	}

	//Write texture
	pTex = (DWORD *)lockRect.pBits;
	for (u = 0; u < 4; u++) {
		for (v = 0; v < 4; v++) {
			pTex[v] = 0xFFFFFFFF;
		}
		pTex = (DWORD *)((BYTE *)pTex + lockRect.Pitch);
	}

	//Unlock texture level 0
	if (FAILED(m_pNoTexObj->UnlockRect(0))) {
		appErrorf(TEXT("Texture unlock failed"));
	}

	return;
	unguard;
}

void UD3D9RenderDevice::ScanForOldTextures(void) {
	guard(UD3D9RenderDevice::ScanForOldTextures);

	unsigned int u;
	FCachedTexture *pCT;

	//Prevent currently bound textures from being recycled
	for (u = 0; u < MAX_TMUNITS; u++) {
		FCachedTexture *pBind = TexInfo[u].pBind;
		if (pBind != NULL) {
			//Update last used frame count so that the texture will not be recycled
			pBind->LastUsedFrameCount = m_currentFrameCount;

			//Move node to tail of linked list if in LRU list
			if (pBind->bindType == BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST) {
				m_nonZeroPrefixBindChain->unlink(pBind);
				m_nonZeroPrefixBindChain->link_to_tail(pBind);
			}
		}
	}

	pCT = m_nonZeroPrefixBindChain->begin();
	while (pCT != m_nonZeroPrefixBindChain->end()) {
		DWORD numFramesSinceUsed = m_currentFrameCount - pCT->LastUsedFrameCount;
		if (numFramesSinceUsed > DynamicTexIdRecycleLevel) {
			//See if the tex pool is not enabled, or the tex format is not RGBA8, or the texture has mipmaps
			if (!UseTexPool || (pCT->texFormat != D3DFMT_A8R8G8B8) || (pCT->texParams.filter & CT_HAS_MIPMAPS_BIT)) {
				//Remove node from linked list
				m_nonZeroPrefixBindChain->unlink(pCT);

				//Get pointer to node in bind map
				QWORD_CTTree_t::node_t *pNode = (QWORD_CTTree_t::node_t *)((BYTE *)pCT - (PTRINT)&(((QWORD_CTTree_t::node_t *)0)->data));
				//Extract tree index
				BYTE treeIndex = pCT->treeIndex;
				//Advanced cached texture pointer to next entry in linked list
				pCT = pCT->pNext;

				//Remove node from bind map
				m_nonZeroPrefixBindTrees[treeIndex].remove(pNode);

				//Delete the texture
				pNode->data.pTexObj->Release();
#if 0
{
	static int si;
	dout << L"utd3d9r: Texture delete = " << si++ << std::endl;
}
#endif

				continue;
			}
			else {
				TexPoolMap_t::node_t *texPoolPtr;

#if 0
{
	static int si;
	dout << L"utd3d9r: TexPool free = " << si++ << L", Id = 0x" << HexString((DWORD)pCT->pTexObj, 32)
		<< L", u = " << pCT->UBits << L", v = " << pCT->VBits << std::endl;
}
#endif

				//Remove node from linked list
				m_nonZeroPrefixBindChain->unlink(pCT);

				//Create a key from the lg2 width and height of the texture object
				TexPoolMapKey_t texPoolKey = MakeTexPoolMapKey(pCT->UBits, pCT->VBits);

				//Get pointer to node in bind map
				QWORD_CTTree_t::node_t *pNode = (QWORD_CTTree_t::node_t *)((BYTE *)pCT - (PTRINT)&(((QWORD_CTTree_t::node_t *)0)->data));
				//Extract tree index
				BYTE treeIndex = pCT->treeIndex;
				//Advanced cached texture pointer to next entry in linked list
				pCT = pCT->pNext;

				//Remove node from bind map
				m_nonZeroPrefixBindTrees[treeIndex].remove(pNode);

				//See if the key does not yet exist
				texPoolPtr = m_RGBA8TexPool->find(texPoolKey);
				//If the key does not yet exist, add an empty vector in its place
				if (texPoolPtr == 0) {
					texPoolPtr = m_TexPoolMap_Allocator.alloc_node();
					texPoolPtr->key = texPoolKey;
					texPoolPtr->data = QWORD_CTTree_NodePool_t();
					m_RGBA8TexPool->insert(texPoolPtr);
				}

				//Add node plus texture id to a list in the tex pool based on its dimensions
				texPoolPtr->data.add(pNode);

				continue;
			}
		}

		//The list is sorted
		//Stop searching on first one not to be recycled
		break;

		//pCT = pCT->pNext;
	}

	unguard;
}

void UD3D9RenderDevice::SetNoTextureNoCheck(INT Multi) {
	guard(UD3D9RenderDevice::SetNoTexture);

	// Set small white texture.
	clockFast(BindCycles);

	//Set texture
	m_d3dDevice->SetTexture(Multi, m_pNoTexObj);

	//Set filter
	SetTexFilter(Multi, CT_MIN_FILTER_POINT | CT_MIP_FILTER_NONE);

	TexInfo[Multi].CurrentCacheID = TEX_CACHE_ID_NO_TEX;
	TexInfo[Multi].pBind = NULL;

	unclockFast(BindCycles);

	unguard;
}

bool UD3D9RenderDevice::shouldGenHashTexture(const FTextureInfo& tex) {
	if (!EnableHashTextures) {
		return false;
	}
	return tex.bRealtime;
}

void UD3D9RenderDevice::fillHashTexture(FTexConvertCtx convertContext, FTextureInfo& tex) {
	const FString name = tex.Texture->GetPathNameSafe();
	debugf(NAME_DevGraphics, TEXT("Generating magic hash texture for '%s'"), *name);
	const unsigned int nameLen = name.Len();
	uint8_t* pTex = (uint8_t*) convertContext.lockRect.pBits;
	unsigned int nameIdx = 0;
	for (int y = 0; y < convertContext.texHeightPow2; y++) {
		for (int x = 0; x < convertContext.texWidthPow2; x++) {
			*pTex++ = name[nameIdx++] ^ 0x80; // B
			if (nameIdx >= nameLen) nameIdx = 0;
			*pTex++ = name[nameIdx++]; // G
			if (nameIdx >= nameLen) nameIdx = 0;
			*pTex++ = name[nameIdx++] ^ 0x80; // R
			if (nameIdx >= nameLen) nameIdx = 0;
			*pTex++ = 0xFF; // A
		}
	}
}

bool UD3D9RenderDevice::BindTexture(DWORD texNum, FTexInfo& Tex, FTextureInfo& Info, DWORD PolyFlags, FCachedTexture*& Bind)
{
	const bool isZeroPrefixCacheID = ((Tex.CurrentCacheID & 0xFFFFFFFF00000000ULL) == 0) ? true : false;

	FCachedTexture* pBind = NULL;
	bool existingBind = false;
	HRESULT hResult;

	if (isZeroPrefixCacheID) {
		DWORD CacheIDSuffix = (Tex.CurrentCacheID & 0x00000000FFFFFFFFULL);

		DWORD_CTTree_t* zeroPrefixBindTree = &m_zeroPrefixBindTrees[CTZeroPrefixCacheIDSuffixToTreeIndex(CacheIDSuffix)];
		DWORD_CTTree_t::node_t* bindTreePtr = zeroPrefixBindTree->find(CacheIDSuffix);
		if (bindTreePtr != 0) {
			pBind = &bindTreePtr->data;
			existingBind = true;
		}
		else {
			DWORD_CTTree_t::node_t* pNewNode;

			//Insert new texture info
			pNewNode = m_DWORD_CTTree_Allocator.alloc_node();
			pNewNode->key = CacheIDSuffix;
			zeroPrefixBindTree->insert(pNewNode);
			pBind = &pNewNode->data;

			//Set bind type
			pBind->bindType = BIND_TYPE_ZERO_PREFIX;

			//Set default tex params
			pBind->texParams = CT_DEFAULT_TEX_PARAMS;
			pBind->dynamicTexBits = (PolyFlags & PF_NoSmooth) ? DT_NO_SMOOTH_BIT : 0;

			//Cache texture info for the new texture
			CacheTextureInfo(pBind, Info, PolyFlags);

#if 0
			{
				static int si;
				dout << L"utd3d9r: Create texture zp = " << si++ << std::endl;
			}
#endif
			//Create the texture
			hResult = m_d3dDevice->CreateTexture(
				1U << pBind->UBits, 1U << pBind->VBits, (Info.NumMips == 1) ? 1 : (pBind->MaxLevel + 1),
				0, pBind->texFormat, D3DPOOL_MANAGED, &pBind->pTexObj, NULL);
			if (FAILED(hResult)) {
				appErrorf(TEXT("CreateTexture failed"));
			}

			//Allocate a new texture id
			AllocatedTextures++;
		}
	}
	else {
		DWORD CacheIDSuffix = (Tex.CurrentCacheID & 0x00000000FFFFFFFFULL);
		DWORD treeIndex = CTNonZeroPrefixCacheIDSuffixToTreeIndex(CacheIDSuffix);

		QWORD_CTTree_t* nonZeroPrefixBindTree = &m_nonZeroPrefixBindTrees[treeIndex];
		QWORD_CTTree_t::node_t* bindTreePtr = nonZeroPrefixBindTree->find(Tex.CurrentCacheID);
		if (bindTreePtr != 0) {
			pBind = &bindTreePtr->data;
			pBind->LastUsedFrameCount = m_currentFrameCount;

			//Check if texture is in LRU list
			if (pBind->bindType == BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST) {
				//Move node to tail of linked list
				m_nonZeroPrefixBindChain->unlink(pBind);
				m_nonZeroPrefixBindChain->link_to_tail(pBind);
			}

			existingBind = true;
		}
		else {
			QWORD_CTTree_t::node_t* pNewNode;

			//Allocate a new node
			//Use the node pool if it is not empty
			pNewNode = m_nonZeroPrefixNodePool.try_remove();
			if (!pNewNode) {
				pNewNode = m_QWORD_CTTree_Allocator.alloc_node();
			}

			//Insert new texture info
			pNewNode->key = Tex.CurrentCacheID;
			nonZeroPrefixBindTree->insert(pNewNode);
			pBind = &pNewNode->data;
			pBind->LastUsedFrameCount = m_currentFrameCount;

			//Set bind type
			pBind->bindType = BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST;

			//Save tree index
			pBind->treeIndex = (BYTE)treeIndex;

			//Set default tex params
			pBind->texParams = CT_DEFAULT_TEX_PARAMS;
			pBind->dynamicTexBits = (PolyFlags & PF_NoSmooth) ? DT_NO_SMOOTH_BIT : 0;

			//Check if texture should be in LRU list
			if (pBind->bindType == BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST) {
				//Add node to linked list
				m_nonZeroPrefixBindChain->link_to_tail(pBind);
			}

#if UNREAL_TOURNAMENT_OLDUNREAL
			pBind->RealtimeChangeCount = Info.Texture ? Info.Texture->RealtimeChangeCount : 0;
#endif

			//Cache texture info for the new texture
			CacheTextureInfo(pBind, Info, PolyFlags);

			//See if the tex pool is enabled
			bool needTexIdAllocate = true;
			if (UseTexPool) {
				//See if the format will be RGBA8
				//Only textures without mipmaps are stored in the tex pool
				if ((pBind->texType == TEX_TYPE_NORMAL) && (Info.NumMips == 1)) {
					TexPoolMap_t::node_t* texPoolPtr;

					//Create a key from the lg2 width and height of the texture object
					TexPoolMapKey_t texPoolKey = MakeTexPoolMapKey(pBind->UBits, pBind->VBits);

					//Search for the key in the map
					texPoolPtr = m_RGBA8TexPool->find(texPoolKey);
					if (texPoolPtr != 0) {
						QWORD_CTTree_NodePool_t::node_t* texPoolNodePtr;

						//Get a reference to the pool of nodes with tex ids of the right dimension
						QWORD_CTTree_NodePool_t& texPool = texPoolPtr->data;

						//Attempt to get a texture id for the tex pool
						if ((texPoolNodePtr = texPool.try_remove()) != 0) {
							//Use texture id from node in tex pool
							pBind->pTexObj = texPoolNodePtr->data.pTexObj;

							//Use tex params from node in tex pool
							pBind->texParams = texPoolNodePtr->data.texParams;
							pBind->dynamicTexBits = texPoolNodePtr->data.dynamicTexBits;

							//Then add node to free list
							m_nonZeroPrefixNodePool.add(texPoolNodePtr);

#if 0
							{
								static int si;
								dout << L"utd3d9r: TexPool retrieve = " << si++ << L", Id = 0x" << HexString((DWORD)pBind->pTexObj, 32)
									<< L", u = " << pBind->UBits << L", v = " << pBind->VBits << std::endl;
							}
#endif

							//Clear the need tex id allocate flag
							needTexIdAllocate = false;
						}
					}
				}
			}
			if (needTexIdAllocate) {
#if 0
				{
					static int si;
					dout << L"utd3d9r: Create texture nzp = " << si++ << std::endl;
				}
#endif
				//Create the texture
				hResult = m_d3dDevice->CreateTexture(
					1U << pBind->UBits, 1U << pBind->VBits, (Info.NumMips == 1) ? 1 : (pBind->MaxLevel + 1),
					0, pBind->texFormat, D3DPOOL_MANAGED, &pBind->pTexObj, NULL);
				if (FAILED(hResult)) {
					appErrorf(TEXT("CreateTexture failed"));
				}

				//Allocate a new texture id
				AllocatedTextures++;
			}
		}
	}

	Bind = pBind;
	return existingBind;
}

void UD3D9RenderDevice::SetTextureNoCheck(DWORD texNum, FTexInfo& Tex, FTextureInfo& Info, DWORD PolyFlags) {
	guard(UD3D9RenderDevice::SetTexture);

	// Make current.
	clockFast(BindCycles);

	FCachedTexture* pBind = nullptr;
	bool existingBind = BindTexture(texNum, Tex, Info, PolyFlags, pBind);

	//Save pointer to current texture bind for current texture unit
	Tex.pBind = pBind;

	//Set texture
	m_d3dDevice->SetTexture(texNum, pBind->pTexObj);

	unclockFast(BindCycles);

	// Account for all the impact on scale normalization.
	Tex.UMult = pBind->UMult;
	Tex.VMult = pBind->VMult;

	//Check for any changes to dynamic texture object parameters
	{
		BYTE desiredDynamicTexBits;

		desiredDynamicTexBits = (PolyFlags & PF_NoSmooth) ? DT_NO_SMOOTH_BIT : 0;
		if (desiredDynamicTexBits != pBind->dynamicTexBits) {
			BYTE dynamicTexBitsXor;

			dynamicTexBitsXor = desiredDynamicTexBits ^ pBind->dynamicTexBits;

			//Update dynamic tex bits early as there are no subsequent dependencies
			pBind->dynamicTexBits = desiredDynamicTexBits;

			if (dynamicTexBitsXor & DT_NO_SMOOTH_BIT) {
				BYTE desiredTexParamsFilter;

				//Set partial desired filter tex params
				desiredTexParamsFilter = 0;
				if (NoFiltering) {
					desiredTexParamsFilter |= CT_MIN_FILTER_POINT | CT_MIP_FILTER_NONE;
				}
				else if (PolyFlags & PF_NoSmooth) {
					desiredTexParamsFilter |= CT_MIN_FILTER_POINT;
					desiredTexParamsFilter |= ((pBind->texParams.filter & CT_HAS_MIPMAPS_BIT) == 0) ? CT_MIP_FILTER_NONE : CT_MIP_FILTER_POINT;
				}
				else {
					desiredTexParamsFilter |= (MaxAnisotropy) ? CT_MIN_FILTER_ANISOTROPIC : CT_MIN_FILTER_LINEAR;
					desiredTexParamsFilter |= ((pBind->texParams.filter & CT_HAS_MIPMAPS_BIT) == 0) ? CT_MIP_FILTER_NONE : (UseTrilinear ? CT_MIP_FILTER_LINEAR : CT_MIP_FILTER_POINT);
					desiredTexParamsFilter |= CT_MAG_FILTER_LINEAR_NOT_POINT_BIT;
				}

				//Store partial updated texture parameter state in cached texture object
				const BYTE MODIFIED_TEX_PARAMS_FILTER_BITS = CT_MIN_FILTER_MASK | CT_MIP_FILTER_MASK | CT_MAG_FILTER_LINEAR_NOT_POINT_BIT;
				pBind->texParams.filter = (pBind->texParams.filter & ~MODIFIED_TEX_PARAMS_FILTER_BITS) | desiredTexParamsFilter;
			}
		}
	}

#if UNREAL_TOURNAMENT_OLDUNREAL
	if (Info.Texture)
	{
		if (pBind->RealtimeChangeCount != Info.Texture->RealtimeChangeCount)
			pBind->RealtimeChangeCount = Info.Texture->RealtimeChangeCount;
		else
			Info.bRealtimeChanged = 0;
	}
#endif

	// Upload if needed.
	if (!existingBind || (Info.bRealtimeChanged && !shouldGenHashTexture(Info))) {
		FColor paletteIndex0;

		// Cleanup texture flags.
		if (SupportsLazyTextures) {
			Info.Load();
		}
		Info.bRealtimeChanged = 0;

		//Set palette index 0 to black for masked paletted textures
		if (Info.Palette && (PolyFlags & PF_Masked)) {
			paletteIndex0 = Info.Palette[0];
			Info.Palette[0] = FColor(0,0,0,0);
		}

		// Download the texture.
		clockFast(ImageCycles);

		m_texConvertCtx.pBind = pBind;

		UBOOL SkipMipmaps = (Info.NumMips == 1);
		INT MaxLevel = pBind->MaxLevel;

		//Only calculate texture filter parameters for new textures
		if (!existingBind) {
			tex_params_t desiredTexParams;

			//Set desired filter tex params
			desiredTexParams.filter = 0;
			if (NoFiltering) {
				desiredTexParams.filter |= CT_MIN_FILTER_POINT | CT_MIP_FILTER_NONE;
			}
			else if (PolyFlags & PF_NoSmooth) {
				desiredTexParams.filter |= CT_MIN_FILTER_POINT;
				desiredTexParams.filter |= SkipMipmaps ? CT_MIP_FILTER_NONE : CT_MIP_FILTER_POINT;
			}
			else {
				desiredTexParams.filter |= (MaxAnisotropy) ? CT_MIN_FILTER_ANISOTROPIC : CT_MIN_FILTER_LINEAR;
				desiredTexParams.filter |= SkipMipmaps ? CT_MIP_FILTER_NONE : (UseTrilinear ? CT_MIP_FILTER_LINEAR : CT_MIP_FILTER_POINT);
				desiredTexParams.filter |= CT_MAG_FILTER_LINEAR_NOT_POINT_BIT;
			}

			if (!SkipMipmaps) {
				desiredTexParams.filter |= CT_HAS_MIPMAPS_BIT;
			}

			//Store updated texture parameter state in cached texture object
			pBind->texParams = desiredTexParams;
		}


		//Some textures only upload the base texture
		INT MaxUploadLevel = MaxLevel;
		if (SkipMipmaps) {
			MaxUploadLevel = 0;
		}


		//Set initial texture width and height in the context structure
		//Setup code must ensure that both UBits and VBits are greater than or equal to 0
		m_texConvertCtx.texWidthPow2 = 1 << pBind->UBits;
		m_texConvertCtx.texHeightPow2 = 1 << pBind->VBits;

		guard(WriteTexture);
		INT Level;
		for (Level = 0; Level <= MaxUploadLevel; Level++) {
			// Convert the mipmap.
			INT MipIndex = pBind->BaseMip + Level;
			INT stepBits = 0;
			if (MipIndex >= Info.NumMips && Info.NumMips > 0) {
				stepBits = MipIndex - (Info.NumMips - 1);
				MipIndex = Info.NumMips - 1;
			}
			m_texConvertCtx.stepBits = stepBits;

			FMipmapBase* Mip = Info.Mips[MipIndex];
			if (!Mip->DataPtr) {
				//Skip looking at any subsequent mipmap pointers
				break;
			}
			else {
				//Lock texture level
				if (FAILED(pBind->pTexObj->LockRect(Level, &m_texConvertCtx.lockRect, NULL, D3DLOCK_NOSYSLOCK))) {
					appErrorf(TEXT("Texture lock failed"));
				}

				//Texture data copy and potential conversion if necessary
				switch (pBind->texType) {
				case TEX_TYPE_COMPRESSED_DXT1:
					guard(ConvertDXT1_DXT1);
					ConvertDXT1_DXT1(Mip, Level);
					unguard;
					break;

				case TEX_TYPE_COMPRESSED_DXT1_TO_DXT3:
					guard(ConvertDXT1_DXT3);
					ConvertDXT1_DXT3(Mip, Level);
					unguard;
					break;

				case TEX_TYPE_COMPRESSED_DXT3:
					guard(ConvertDXT3);
					ConvertDXT35_DXT35(Mip, Level);
					unguard;
					break;

				case TEX_TYPE_COMPRESSED_DXT5:
					guard(ConvertDXT5);
					ConvertDXT35_DXT35(Mip, Level);
					unguard;
					break;

				case TEX_TYPE_PALETTED:
					//Not supported
					break;

				case TEX_TYPE_HAS_PALETTE:
					switch (pBind->texFormat) {
					case D3DFMT_R5G6B5:
						guard(ConvertP8_RGB565);
						if (stepBits == 0) {
							ConvertP8_RGB565_NoStep(Mip, Info.Palette, Level);
						}
						else {
							ConvertP8_RGB565(Mip, Info.Palette, Level);
						}
						unguard;
						break;

					case D3DFMT_X1R5G5B5:
					case D3DFMT_A1R5G5B5:
						guard(ConvertP8_RGBA5551);
						if (stepBits == 0) {
							ConvertP8_RGBA5551_NoStep(Mip, Info.Palette, Level);
						}
						else {
							ConvertP8_RGBA5551(Mip, Info.Palette, Level);
						}
						unguard;
						break;

					default:
						guard(ConvertP8_RGBA8888);
						if (stepBits == 0) {
							ConvertP8_RGBA8888_NoStep(Mip, Info.Palette, Level);
						}
						else {
							ConvertP8_RGBA8888(Mip, Info.Palette, Level);
						}
						unguard;
					}
					break;

				case TEX_TYPE_CACHE_GEN:
					fillHashTexture(m_texConvertCtx, Info);
					break;

				default:
#if UNREAL_TOURNAMENT_OLDUNREAL
					switch (Info.Format) {
					case TEXF_BGRA8:
						guard(ConvertBGRA8);
						(this->*pBind->pConvertBGRA8)(Mip, Level);
						unguard;
						break;

					case TEXF_RGBA8_:
						guard(ConvertRGBA8);
						(this->*pBind->pConvertRGBA8)(Mip, Level);
						unguard;
						break;

					default:
						guard(ConvertBGRA7777);
						(this->*pBind->pConvertBGRA7777)(Mip, Level);
						unguard;
					}
#else
					guard(ConvertBGRA7777);
					(this->*pBind->pConvertBGRA7777)(Mip, Level);
					unguard;
#endif
				}

				DWORD texWidth, texHeight;

				//Get current texture width and height
				texWidth = m_texConvertCtx.texWidthPow2;
				texHeight = m_texConvertCtx.texHeightPow2;

				//Calculate and save next texture width and height
				//Both are divided by two down to a floor of 1
				//Texture width and height must be even powers of 2 for the following code to work
				m_texConvertCtx.texWidthPow2 = (texWidth & 0x1) | (texWidth >> 1);
				m_texConvertCtx.texHeightPow2 = (texHeight & 0x1) | (texHeight >> 1);

				//Unlock texture level
				if (FAILED(pBind->pTexObj->UnlockRect(Level))) {
					appErrorf(TEXT("Texture unlock failed"));
				}
			}
		}
		unguard;

		unclockFast(ImageCycles);

		//Restore palette index 0 for masked paletted textures
		if (Info.Palette && (PolyFlags & PF_Masked)) {
			Info.Palette[0] = paletteIndex0;
		}

		// Cleanup.
		if (SupportsLazyTextures) {
			Info.Unload();
		}
	}

	//Set texture filter parameters
	SetTexFilter(texNum, pBind->texParams.filter);

	unguard;
}

void UD3D9RenderDevice::CacheTextureInfo(FCachedTexture *pBind, const FTextureInfo &Info, DWORD PolyFlags) {
#if 0
{
	dout << L"utd3d9r: CacheId = "
		<< HexString((DWORD)((QWORD)Info.CacheID >> 32), 32) << L":"
		<< HexString((DWORD)((QWORD)Info.CacheID & 0xFFFFFFFF), 32) << std::endl;
}
{
	const UTexture *pTexture = Info.Texture;	
	dout << L"utd3d9r: TexName = " << *FObjectFullName(pTexture) << std::endl;
}
{
	dout << L"utd3d9r: NumMips = " << Info.NumMips << std::endl;
}
{
	unsigned int u;

	dout << L"utd3d9r: ZPBindTree Size = ";
	for (u = 0; u < NUM_CTTree_TREES; u++) {
		dout << m_zeroPrefixBindTrees[u].calc_size();
		if (u != (NUM_CTTree_TREES - 1)) dout << L", ";
	}
	dout << std::endl;

	dout << L"utd3d9r: NZPBindTree Size = ";
	for (u = 0; u < NUM_CTTree_TREES; u++) {
		dout << m_nonZeroPrefixBindTrees[u].calc_size();
		if (u != (NUM_CTTree_TREES - 1)) dout << L", ";
	}
	dout << std::endl;
}
#endif

	// Figure out scaling info for the texture.
	DWORD texFlags = 0;
	INT BaseMip = 0;
	INT MaxLevel;
	INT UBits = Info.Mips[0]->UBits;
	INT VBits = Info.Mips[0]->VBits;
	INT UCopyBits = 0;
	INT VCopyBits = 0;
	if ((UBits - VBits) > MaxLogUOverV) {
		VCopyBits += (UBits - VBits) - MaxLogUOverV;
		VBits = UBits - MaxLogUOverV;
	}
	if ((VBits - UBits) > MaxLogVOverU) {
		UCopyBits += (VBits - UBits) - MaxLogVOverU;
		UBits = VBits - MaxLogVOverU;
	}
	if (UBits < MinLogTextureSize) {
		UCopyBits += MinLogTextureSize - UBits;
		UBits += MinLogTextureSize - UBits;
	}
	if (VBits < MinLogTextureSize) {
		VCopyBits += MinLogTextureSize - VBits;
		VBits += MinLogTextureSize - VBits;
	}
	if (UBits > MaxLogTextureSize) {
		BaseMip += UBits - MaxLogTextureSize;
		VBits -= UBits - MaxLogTextureSize;
		UBits = MaxLogTextureSize;
		if (VBits < 0) {
			VCopyBits = -VBits;
			VBits = 0;
		}
	}
	if (VBits > MaxLogTextureSize) {
		BaseMip += VBits - MaxLogTextureSize;
		UBits -= VBits - MaxLogTextureSize;
		VBits = MaxLogTextureSize;
		if (UBits < 0) {
			UCopyBits = -UBits;
			UBits = 0;
		}
	}

	// stijn: make an exception and draw the larger texture if the necessary mips don't exist
	if (BaseMip >= Info.NumMips && Info.NumMips > 0)
	{
		BaseMip = Info.NumMips - 1;

		INT TryUBits = Info.Mips[0]->UBits >> BaseMip;
		INT TryVBits = Info.Mips[0]->VBits >> BaseMip;

		// check if the hardware supports the lower mip
		if (m_d3dCaps.MaxTextureWidth  >= (1 << TryUBits) &&
			m_d3dCaps.MaxTextureHeight >= (1 << TryVBits))
		{
			// fine
			UBits = TryUBits;
			VBits = TryVBits;
			UCopyBits = 0;
			VCopyBits = 0;
		}
		else
		{
			// not fine. Just draw the a partial texture then. This will clearly signal
			// to the player that something is wrong, and it prevents us from crashing.
		}
	}

	pBind->BaseMip = BaseMip;
	MaxLevel = Min(UBits, VBits) - MinLogTextureSize;
	if (MaxLevel < 0) {
		MaxLevel = 0;
	}
	pBind->MaxLevel = MaxLevel;
	pBind->UBits = UBits;
	pBind->VBits = VBits;

	pBind->UMult = 1.0f / (Info.UScale * (Info.USize << UCopyBits));
	pBind->VMult = 1.0f / (Info.VScale * (Info.VSize << VCopyBits));

	pBind->UClampVal = Info.UClamp - 1;
	pBind->VClampVal = Info.VClamp - 1;

	//Check for texture that does not require clamping
	//No clamp required if ((Info.UClamp == Info.USize) & (Info.VClamp == Info.VSize))
	if (((Info.UClamp ^ Info.USize) | (Info.VClamp ^ Info.VSize)) == 0) {
		texFlags |= TEX_FLAG_NO_CLAMP;
	}


	//Determine texture type
	//PolyFlags PF_Masked cannot change if existing texture is updated as it caches texture type information here
	pBind->texType = TEX_TYPE_NONE; 
	if (SupportsTC) {
		switch (Info.Format) {
		case TEXF_DXT1:
			if (TexDXT1ToDXT3 && (!(PolyFlags & PF_Masked))) {
				pBind->texType = TEX_TYPE_COMPRESSED_DXT1_TO_DXT3;
				pBind->texFormat = D3DFMT_DXT3;
			}
			else {
				pBind->texType = TEX_TYPE_COMPRESSED_DXT1;
				pBind->texFormat = D3DFMT_DXT1;
			}
			break;
#if UNREAL_TOURNAMENT_OLDUNREAL || UTGLR_UNREAL_227_BUILD
		case TEXF_DXT3: 
			pBind->texType = TEX_TYPE_COMPRESSED_DXT3; 
			pBind->texFormat = D3DFMT_DXT3;
			break;

		case TEXF_DXT5: 
			pBind->texType = TEX_TYPE_COMPRESSED_DXT5; 
			pBind->texFormat = D3DFMT_DXT5;
			break;
#endif
		default:
			;
		}
	}
	if (pBind->texType != TEX_TYPE_NONE) {
		//Using compressed texture
	}
#if UNREAL_TOURNAMENT_OLDUNREAL
	else if (FIsPalettizedFormat(Info.Format)) {
#else
	else if (Info.Palette) {
#endif
		pBind->texType = TEX_TYPE_HAS_PALETTE;
		pBind->texFormat = D3DFMT_A8R8G8B8;
	}
	else {
		pBind->texType = TEX_TYPE_NORMAL;
		if (texFlags & TEX_FLAG_NO_CLAMP) {
			pBind->pConvertBGRA7777 = &UD3D9RenderDevice::ConvertBGRA7777_BGRA8888_NoClamp;
			pBind->pConvertBGRA8 = &UD3D9RenderDevice::ConvertBGRA8_BGRA8888_NoClamp;
			pBind->pConvertRGBA8 = &UD3D9RenderDevice::ConvertRGBA8_BGRA8888_NoClamp;
		}
		else {
			pBind->pConvertBGRA7777 = &UD3D9RenderDevice::ConvertBGRA7777_BGRA8888;
			pBind->pConvertBGRA8 = &UD3D9RenderDevice::ConvertBGRA8_BGRA8888;
			pBind->pConvertRGBA8 = &UD3D9RenderDevice::ConvertRGBA8_BGRA8888;
		}
		pBind->texFormat = D3DFMT_A8R8G8B8;
	}
	if (shouldGenHashTexture(Info)) {
		pBind->texType = TEX_TYPE_CACHE_GEN;
		pBind->texFormat = D3DFMT_A8R8G8B8;
	}

	return;
}


void UD3D9RenderDevice::ConvertDXT1_DXT1(const FMipmapBase *Mip, INT Level) {
	const DWORD *pSrc = (DWORD *)Mip->DataPtr;
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	DWORD UBlocks = 1U << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level - 2);
	DWORD VBlocks = 1U << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level - 2);

	DWORD USet = Min((INT)UBlocks*4, (Mip->USize + 3) >> 2 << 2) >> 1;
	DWORD UZero = UBlocks*2 - USet;
	for (DWORD v = 0; v < VBlocks; v++) {
		DWORD *pDest = pTex;
		if (v*4 < Mip->VSize) {
			appMemcpy(pDest, pSrc, sizeof(DWORD)*USet);
			pSrc += USet;
			pDest += USet;
		} else {
			UZero = UBlocks*2;
		}
		if (UZero)
			appMemzero(pDest, sizeof(DWORD)*UZero);
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	}

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertDXT1_DXT1 = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertDXT1_DXT3(const FMipmapBase *Mip, INT Level) {
	const DWORD *pSrc = (DWORD *)Mip->DataPtr;
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	DWORD UBlocks = 1U << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level - 2);
	DWORD VBlocks = 1U << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level - 2);

	for (DWORD v = 0; v < VBlocks; v++) {
		DWORD *pDest = pTex;
		BOOL bZero = v*4 >= Mip->VSize;
		for (DWORD u = 0; u < UBlocks; u++) {
			//Copy one block
			pDest[0] = 0xFFFFFFFF;
			pDest[1] = 0xFFFFFFFF;
			if (bZero || u*4 >= Mip->USize) {
				appMemzero(&pDest[2], sizeof(DWORD)*2);
			} else {
				appMemcpy(&pDest[2], pSrc, sizeof(DWORD)*2);
				pSrc += 2;
			}
			pDest += 4;
		}
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	}

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertDXT1_DXT3 = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertDXT35_DXT35(const FMipmapBase *Mip, INT Level) {
	const DWORD *pSrc = (DWORD *)Mip->DataPtr;
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	DWORD UBlocks = 1U << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level - 2);
	DWORD VBlocks = 1U << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level - 2);

	DWORD USet = Min((INT)UBlocks*4, (Mip->USize + 3) >> 2 << 2);
	DWORD UZero = UBlocks*4 - USet;
	for (DWORD v = 0; v < VBlocks; v++) {
		DWORD *pDest = pTex;
		if (v*4 < Mip->VSize) {
			appMemcpy(pDest, pSrc, sizeof(DWORD)*USet);
			pSrc += USet;
			pDest += USet;
		} else {
			UZero = UBlocks*4;
		}
		if (UZero)
			appMemzero(pDest, sizeof(DWORD)*UZero);
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	}

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertDXT35_DXT35 = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertP8_RGBA8888(const FMipmapBase *Mip, const FColor *Palette, INT Level) {
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	INT StepBits = m_texConvertCtx.stepBits;
	DWORD UMask = (1U << Mip->UBits) - 1;
	DWORD VMask = (1U << Mip->VBits) - 1;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		INT VOff = i & VMask;
		BYTE* Base = (BYTE*)Mip->DataPtr + VOff * Mip->USize;
		BOOL bZero = VOff >= Mip->VSize;
		INT j = 0;
		do { //j_stop always >= 1
			INT UOff = j & UMask;
			if (bZero || UOff >= Mip->USize)
				pTex[j] = 0;
			else {
				DWORD dwColor = GET_COLOR_DWORD(Palette[Base[UOff]]);
				pTex[j] = (dwColor & 0xFF00FF00) | ((dwColor >> 16) & 0xFF) | ((dwColor << 16) & 0xFF0000);
			}
		} while ((j += ij_inc) < j_stop);
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertP8_RGBA8888 = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertP8_RGBA8888_NoStep(const FMipmapBase *Mip, const FColor *Palette, INT Level) {
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	DWORD UMask = (1U << Mip->UBits) - 1;
	DWORD VMask = (1U << Mip->VBits) - 1;
	INT i_stop = m_texConvertCtx.texHeightPow2;
	INT j_stop = m_texConvertCtx.texWidthPow2;
	INT i = 0;
	do { //i_stop always >= 1
		INT VOff = i & VMask;
		BYTE* Base = (BYTE*)Mip->DataPtr + VOff * Mip->USize;
		BOOL bZero = VOff >= Mip->VSize;
		INT j = 0;
		do { //j_stop always >= 1
			INT UOff = j & UMask;
			if (bZero || UOff >= Mip->USize)
				pTex[j] = 0;
			else {
				DWORD dwColor = GET_COLOR_DWORD(Palette[Base[UOff]]);
				pTex[j] = (dwColor & 0xFF00FF00) | ((dwColor >> 16) & 0xFF) | ((dwColor << 16) & 0xFF0000);
			}	
		} while ((j += 1) < j_stop);
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += 1) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertP8_RGBA8888_NoStep = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertP8_RGB565(const FMipmapBase *Mip, const FColor *Palette, INT Level) {
	_WORD *pTex = (_WORD *)m_texConvertCtx.lockRect.pBits;
	INT StepBits = m_texConvertCtx.stepBits;
	DWORD UMask = (1U << Mip->UBits) - 1;
	DWORD VMask = (1U << Mip->VBits) - 1;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		INT VOff = i & VMask;
		BYTE* Base = (BYTE*)Mip->DataPtr + VOff * Mip->USize;
		BOOL bZero = VOff >= Mip->VSize;
		INT j = 0;
		do { //j_stop always >= 1
			INT UOff = j & UMask;
			if (bZero || UOff >= Mip->USize)
				pTex[j] = 0;
			else {
				DWORD dwColor = GET_COLOR_DWORD(Palette[Base[UOff]]);
				pTex[j] = ((dwColor >> 19) & 0x001F) | ((dwColor >> 5) & 0x07E0) | ((dwColor << 8) & 0xF800);
			}
		} while ((j += ij_inc) < j_stop);
		pTex = (WORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertP8_RGBA565 = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertP8_RGB565_NoStep(const FMipmapBase *Mip, const FColor *Palette, INT Level) {
	_WORD *pTex = (_WORD *)m_texConvertCtx.lockRect.pBits;
	DWORD UMask = (1U << Mip->UBits) - 1;
	DWORD VMask = (1U << Mip->VBits) - 1;
	INT i_stop = m_texConvertCtx.texHeightPow2;
	INT j_stop = m_texConvertCtx.texWidthPow2;
	INT i = 0;
	do { //i_stop always >= 1
		INT VOff = i & VMask;
		BYTE* Base = (BYTE*)Mip->DataPtr + VOff * Mip->USize;
		BOOL bZero = VOff >= Mip->VSize;
		INT j = 0;
		do { //j_stop always >= 1
			INT UOff = j & UMask;
			if (bZero || UOff >= Mip->USize)
				pTex[j] = 0;
			else {
				DWORD dwColor = GET_COLOR_DWORD(Palette[Base[UOff]]);
				pTex[j] = ((dwColor >> 19) & 0x001F) | ((dwColor >> 5) & 0x07E0) | ((dwColor << 8) & 0xF800);
			}
		} while ((j += 1) < j_stop);
		pTex = (_WORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += 1) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertP8_RGBA565_NoStep = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertP8_RGBA5551(const FMipmapBase *Mip, const FColor *Palette, INT Level) {
	_WORD *pTex = (_WORD *)m_texConvertCtx.lockRect.pBits;
	INT StepBits = m_texConvertCtx.stepBits;
	DWORD UMask = (1U << Mip->UBits) - 1;
	DWORD VMask = (1U << Mip->VBits) - 1;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		INT VOff = i & VMask;
		BYTE* Base = (BYTE*)Mip->DataPtr + VOff * Mip->USize;
		BOOL bZero = VOff >= Mip->VSize;
		INT j = 0;
		do { //j_stop always >= 1
			INT UOff = j & UMask;
			if (bZero || UOff >= Mip->USize)
				pTex[j] = 0;
			else {
				DWORD dwColor = GET_COLOR_DWORD(Palette[Base[UOff]]);
				pTex[j] = ((dwColor >> 19) & 0x001F) | ((dwColor >> 6) & 0x03E0) | ((dwColor << 7) & 0x7C00) | ((dwColor >> 16) & 0x8000);
			}
		} while ((j += ij_inc) < j_stop);
		pTex = (WORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertP8_RGBA5551 = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertP8_RGBA5551_NoStep(const FMipmapBase *Mip, const FColor *Palette, INT Level) {
	_WORD *pTex = (_WORD *)m_texConvertCtx.lockRect.pBits;
	DWORD UMask = (1U << Mip->UBits) - 1;
	DWORD VMask = (1U << Mip->VBits) - 1;
	INT i_stop = m_texConvertCtx.texHeightPow2;
	INT j_stop = m_texConvertCtx.texWidthPow2;
	INT i = 0;
	do { //i_stop always >= 1
		INT VOff = i & VMask;
		BYTE* Base = (BYTE*)Mip->DataPtr + VOff * Mip->USize;
		BOOL bZero = VOff >= Mip->VSize;
		INT j = 0;
		do { //j_stop always >= 1
			INT UOff = j & UMask;
			if (bZero || UOff >= Mip->USize)
				pTex[j] = 0;
			else {
				DWORD dwColor = GET_COLOR_DWORD(Palette[Base[UOff]]);
				pTex[j] = ((dwColor >> 19) & 0x001F) | ((dwColor >> 6) & 0x03E0) | ((dwColor << 7) & 0x7C00) | ((dwColor >> 16) & 0x8000);
			}
		} while ((j += 1) < j_stop);
		pTex = (_WORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += 1) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertP8_RGBA5551_NoStep = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertBGRA7777_BGRA8888(const FMipmapBase *Mip, INT Level) {
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	INT StepBits = m_texConvertCtx.stepBits;
	DWORD VMask = (1U << Mip->VBits) - 1;
	DWORD VClampVal = m_texConvertCtx.pBind->VClampVal;
	DWORD UMask = (1U << Mip->UBits) - 1;
	DWORD UClampVal = m_texConvertCtx.pBind->UClampVal;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		INT VOff = i & VMask;
		FColor* Base = (FColor*)Mip->DataPtr + Min<DWORD>(VOff, VClampVal) * Mip->USize;
		BOOL bZero = VOff >= Mip->VSize;
		INT j = 0;
		do { //j_stop always >= 1;
			INT UOff = j & UMask;
			if (bZero || UOff >= Mip->USize)
				pTex[j] = 0;
			else {
				DWORD dwColor = GET_COLOR_DWORD(Base[Min<DWORD>(UOff, UClampVal)]);
				pTex[j] = dwColor * 2; // because of 7777
			}
		} while ((j += ij_inc) < j_stop);
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertBGRA7777_BGRA8888 = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertBGRA7777_BGRA8888_NoClamp(const FMipmapBase *Mip, INT Level) {
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	INT StepBits = m_texConvertCtx.stepBits;
	DWORD VMask = (1U << Mip->VBits) - 1;
	DWORD UMask = (1U << Mip->UBits) - 1;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		INT VOff = i & VMask;
		FColor* Base = (FColor*)Mip->DataPtr + VOff * Mip->USize;
		BOOL bZero = VOff >= Mip->VSize;
		INT j = 0;
		do { //j_stop always >= 1;
			INT UOff = j & UMask;
			if (bZero || UOff >= Mip->USize)
				pTex[j] = 0;
			else {
				DWORD dwColor = GET_COLOR_DWORD(Base[UOff]);
				pTex[j] = dwColor * 2; // because of 7777
			}
		} while ((j += ij_inc) < j_stop);
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertBGRA7777_BGRA8888_NoClamp = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertBGRA8_BGRA8888(const FMipmapBase *Mip, INT Level) {
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	INT StepBits = m_texConvertCtx.stepBits;
	DWORD VMask = (1U << Mip->VBits) - 1;
	DWORD VClampVal = m_texConvertCtx.pBind->VClampVal;
	DWORD UMask = (1U << Mip->UBits) - 1;
	DWORD UClampVal = m_texConvertCtx.pBind->UClampVal;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		INT VOff = i & VMask;
		FColor* Base = (FColor*)Mip->DataPtr + Min<DWORD>(VOff, VClampVal) * Mip->USize;
		BOOL bZero = VOff >= Mip->VSize;
		INT j = 0;
		do { //j_stop always >= 1;
			INT UOff = j & UMask;
			if (bZero || UOff >= Mip->USize)
				pTex[j] = 0;
			else {
				DWORD dwColor = GET_COLOR_DWORD(Base[Min<DWORD>(UOff, UClampVal)]);
				pTex[j] = dwColor;
			}
		} while ((j += ij_inc) < j_stop);
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertBGRA7777_BGRA8888 = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertBGRA8_BGRA8888_NoClamp(const FMipmapBase *Mip, INT Level) {
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	INT StepBits = m_texConvertCtx.stepBits;
	DWORD VMask = (1U << Mip->VBits) - 1;
	DWORD UMask = (1U << Mip->UBits) - 1;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		INT VOff = i & VMask;
		FColor* Base = (FColor*)Mip->DataPtr + VOff * Mip->USize;
		BOOL bZero = VOff >= Mip->VSize;
		INT j = 0;
		do { //j_stop always >= 1;
			INT UOff = j & UMask;
			if (bZero || UOff >= Mip->USize)
				pTex[j] = 0;
			else {
				DWORD dwColor = GET_COLOR_DWORD(Base[UOff]);
				pTex[j] = dwColor;
			}
		} while ((j += ij_inc) < j_stop);
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertBGRA7777_BGRA8888_NoClamp = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertRGBA8_BGRA8888(const FMipmapBase *Mip, INT Level) {
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	INT StepBits = m_texConvertCtx.stepBits;
	DWORD VMask = (1U << Mip->VBits) - 1;
	DWORD VClampVal = m_texConvertCtx.pBind->VClampVal;
	DWORD UMask = (1U << Mip->UBits) - 1;
	DWORD UClampVal = m_texConvertCtx.pBind->UClampVal;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		INT VOff = i & VMask;
		FColor* Base = (FColor*)Mip->DataPtr + Min<DWORD>(VOff, VClampVal) * Mip->USize;
		BOOL bZero = VOff >= Mip->VSize;
		INT j = 0;
		do { //j_stop always >= 1;
			INT UOff = j & UMask;
			if (bZero || UOff >= Mip->USize)
				pTex[j] = 0;
			else {
				DWORD dwColor = GET_COLOR_DWORD(Base[Min<DWORD>(UOff, UClampVal)]);
				pTex[j] = (dwColor & 0xFF00FF00) | ((dwColor >> 16) & 0xFF) | ((dwColor << 16) & 0xFF0000);
			}
		} while ((j += ij_inc) < j_stop);
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertBGRA7777_BGRA8888 = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::ConvertRGBA8_BGRA8888_NoClamp(const FMipmapBase *Mip, INT Level) {
	DWORD *pTex = (DWORD *)m_texConvertCtx.lockRect.pBits;
	INT StepBits = m_texConvertCtx.stepBits;
	DWORD VMask = (1U << Mip->VBits) - 1;
	DWORD UMask = (1U << Mip->UBits) - 1;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		INT VOff = i & VMask;
		FColor* Base = (FColor*)Mip->DataPtr + VOff * Mip->USize;
		BOOL bZero = VOff >= Mip->VSize;
		INT j = 0;
		do { //j_stop always >= 1;
			INT UOff = j & UMask;
			if (bZero || UOff >= Mip->USize)
				pTex[j] = 0;
			else {
				DWORD dwColor = GET_COLOR_DWORD(Base[UOff]);
				pTex[j] = (dwColor & 0xFF00FF00) | ((dwColor >> 16) & 0xFF) | ((dwColor << 16) & 0xFF0000);
			}
		} while ((j += ij_inc) < j_stop);
		pTex = (DWORD *)((BYTE *)pTex + m_texConvertCtx.lockRect.Pitch);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utd3d9r: ConvertBGRA7777_BGRA8888_NoClamp = " << si++ << std::endl;
	}
#endif
}

void UD3D9RenderDevice::SetBlendNoCheck(DWORD blendFlags, bool isUI) {
	guardSlow(UD3D9RenderDevice::SetBlend);

	if (isUI) { // bit of a hack to ensure D3DRS_ZWRITEENABLE is FALSE for ui stuff so remix picks it up correctly
		blendFlags &= ~PF_Occlude;
	}

	// Detect changes in the blending modes.
	DWORD Xor = m_curBlendFlags ^ blendFlags;

	//Save copy of current blend flags
	DWORD curBlendFlags = m_curBlendFlags;

	//Update main copy of current blend flags early
	m_curBlendFlags = blendFlags;

	const DWORD GL_BLEND_FLAG_BITS = PF_Translucent | PF_Modulated | PF_Highlighted | PF_AlphaBlend | PF_NotSolid;
	DWORD relevantBlendFlagBits = GL_BLEND_FLAG_BITS | m_smoothMaskedTexturesBit;

	if (Xor & (relevantBlendFlagBits)) {
		if (!(blendFlags & (relevantBlendFlagBits))) {
			m_d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			m_d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
			m_d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
		}
		else {
			if ( !(curBlendFlags & relevantBlendFlagBits) ) {
				m_d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			}
			if (blendFlags & PF_Translucent) {
				m_d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
				// hack so remix renders translucent stuff without masking white values
				// https://github.com/NVIDIAGameWorks/rtx-remix/issues/392
				m_d3dDevice->SetRenderState(D3DRS_DESTBLEND, isUI ? D3DBLEND_INVSRCCOLOR : D3DBLEND_ONE);
			}
			else if (blendFlags & PF_Modulated) {
				m_d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
				m_d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
			}
			else if (blendFlags & PF_Highlighted) {
				m_d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
				m_d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			}
			else if (blendFlags & (PF_Masked|PF_AlphaBlend)) {
				m_d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
				m_d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			}
			else if ((blendFlags & PF_NotSolid) && !(blendFlags & PF_TwoSided)) {
				// Make non solid surfaces translucent so that light can shine through them
				m_d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
				m_d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
			}
		}
	}
	if (Xor & (PF_Masked|PF_AlphaBlend)) {
		if (blendFlags & PF_AlphaBlend) {
			m_fsBlendInfo[0] = 0.01f;

			m_alphaTestEnabled = true;
			m_d3dDevice->SetRenderState(D3DRS_ALPHAREF, 1);
			m_d3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);

		}
		else if (blendFlags & PF_Masked) {
			//Enable alpha test with alpha ref of D3D9 version of 0.5
			m_fsBlendInfo[0] = (127.0f / 255.0f) + 1e-6f;

			m_alphaTestEnabled = true;
			m_d3dDevice->SetRenderState(D3DRS_ALPHAREF, 127);
			m_d3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
		}
		else {
			//Disable alpha test
			m_fsBlendInfo[0] = 0.0f;

			m_alphaTestEnabled = false;
			m_d3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		}
	}
	if (Xor & PF_Invisible) {
		DWORD colorEnableBits = ((blendFlags & PF_Invisible) == 0) ? D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_RED : 0;
		m_d3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE, colorEnableBits);
	}
	if (Xor & PF_Occlude) {
		DWORD flag = ((blendFlags & PF_Occlude) == 0) ? FALSE : TRUE;
		m_d3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, flag);
	}
	if (Xor & PF_RenderFog) {
		DWORD flag = ((blendFlags & PF_RenderFog) == 0) ? FALSE : TRUE;
		m_d3dDevice->SetRenderState(D3DRS_SPECULARENABLE, flag);
	}
	if (Xor & PF_TwoSided) {
		D3DCULL flag = ((blendFlags & PF_TwoSided) == 0) ? D3DCULL_CCW : D3DCULL_NONE;
		m_d3dDevice->SetRenderState(D3DRS_CULLMODE, flag);
	}

	unguardSlow;
}

//This function will initialize or invalidate the texture environment state
//The current architecture allows both operations to be done in the same way
void UD3D9RenderDevice::InitOrInvalidateTexEnvState(void) {
	INT TMU;

	//For initialization, flags for all texture units are cleared
	//For initialization, first texture unit is modulated by default rather
	//than disabled, but priority bit encoding of flags will prevent problems
	//from the mismatch and will only result in one extra state update
	//For invalidation, flags for all texture units are also cleared as it is
	//fast enough and has no potential outside interaction side effects
	for (TMU = 0; TMU < MAX_TMUNITS; TMU++) {
		m_curTexEnvFlags[TMU] = 0;
	}

	//Set TexEnv 0 to modulated by default
	SetTexEnv(0, PF_Modulated);

	return;
}

void UD3D9RenderDevice::SetTexLODBiasState(INT TMUnits) {
	INT TMU;

	//Set texture LOD bias for all texture units
	for (TMU = 0; TMU < TMUnits; TMU++) {
		float fParam;

		//Set texture LOD bias
		fParam = LODBias;
		m_d3dDevice->SetSamplerState(TMU, D3DSAMP_MIPMAPLODBIAS, *(DWORD *)&fParam);
	}

	return;
}

void UD3D9RenderDevice::SetTexMaxAnisotropyState(INT TMUnits) {
	INT TMU;

	//Set maximum level of anisotropy for all texture units
	for (TMU = 0; TMU < TMUnits; TMU++) {
		m_d3dDevice->SetSamplerState(TMU, D3DSAMP_MAXANISOTROPY, MaxAnisotropy);
	}

	return;
}

void UD3D9RenderDevice::SetTexEnvNoCheck(DWORD texUnit, DWORD texEnvFlags) {
	guardSlow(UD3D9RenderDevice::SetTexEnv);

	//Update current tex env flags early as there are no subsequent dependencies
	m_curTexEnvFlags[texUnit] = texEnvFlags;

	//Mark the texture unit as enabled
	m_texEnableBits |= 1U << texUnit;

	if (texEnvFlags & PF_Modulated) {
		D3DTEXTUREOP texOp;

		if ((texEnvFlags & PF_FlatShaded) || (texUnit != 0) && !OneXBlending) {
			texOp = D3DTOP_MODULATE2X;
		}
		else {
			texOp = D3DTOP_MODULATE;
		}

		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLOROP, texOp);
		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

//		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLORARG2, D3DTA_CURRENT);
	}
	else if (texEnvFlags & PF_Memorized) {
		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLOROP, D3DTOP_BLENDCURRENTALPHA);
		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

//		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	}
	else if (texEnvFlags & PF_Highlighted) {
		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLOROP, D3DTOP_MODULATEINVALPHA_ADDCOLOR);
		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);

//		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		m_d3dDevice->SetTextureStageState(texUnit, D3DTSS_COLORARG2, D3DTA_CURRENT);
	}

	unguardSlow;
}


void UD3D9RenderDevice::SetTexFilterNoCheck(DWORD texNum, BYTE texFilterParams) {
	guardSlow(UD3D9RenderDevice::SetTexFilter);

	BYTE texFilterParamsXor = m_curTexStageParams[texNum].filter ^ texFilterParams;

	//Update main copy of current tex filter params early
	m_curTexStageParams[texNum].filter = texFilterParams;

	if (texFilterParamsXor & CT_MIN_FILTER_MASK) {
		D3DTEXTUREFILTERTYPE texFilterType = D3DTEXF_POINT;

		switch (texFilterParams & CT_MIN_FILTER_MASK) {
		case CT_MIN_FILTER_POINT: texFilterType = D3DTEXF_POINT; break;
		case CT_MIN_FILTER_LINEAR: texFilterType = D3DTEXF_LINEAR; break;
		case CT_MIN_FILTER_ANISOTROPIC: texFilterType = D3DTEXF_ANISOTROPIC; break;
		default:
			;
		}

		m_d3dDevice->SetSamplerState(texNum, D3DSAMP_MINFILTER, texFilterType);
	}
	if (texFilterParamsXor & CT_MIP_FILTER_MASK) {
		D3DTEXTUREFILTERTYPE texFilterType = D3DTEXF_NONE;

		switch (texFilterParams & CT_MIP_FILTER_MASK) {
		case CT_MIP_FILTER_NONE: texFilterType = D3DTEXF_NONE; break;
		case CT_MIP_FILTER_POINT: texFilterType = D3DTEXF_POINT; break;
		case CT_MIP_FILTER_LINEAR: texFilterType = D3DTEXF_LINEAR; break;
		default:
			;
		}

		m_d3dDevice->SetSamplerState(texNum, D3DSAMP_MIPFILTER, texFilterType);
	}
	if (texFilterParamsXor & CT_MAG_FILTER_LINEAR_NOT_POINT_BIT) {
		m_d3dDevice->SetSamplerState(texNum, D3DSAMP_MAGFILTER, (texFilterParams & CT_MAG_FILTER_LINEAR_NOT_POINT_BIT) ? D3DTEXF_LINEAR : D3DTEXF_POINT);
	}
	if (texFilterParamsXor & CT_ADDRESS_CLAMP_NOT_WRAP_BIT) {
		D3DTEXTUREADDRESS texAddressMode = (texFilterParams & CT_ADDRESS_CLAMP_NOT_WRAP_BIT) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP;
		m_d3dDevice->SetSamplerState(texNum, D3DSAMP_ADDRESSU, texAddressMode);
		m_d3dDevice->SetSamplerState(texNum, D3DSAMP_ADDRESSV, texAddressMode);
	}

	unguardSlow;
}


void UD3D9RenderDevice::SetVertexDeclNoCheck(IDirect3DVertexDeclaration9 *vertexDecl) {
	HRESULT hResult;

	//Set vertex declaration
	hResult = m_d3dDevice->SetVertexDeclaration(vertexDecl);
	if (FAILED(hResult)) {
		appErrorf(TEXT("SetVertexDeclaration failed"));
	}

	//Save new current vertex declaration
	m_curVertexDecl = vertexDecl;

	return;
}


void UD3D9RenderDevice::RenderPassesExec(void) {
	guard(UD3D9RenderDevice::RenderPassesExec);

	//Some render passes paths may use fragment program

	if (m_rpMasked && m_rpForceSingle && !m_rpSetDepthEqual) {
		m_d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_EQUAL);
		m_rpSetDepthEqual = true;
	}

	//Call the render passes no check setup proc
	RenderPassesNoCheckSetup();

	m_rpTMUnits = 1;
	m_rpForceSingle = true;


	//for (INT PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++) {
	//	assert(MultiDrawCountArray[PolyNum] == 3);
	//	//m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos + MultiDrawFirstArray[PolyNum], MultiDrawCountArray[PolyNum] - 2);
	//}
	int ptCount = m_csVertexArray.size();
	INT bufferPos = getVertBufferPos(ptCount);
	m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, bufferPos, ptCount / 3);

#ifdef UTGLR_DEBUG_WORLD_WIREFRAME
	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);

	SetBlend(PF_Modulated);

	for (PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++) {
		m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, bufferPos + MultiDrawFirstArray[PolyNum], MultiDrawCountArray[PolyNum] - 2);
	}

	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
#endif

#if 0
{
	dout << L"utd3d9r: PassCount = " << m_rpPassCount << std::endl;
}
#endif
	m_rpPassCount = 0;


	unguard;
}

//Must be called with (m_rpPassCount > 0)
void UD3D9RenderDevice::RenderPassesNoCheckSetup(void) {
	INT i;
	INT t;

	SetBlend(MultiPass.TMU[0].PolyFlags);

	i = 0;
	do {
		if (i != 0) {
			SetTexEnv(i, MultiPass.TMU[i].PolyFlags);
		}

		SetTexture(i, *MultiPass.TMU[i].Info, MultiPass.TMU[i].PolyFlags, MultiPass.TMU[i].PanBias);
	} while (++i < m_rpPassCount);

	//Set stream state based on number of texture units in use
	SetStreamState(m_standardNTextureVertexDecl[m_rpPassCount - 1]);

	//Check for additional enabled texture units that should be disabled
	DisableSubsequentTextures(m_rpPassCount);

	int ptCount = m_csVertexArray.size();
	//Make sure at least m_csPtCount entries are left in the vertex buffers
	if ((m_curVertexBufferPos + ptCount) >= VERTEX_BUFFER_SIZE) {
		FlushVertexBuffers();
	}

	//Lock vertexColor and texCoord buffers
	LockVertexColorBuffer(ptCount);
	t = 0;
	do {
		LockTexCoordBuffer(t, ptCount);
	} while (++t < m_rpPassCount);

	//Write vertex and color
	FGLVertexColor *pVertexColorArray = m_pVertexColorArray;
	DWORD rpColor = m_rpColor;
	for (FGLVertexNormTex& vert : m_csVertexArray) {
		pVertexColorArray->x = vert.vert.x;
		pVertexColorArray->y = vert.vert.y;
		pVertexColorArray->z = vert.vert.z;
		pVertexColorArray->norm.x = vert.norm.x;
		pVertexColorArray->norm.y = vert.norm.y;
		pVertexColorArray->norm.z = vert.norm.z;
		pVertexColorArray->color = rpColor;
		pVertexColorArray++;
	}

	//Write texCoord
	t = 0;
	do {
		FLOAT UPan = TexInfo[t].UPan;
		FLOAT VPan = TexInfo[t].VPan;
		FLOAT UMult = TexInfo[t].UMult;
		FLOAT VMult = TexInfo[t].VMult;
		FGLTexCoord *pTexCoord = m_pTexCoordArray[t];

		INT ptCounter = ptCount;
		for (FGLVertexNormTex& vert : m_csVertexArray) {
			pTexCoord->u = (vert.tex.u - UPan) * UMult;
			pTexCoord->v = (vert.tex.v - VPan) * VMult;

			pTexCoord++;
		} while (--ptCounter != 0);
	} while (++t < m_rpPassCount);

	//Unlock vertexColor and texCoord buffers
	UnlockVertexColorBuffer();
	t = 0;
	do {
		UnlockTexCoordBuffer(t);
	} while (++t < m_rpPassCount);

	return;
}

INT UD3D9RenderDevice::BufferStaticComplexSurfaceGeometry(const FSurfaceFacet& Facet, bool append) {
	if (!append) {
		m_csVertexArray.clear();
	}

	// Buffer "static" geometry.
	for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next) {
		//Skip if not enough points
		INT NumPts = Poly->NumPts;
		if (NumPts <= 2) {
			continue;
		}
		INT numPolys = NumPts - 2;

		m_csVertexArray.reserve(m_csVertexArray.size() + NumPts);

		FTransform** pPts = &Poly->Pts[0];
		const FVector* hubPoint = &(*pPts++)->Point;
		const FVector* secondPoint = &(*pPts++)->Point;
		do {
			const FVector* point = &(*pPts++)->Point;
			FGLVertexNormTex* bufVert = &m_csVertexArray.emplace_back();
			bufVert->vert.x = hubPoint->X;
			bufVert->vert.y = hubPoint->Y;
			bufVert->vert.z = hubPoint->Z;
			bufVert->norm.x = 0;
			bufVert->norm.y = 0;
			bufVert->norm.z = 0;
			bufVert->tex.u = (Facet.MapCoords.XAxis | *hubPoint) - m_csUDot;
			bufVert->tex.v = (Facet.MapCoords.YAxis | *hubPoint) - m_csVDot;

			bufVert = &m_csVertexArray.emplace_back();
			bufVert->vert.x = secondPoint->X;
			bufVert->vert.y = secondPoint->Y;
			bufVert->vert.z = secondPoint->Z;
			bufVert->norm.x = 0;
			bufVert->norm.y = 0;
			bufVert->norm.z = 0;
			bufVert->tex.u = (Facet.MapCoords.XAxis | *secondPoint) - m_csUDot;
			bufVert->tex.v = (Facet.MapCoords.YAxis | *secondPoint) - m_csVDot;

			bufVert = &m_csVertexArray.emplace_back();
			bufVert->vert.x = point->X;
			bufVert->vert.y = point->Y;
			bufVert->vert.z = point->Z;
			bufVert->norm.x = 0;
			bufVert->norm.y = 0;
			bufVert->norm.z = 0;
			bufVert->tex.u = (Facet.MapCoords.XAxis | *point) - m_csUDot;
			bufVert->tex.v = (Facet.MapCoords.YAxis | *point) - m_csVDot;

			secondPoint = point;
		} while (--numPolys != 0);
	}

	return m_csVertexArray.size();
}

void UD3D9RenderDevice::EndBufferingNoCheck(void) {
	switch (m_bufferedVertsType) {
	case BV_TYPE_GOURAUD_POLYS:
		EndGouraudPolygonBufferingNoCheck();
		break;

	case BV_TYPE_TILES:
		EndTileBufferingNoCheck();
		break;

	case BV_TYPE_LINES:
		EndLineBufferingNoCheck();
		break;

	case BV_TYPE_POINTS:
		EndPointBufferingNoCheck();
		break;

	default:
		;
	}

	m_bufferedVerts = 0;

	return;
}

void UD3D9RenderDevice::EndGouraudPolygonBufferingNoCheck(void) {
	//EndGouraudPolygonBufferingNoCheck sets its own projection state
	//Stream state set when start buffering
	//Default texture state set when start buffering

	clockFast(GouraudCycles);

	//Unlock vertexColor and texCoord0 buffers
	//Unlock secondary color buffer if fog
	UnlockVertexColorBuffer();
	if (m_requestedColorFlags & CF_FOG_MODE) {
		UnlockSecondaryColorBuffer();
	}
	UnlockTexCoordBuffer(0);

#ifdef UTGLR_DEBUG_ACTOR_WIREFRAME
	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
#endif

	//Draw the triangles
	m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, getVertBufferPos(m_bufferedVerts), m_bufferedVerts / 3);

#ifdef UTGLR_DEBUG_ACTOR_WIREFRAME
	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
#endif

	unclockFast(GouraudCycles);
}

void UD3D9RenderDevice::EndTileBufferingNoCheck(void) {
	//Stream state set when start buffering
	//Default texture state set when start buffering

	clockFast(TileCycles);

	//Unlock vertexColor and texCoord0 buffers
	UnlockVertexColorBuffer();
	UnlockTexCoordBuffer(0);

	//Draw the quads (stored as triangles)
	m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, getVertBufferPos(m_bufferedVerts), m_bufferedVerts / 3);

	unclockFast(TileCycles);
}

void UD3D9RenderDevice::EndLineBufferingNoCheck(void) {
	//AA state set when start buffering
	//Projection state set when start buffering
	//Stream state set when start buffering
	//Default texture state set when start buffering

	//Unlock vertexColor and texCoord0 buffers
	UnlockVertexColorBuffer();
	UnlockTexCoordBuffer(0);

	//Draw the lines
	m_d3dDevice->DrawPrimitive(D3DPT_LINELIST, getVertBufferPos(m_bufferedVerts), m_bufferedVerts / 2);
}

void UD3D9RenderDevice::EndPointBufferingNoCheck(void) {
	//AA state set when start buffering
	//Projection state set when start buffering
	//Stream state set when start buffering
	//Default texture state set when start buffering

	//Unlock vertexColor and texCoord0 buffers
	UnlockVertexColorBuffer();
	UnlockTexCoordBuffer(0);

	//Draw the points (stored as triangles)
	m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, getVertBufferPos(m_bufferedVerts), m_bufferedVerts / 3);
}

void UD3D9RenderDevice::startWorldDraw(FSceneNode* frame) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: startWorldDraw = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::startWorldDraw);
	using namespace DirectX;

	// Setup projection and view matrices for current frame
#if DEUS_EX
	float aspect = (float)m_SetRes_NewX / (float)m_SetRes_NewY;
#else
	float aspect = frame->FX / frame->FY;
#endif
	float fov = Viewport->Actor->FovAngle * PI / 180.0f;
	fov = 2.0 * atan(tan(fov * 0.5) / aspect);
	D3DMATRIX proj = ToD3DMATRIX(
		XMMatrixPerspectiveFovLH(fov, aspect, 0.5f, 65536.0f)
		);

	m_d3dDevice->SetTransform(D3DTS_PROJECTION, &proj);

	FVector origin = frame->Coords.Origin;
	FVector forward = frame->Coords.ZAxis;
	FVector up = frame->Coords.YAxis * -1.0f; // Yeah I don't get it.

	D3DMATRIX view = ToD3DMATRIX(
		XMMatrixLookToLH(
			FVecToDXVec(origin),
			FVecToDXVec(forward),
			FVecToDXVec(up)
		)
	);

	m_d3dDevice->SetTransform(D3DTS_VIEW, &view);
	m_d3dDevice->SetTransform(D3DTS_WORLD, &identityMatrix);
	m_d3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
	unguard;
}


void UD3D9RenderDevice::endWorldDraw(FSceneNode* frame) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: endWorldDraw = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::endWorldDraw);
	using namespace DirectX;

	// World drawing finished, setup for ui
	D3DMATRIX proj = ToD3DMATRIX(
		XMMatrixOrthographicOffCenterLH(0.5f, m_SetRes_NewX + 0.5, m_SetRes_NewY + 0.5, 0.5f, 0.1f, 1.0f)
	);
	m_d3dDevice->SetTransform(D3DTS_PROJECTION, &proj);
	m_d3dDevice->SetTransform(D3DTS_WORLD, &identityMatrix);
	m_d3dDevice->SetTransform(D3DTS_VIEW, &identityMatrix);
	m_d3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	unguard;
}


// Static variables.
INT UD3D9RenderDevice::NumDevices = 0;
INT UD3D9RenderDevice::LockCount = 0;

HMODULE UD3D9RenderDevice::hModuleD3d9 = NULL;
LPDIRECT3DCREATE9 UD3D9RenderDevice::pDirect3DCreate9 = NULL;

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
