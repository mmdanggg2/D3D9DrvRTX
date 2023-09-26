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

#include "D3D9Drv.h"
#include "..\Inc\UTGLRD3D9.h"
#include <DirectXMath.h>
#include "Render.h"

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

static const TCHAR *g_pSection = TEXT("D3D9Drv.D3D9RenderDevice");

//Stream definitions
////////////////////

static const D3DVERTEXELEMENT9 g_oneColorStreamDef[] = {
	{ 0, 0,  D3DDECLTYPE_FLOAT3,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,	0 },
	{ 0, 12, D3DDECLTYPE_D3DCOLOR,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,		0 },
	D3DDECL_END()
};

static const D3DVERTEXELEMENT9 g_standardSingleTextureStreamDef[] = {
	{ 0, 0,  D3DDECLTYPE_FLOAT3,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,	0 },
	{ 0, 12, D3DDECLTYPE_D3DCOLOR,	D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,		0 },
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


/*-----------------------------------------------------------------------------
	D3D9Drv.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UD3D9RenderDevice);


void UD3D9RenderDevice::StaticConstructor() {
	unsigned int u;

	guard(UD3D9RenderDevice::StaticConstructor);

#ifdef UTGLR_DX_BUILD
	const UBOOL UTGLR_DEFAULT_OneXBlending = 1;
#else
	const UBOOL UTGLR_DEFAULT_OneXBlending = 0;
#endif

#if defined UTGLR_DX_BUILD || defined UTGLR_RUNE_BUILD
	const UBOOL UTGLR_DEFAULT_UseS3TC = 0;
#else
	const UBOOL UTGLR_DEFAULT_UseS3TC = 1;
#endif

#if defined UTGLR_DX_BUILD || defined UTGLR_RUNE_BUILD
	const UBOOL UTGLR_DEFAULT_ZRangeHack = 0;
#else
	const UBOOL UTGLR_DEFAULT_ZRangeHack = 1;
#endif

#define CPP_PROPERTY_LOCAL(_name) _name, CPP_PROPERTY(_name)
#define CPP_PROPERTY_LOCAL_DCV(_name) DCV._name, CPP_PROPERTY(DCV._name)

	//Set parameter defaults and add parameters
	SC_AddFloatConfigParam(TEXT("LODBias"), CPP_PROPERTY_LOCAL(LODBias), 0.0f);
	SC_AddFloatConfigParam(TEXT("GammaOffset"), CPP_PROPERTY_LOCAL(GammaOffset), 0.0f);
	SC_AddFloatConfigParam(TEXT("GammaOffsetRed"), CPP_PROPERTY_LOCAL(GammaOffsetRed), 0.0f);
	SC_AddFloatConfigParam(TEXT("GammaOffsetGreen"), CPP_PROPERTY_LOCAL(GammaOffsetGreen), 0.0f);
	SC_AddFloatConfigParam(TEXT("GammaOffsetBlue"), CPP_PROPERTY_LOCAL(GammaOffsetBlue), 0.0f);
	SC_AddBoolConfigParam(1,  TEXT("GammaCorrectScreenshots"), CPP_PROPERTY_LOCAL(GammaCorrectScreenshots), 0);
	SC_AddBoolConfigParam(0,  TEXT("OneXBlending"), CPP_PROPERTY_LOCAL(OneXBlending), UTGLR_DEFAULT_OneXBlending);
	SC_AddIntConfigParam(TEXT("MinLogTextureSize"), CPP_PROPERTY_LOCAL(MinLogTextureSize), 0);
	SC_AddIntConfigParam(TEXT("MaxLogTextureSize"), CPP_PROPERTY_LOCAL(MaxLogTextureSize), 8);
	SC_AddBoolConfigParam(4,  TEXT("UsePrecache"), CPP_PROPERTY_LOCAL(UsePrecache), 0);
	SC_AddBoolConfigParam(3,  TEXT("UseTrilinear"), CPP_PROPERTY_LOCAL(UseTrilinear), 0);
	SC_AddBoolConfigParam(2,  TEXT("UseS3TC"), CPP_PROPERTY_LOCAL(UseS3TC), UTGLR_DEFAULT_UseS3TC);
	SC_AddBoolConfigParam(1,  TEXT("Use16BitTextures"), CPP_PROPERTY_LOCAL(Use16BitTextures), 0);
	SC_AddBoolConfigParam(0,  TEXT("Use565Textures"), CPP_PROPERTY_LOCAL(Use565Textures), 0);
	SC_AddIntConfigParam(TEXT("MaxAnisotropy"), CPP_PROPERTY_LOCAL(MaxAnisotropy), 0);
	SC_AddBoolConfigParam(0,  TEXT("NoFiltering"), CPP_PROPERTY_LOCAL(NoFiltering), 0);
	SC_AddIntConfigParam(TEXT("MaxTMUnits"), CPP_PROPERTY_LOCAL(MaxTMUnits), 0);
	SC_AddIntConfigParam(TEXT("RefreshRate"), CPP_PROPERTY_LOCAL(RefreshRate), 0);
	SC_AddIntConfigParam(TEXT("DetailMax"), CPP_PROPERTY_LOCAL(DetailMax), 0);
	SC_AddBoolConfigParam(8,  TEXT("DetailClipping"), CPP_PROPERTY_LOCAL(DetailClipping), 0);
	SC_AddBoolConfigParam(7,  TEXT("ColorizeDetailTextures"), CPP_PROPERTY_LOCAL(ColorizeDetailTextures), 0);
	SC_AddBoolConfigParam(6,  TEXT("SinglePassFog"), CPP_PROPERTY_LOCAL(SinglePassFog), 1);
	SC_AddBoolConfigParam(5,  TEXT("SinglePassDetail"), CPP_PROPERTY_LOCAL_DCV(SinglePassDetail), 0);
	SC_AddBoolConfigParam(4,  TEXT("UseSSE"), CPP_PROPERTY_LOCAL(UseSSE), 1);
	SC_AddBoolConfigParam(3,  TEXT("UseSSE2"), CPP_PROPERTY_LOCAL(UseSSE2), 1);
	SC_AddBoolConfigParam(2,  TEXT("UseTexIdPool"), CPP_PROPERTY_LOCAL(UseTexIdPool), 1);
	SC_AddBoolConfigParam(1,  TEXT("UseTexPool"), CPP_PROPERTY_LOCAL(UseTexPool), 1);
	SC_AddIntConfigParam(TEXT("DynamicTexIdRecycleLevel"), CPP_PROPERTY_LOCAL(DynamicTexIdRecycleLevel), 100);
	SC_AddBoolConfigParam(1,  TEXT("TexDXT1ToDXT3"), CPP_PROPERTY_LOCAL(TexDXT1ToDXT3), 0);
	SC_AddIntConfigParam(TEXT("SwapInterval"), CPP_PROPERTY_LOCAL(SwapInterval), -1);
	SC_AddIntConfigParam(TEXT("FrameRateLimit"), CPP_PROPERTY_LOCAL(FrameRateLimit), 0);
#if UTGLR_USES_SCENENODEHACK
	SC_AddBoolConfigParam(6,  TEXT("SceneNodeHack"), CPP_PROPERTY_LOCAL(SceneNodeHack), 1);
#endif
	SC_AddBoolConfigParam(5,  TEXT("SmoothMaskedTextures"), CPP_PROPERTY_LOCAL(SmoothMaskedTextures), 0);
	SC_AddBoolConfigParam(4,  TEXT("MaskedTextureHack"), CPP_PROPERTY_LOCAL(MaskedTextureHack), 1);
	SC_AddBoolConfigParam(3,  TEXT("UseTripleBuffering"), CPP_PROPERTY_LOCAL(UseTripleBuffering), 0);
	SC_AddBoolConfigParam(2,  TEXT("UsePureDevice"), CPP_PROPERTY_LOCAL(UsePureDevice), 1);
	SC_AddBoolConfigParam(1,  TEXT("UseSoftwareVertexProcessing"), CPP_PROPERTY_LOCAL(UseSoftwareVertexProcessing), 0);
	SC_AddBoolConfigParam(0,  TEXT("UseAA"), CPP_PROPERTY_LOCAL(UseAA), 0);
	SC_AddIntConfigParam(TEXT("NumAASamples"), CPP_PROPERTY_LOCAL(NumAASamples), 4);
	SC_AddBoolConfigParam(1,  TEXT("NoAATiles"), CPP_PROPERTY_LOCAL(NoAATiles), 1);
	SC_AddBoolConfigParam(0,  TEXT("ZRangeHack"), CPP_PROPERTY_LOCAL(ZRangeHack), UTGLR_DEFAULT_ZRangeHack);

	SurfaceSelectionColor = FColor(0, 0, 31, 31);
	new(GetClass(), TEXT("SurfaceSelectionColor"), RF_Public)UStructProperty(CPP_PROPERTY(SurfaceSelectionColor), TEXT("Options"), CPF_Config, FindObjectChecked<UStruct>(NULL, TEXT("Core.Object.Color"), 1));

#undef CPP_PROPERTY_LOCAL
#undef CPP_PROPERTY_LOCAL_DCV

	//Driver flags
	SpanBased				= 0;
	SupportsFogMaps			= 1;
#ifdef UTGLR_RUNE_BUILD
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
	m_pAlphaTexObj = NULL;

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

	DescFlags |= RDDESCF_Certified;

	SupportsStaticBsp = true;
	UseAmbientlessLightmaps = false;

	unguard;
}


void UD3D9RenderDevice::SC_AddBoolConfigParam(DWORD BitMaskOffset, const TCHAR *pName, UBOOL &param, ECppProperty EC_CppProperty, INT InOffset, UBOOL defaultValue) {
	//param = (((defaultValue) != 0) ? 1 : 0) << BitMaskOffset; //Doesn't exactly work like a UBOOL "// Boolean 0 (false) or 1 (true)."
	// stijn: we no longer need the manual bitmask shifting in patch 469
	param = defaultValue;
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


#ifdef UTGLR_INCLUDE_SSE_CODE
bool UD3D9RenderDevice::CPU_DetectCPUID(void) {
	//Check for cpuid instruction support
	__try {
		__asm {
			//CPUID function 0
			xor eax, eax
			cpuid
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}

	return true;
}

bool UD3D9RenderDevice::CPU_DetectSSE(void) {
	bool bSupportsSSE;

	//Check for cpuid instruction support
	if (CPU_DetectCPUID() != true) {
		return false;
	}

	//Check for SSE support
	bSupportsSSE = false;
	__asm {
		//CPUID function 1
		mov eax, 1
		cpuid

		//Check the SSE bit
		test edx, 0x02000000
		jz l_no_sse

		//Set bSupportsSSE to true
		mov bSupportsSSE, 1

l_no_sse:
	}

	//Return if no CPU SSE support
	if (bSupportsSSE == false) {
		return bSupportsSSE;
	}

	//Check for SSE OS support
	__try {
		__asm {
			//Execute SSE instruction
			xorps xmm0, xmm0
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		//Clear SSE support flag
		bSupportsSSE = false;
	}

	return bSupportsSSE;
}

bool UD3D9RenderDevice::CPU_DetectSSE2(void) {
	bool bSupportsSSE2;

	//Check for cpuid instruction support
	if (CPU_DetectCPUID() != true) {
		return false;
	}

	//Check for SSE2 support
	bSupportsSSE2 = false;
	__asm {
		//CPUID function 1
		mov eax, 1
		cpuid

		//Check the SSE2 bit
		test edx, 0x04000000
		jz l_no_sse2

		//Set bSupportsSSE2 to true
		mov bSupportsSSE2, 1

l_no_sse2:
	}

	//Return if no CPU SSE2 support
	if (bSupportsSSE2 == false) {
		return bSupportsSSE2;
	}

	//Check for SSE2 OS support
	__try {
		__asm {
			//Execute SSE2 instruction
			xorpd xmm0, xmm0
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		//Clear SSE2 support flag
		bSupportsSSE2 = false;
	}

	return bSupportsSSE2;
}
#endif //UTGLR_INCLUDE_SSE_CODE


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

#ifdef UTGLR_INCLUDE_SSE_CODE
__declspec(naked) static void FASTCALL Buffer3ColoredVerts_SSE(UD3D9RenderDevice *pRD, FTransTexture** Pts) {
	static float f255 = 255.0f;
	__asm {
		//pRD is in ecx
		//Pts is in edx

		push ebx
		push esi
		push edi

		mov eax, [ecx]UD3D9RenderDevice.m_bufferedVerts

		lea ebx, [eax*8]
		add ebx, [ecx]UD3D9RenderDevice.m_pTexCoordArray[0]

		mov edi, eax
		shl edi, 4
		add edi, [ecx]UD3D9RenderDevice.m_pVertexColorArray

		//m_bufferedVerts += 3
		add eax, 3
		mov [ecx]UD3D9RenderDevice.m_bufferedVerts, eax

		lea eax, [ecx]UD3D9RenderDevice.TexInfo
		movss xmm0, [eax]FTexInfo.UMult
		movss xmm1, [eax]FTexInfo.VMult
		movss xmm2, f255

		//Pts in edx
		//Get PtsPlus12B
		lea esi, [edx + 12]

v_loop:
			mov eax, [edx]
			add edx, 4

			movss xmm3, [eax]FTransTexture.U
			mulss xmm3, xmm0
			movss [ebx]FGLTexCoord.u, xmm3
			movss xmm3, [eax]FTransTexture.V
			mulss xmm3, xmm1
			movss [ebx]FGLTexCoord.v, xmm3
			add ebx, TYPE FGLTexCoord

			mov ecx, [eax]FOutVector.Point.X
			mov [edi]FGLVertexColor.x, ecx
			mov ecx, [eax]FOutVector.Point.Y
			mov [edi]FGLVertexColor.y, ecx
			mov ecx, [eax]FOutVector.Point.Z
			mov [edi]FGLVertexColor.z, ecx

			movss xmm3, [eax]FTransSample.Light + 0
			mulss xmm3, xmm2
			movss xmm4, [eax]FTransSample.Light + 4
			mulss xmm4, xmm2
			movss xmm5, [eax]FTransSample.Light + 8
			mulss xmm5, xmm2
			cvtss2si eax, xmm3
			shl eax, 16
			cvtss2si ecx, xmm4
			and ecx, 255
			shl ecx, 8
			or eax, ecx
			cvtss2si ecx, xmm5
			and ecx, 255
			or ecx, 0xFF000000
			or eax, ecx
			mov [edi]FGLVertexColor.color, eax
			add edi, TYPE FGLVertexColor

			cmp edx, esi
			jne v_loop

		pop edi
		pop esi
		pop ebx

		ret
	}
}

__declspec(naked) static void FASTCALL Buffer3ColoredVerts_SSE2(UD3D9RenderDevice *pRD, FTransTexture** Pts) {
	static __m128 fColorMul = { 255.0f, 255.0f, 255.0f, 0.0f };
	static DWORD alphaOr = 0xFF000000;
	__asm {
		//pRD is in ecx
		//Pts is in edx

		push ebx
		push esi
		push edi

		mov eax, [ecx]UD3D9RenderDevice.m_bufferedVerts

		lea ebx, [eax*8]
		add ebx, [ecx]UD3D9RenderDevice.m_pTexCoordArray[0]

		mov edi, eax
		shl edi, 4
		add edi, [ecx]UD3D9RenderDevice.m_pVertexColorArray

		//m_bufferedVerts += 3
		add eax, 3
		mov [ecx]UD3D9RenderDevice.m_bufferedVerts, eax

		lea eax, [ecx]UD3D9RenderDevice.TexInfo
		movss xmm0, [eax]FTexInfo.UMult
		movss xmm1, [eax]FTexInfo.VMult
		movaps xmm2, fColorMul
		movd xmm3, alphaOr

		//Pts in edx
		//Get PtsPlus12B
		lea esi, [edx + 12]

v_loop:
			mov eax, [edx]
			add edx, 4

			movss xmm4, [eax]FTransTexture.U
			mulss xmm4, xmm0
			movss [ebx]FGLTexCoord.u, xmm4
			movss xmm4, [eax]FTransTexture.V
			mulss xmm4, xmm1
			movss [ebx]FGLTexCoord.v, xmm4
			add ebx, TYPE FGLTexCoord

			mov ecx, [eax]FOutVector.Point.X
			mov [edi]FGLVertexColor.x, ecx
			mov ecx, [eax]FOutVector.Point.Y
			mov [edi]FGLVertexColor.y, ecx
			mov ecx, [eax]FOutVector.Point.Z
			mov [edi]FGLVertexColor.z, ecx

			movups xmm4, [eax]FTransSample.Light
			shufps xmm4, xmm4, 0xC6
			mulps xmm4, xmm2
			cvtps2dq xmm4, xmm4
			packssdw xmm4, xmm4
			packuswb xmm4, xmm4
			por xmm4, xmm3
			movd [edi]FGLVertexColor.color, xmm4
			add edi, TYPE FGLVertexColor

			cmp edx, esi
			jne v_loop

		pop edi
		pop esi
		pop ebx

		ret
	}
}
#endif

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

#ifdef UTGLR_INCLUDE_SSE_CODE
__declspec(naked) static void FASTCALL Buffer3FoggedVerts_SSE(UD3D9RenderDevice *pRD, FTransTexture** Pts) {
	static float f255 = 255.0f;
	static float f1 = 1.0f;
	__asm {
		//pRD is in ecx
		//Pts is in edx

		push ebx
		push esi
		push edi
		push ebp
		sub esp, 4

		mov eax, [ecx]UD3D9RenderDevice.m_bufferedVerts

		lea ebx, [eax*8]
		add ebx, [ecx]UD3D9RenderDevice.m_pTexCoordArray[0]

		mov edi, eax
		shl edi, 4
		add edi, [ecx]UD3D9RenderDevice.m_pVertexColorArray

		lea esi, [eax*4]
		add esi, [ecx]UD3D9RenderDevice.m_pSecondaryColorArray

		//m_bufferedVerts += 3
		add eax, 3
		mov [ecx]UD3D9RenderDevice.m_bufferedVerts, eax

		lea eax, [ecx]UD3D9RenderDevice.TexInfo
		movss xmm0, [eax]FTexInfo.UMult
		movss xmm1, [eax]FTexInfo.VMult
		movss xmm2, f255

		//Pts in edx
		//Get PtsPlus12B
		lea ebp, [edx + 12]

v_loop:
			mov eax, [edx]
			add edx, 4

			movss xmm3, [eax]FTransTexture.U
			mulss xmm3, xmm0
			movss [ebx]FGLTexCoord.u, xmm3
			movss xmm3, [eax]FTransTexture.V
			mulss xmm3, xmm1
			movss [ebx]FGLTexCoord.v, xmm3
			add ebx, TYPE FGLTexCoord

			mov [esp], ebx

			movss xmm6, f1
			subss xmm6, [eax]FTransSample.Fog + 12
			mulss xmm6, xmm2

			mov ecx, [eax]FOutVector.Point.X
			mov [edi]FGLVertexColor.x, ecx
			mov ecx, [eax]FOutVector.Point.Y
			mov [edi]FGLVertexColor.y, ecx
			mov ecx, [eax]FOutVector.Point.Z
			mov [edi]FGLVertexColor.z, ecx

			movss xmm3, [eax]FTransSample.Light + 0
			mulss xmm3, xmm6
			movss xmm4, [eax]FTransSample.Light + 4
			mulss xmm4, xmm6
			movss xmm5, [eax]FTransSample.Light + 8
			mulss xmm5, xmm6
			cvtss2si ebx, xmm3
			shl ebx, 16
			cvtss2si ecx, xmm4
			and ecx, 255
			shl ecx, 8
			or ebx, ecx
			cvtss2si ecx, xmm5
			and ecx, 255
			or ecx, 0xFF000000
			or ebx, ecx
			mov [edi]FGLVertexColor.color, ebx
			add edi, TYPE FGLVertexColor

			mov ebx, [esp]

			movss xmm3, [eax]FTransSample.Fog + 0
			mulss xmm3, xmm2
			movss xmm4, [eax]FTransSample.Fog + 4
			mulss xmm4, xmm2
			movss xmm5, [eax]FTransSample.Fog + 8
			mulss xmm5, xmm2
			cvtss2si eax, xmm3
			and eax, 255
			shl eax, 16
			cvtss2si ecx, xmm4
			and ecx, 255
			shl ecx, 8
			or eax, ecx
			cvtss2si ecx, xmm5
			and ecx, 255
			or eax, ecx
			mov [esi]FGLSecondaryColor.specular, eax
			add esi, TYPE FGLSecondaryColor

			cmp edx, ebp
			jne v_loop

		add esp, 4
		pop ebp
		pop edi
		pop esi
		pop ebx

		ret
	}
}

__declspec(naked) static void FASTCALL Buffer3FoggedVerts_SSE2(UD3D9RenderDevice *pRD, FTransTexture** Pts) {
	static __m128 fColorMul = { 255.0f, 255.0f, 255.0f, 0.0f };
	static DWORD alphaOr = 0xFF000000;
	static float f1 = 1.0f;
	__asm {
		//pRD is in ecx
		//Pts is in edx

		push ebx
		push esi
		push edi
		push ebp

		mov eax, [ecx]UD3D9RenderDevice.m_bufferedVerts

		lea ebx, [eax*8]
		add ebx, [ecx]UD3D9RenderDevice.m_pTexCoordArray[0]

		mov edi, eax
		shl edi, 4
		add edi, [ecx]UD3D9RenderDevice.m_pVertexColorArray

		lea esi, [eax*4]
		add esi, [ecx]UD3D9RenderDevice.m_pSecondaryColorArray

		//m_bufferedVerts += 3
		add eax, 3
		mov [ecx]UD3D9RenderDevice.m_bufferedVerts, eax

		lea eax, [ecx]UD3D9RenderDevice.TexInfo
		movss xmm0, [eax]FTexInfo.UMult
		movss xmm1, [eax]FTexInfo.VMult
		movaps xmm2, fColorMul
		movd xmm3, alphaOr

		//Pts in edx
		//Get PtsPlus12B
		lea ebp, [edx + 12]

v_loop:
			mov eax, [edx]
			add edx, 4

			movss xmm4, [eax]FTransTexture.U
			mulss xmm4, xmm0
			movss [ebx]FGLTexCoord.u, xmm4
			movss xmm4, [eax]FTransTexture.V
			mulss xmm4, xmm1
			movss [ebx]FGLTexCoord.v, xmm4
			add ebx, TYPE FGLTexCoord

			movss xmm6, f1
			subss xmm6, [eax]FTransSample.Fog + 12
			mulss xmm6, xmm2
			shufps xmm6, xmm6, 0x00

			mov ecx, [eax]FOutVector.Point.X
			mov [edi]FGLVertexColor.x, ecx
			mov ecx, [eax]FOutVector.Point.Y
			mov [edi]FGLVertexColor.y, ecx
			mov ecx, [eax]FOutVector.Point.Z
			mov [edi]FGLVertexColor.z, ecx

			movups xmm4, [eax]FTransSample.Light
			shufps xmm4, xmm4, 0xC6
			mulps xmm4, xmm6
			cvtps2dq xmm4, xmm4
			packssdw xmm4, xmm4
			packuswb xmm4, xmm4
			por xmm4, xmm3
			movd [edi]FGLVertexColor.color, xmm4
			add edi, TYPE FGLVertexColor

			movups xmm4, [eax]FTransSample.Fog
			shufps xmm4, xmm4, 0xC6
			mulps xmm4, xmm2
			cvtps2dq xmm4, xmm4
			packssdw xmm4, xmm4
			packuswb xmm4, xmm4
			movd [esi]FGLSecondaryColor.specular, xmm4
			add esi, TYPE FGLSecondaryColor

			cmp edx, ebp
			jne v_loop

		pop ebp
		pop edi
		pop esi
		pop ebx

		ret
	}
}
#endif

//#define UTGLR_DEBUG_SHOW_CALL_COUNTS
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
#ifdef UTGLR_RUNE_BUILD
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
#ifdef UTGLR_RUNE_BUILD
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
#ifdef UTGLR_RUNE_BUILD
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


void UD3D9RenderDevice::BuildGammaRamp(float redGamma, float greenGamma, float blueGamma, int brightness, D3DGAMMARAMP &ramp) {
	unsigned int u;

	//Parameter clamping
	if (brightness < -50) brightness = -50;
	if (brightness > 50) brightness = 50;

	float rcpRedGamma = 1.0f / (2.5f * redGamma);
	float rcpGreenGamma = 1.0f / (2.5f * greenGamma);
	float rcpBlueGamma = 1.0f / (2.5f * blueGamma);
	for (u = 0; u < 256; u++) {
		int iVal;

		//Initial value
		iVal = u;

		//Brightness
		iVal += brightness;
		//Clamping
		if (iVal < 0) iVal = 0;
		if (iVal > 255) iVal = 255;

		//Gamma + Save results
		ramp.red[u] = (_WORD)(int)appRound((float)appPow(iVal / 255.0f, rcpRedGamma) * 65535.0f);
		ramp.green[u] = (_WORD)(int)appRound((float)appPow(iVal / 255.0f, rcpGreenGamma) * 65535.0f);
		ramp.blue[u] = (_WORD)(int)appRound((float)appPow(iVal / 255.0f, rcpBlueGamma) * 65535.0f);
	}

	return;
}

void UD3D9RenderDevice::BuildGammaRamp(float redGamma, float greenGamma, float blueGamma, int brightness, FByteGammaRamp &ramp) {
	unsigned int u;

	//Parameter clamping
	if (brightness < -50) brightness = -50;
	if (brightness > 50) brightness = 50;

	float rcpRedGamma = 1.0f / (2.5f * redGamma);
	float rcpGreenGamma = 1.0f / (2.5f * greenGamma);
	float rcpBlueGamma = 1.0f / (2.5f * blueGamma);
	for (u = 0; u < 256; u++) {
		int iVal;

		//Initial value
		iVal = u;

		//Brightness
		iVal += brightness;
		//Clamping
		if (iVal < 0) iVal = 0;
		if (iVal > 255) iVal = 255;

		//Gamma + Save results
		ramp.red[u] = (BYTE)(int)appRound((float)appPow(iVal / 255.0f, rcpRedGamma) * 255.0f);
		ramp.green[u] = (BYTE)(int)appRound((float)appPow(iVal / 255.0f, rcpGreenGamma) * 255.0f);
		ramp.blue[u] = (BYTE)(int)appRound((float)appPow(iVal / 255.0f, rcpBlueGamma) * 255.0f);
	}

	return;
}

void UD3D9RenderDevice::SetGamma(FLOAT GammaCorrection) {
	D3DGAMMARAMP gammaRamp;

	GammaCorrection += GammaOffset;

	//Do not attempt to set gamma if <= zero
	if (GammaCorrection <= 0.0f) {
		return;
	}

	BuildGammaRamp(GammaCorrection + GammaOffsetRed, GammaCorrection + GammaOffsetGreen, GammaCorrection + GammaOffsetBlue, Brightness, gammaRamp);

	m_setGammaRampSucceeded = false;
	m_d3dDevice->SetGammaRamp(0, D3DSGR_NO_CALIBRATION, &gammaRamp);
	if (1) {
		m_setGammaRampSucceeded = true;
		SavedGammaCorrection = GammaCorrection;
	}

	return;
}

void UD3D9RenderDevice::ResetGamma(void) {
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

	//Reset gamma ramp
	ResetGamma();

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

	//Reset gamma ramp
	ResetGamma();

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

	//Get adapter identifier for other vendor specific checks
	m_isATI = false;
	m_isNVIDIA = false;
	{
		D3DADAPTER_IDENTIFIER9 ident;

		hResult = m_d3d9->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &ident);
		if (FAILED(hResult)) {
			appErrorf(TEXT("GetAdapterIdentifier failed (0x%08X)"), hResult);
		}

		debugf(TEXT("D3D adapter driver      : %s"), appFromAnsi(ident.Driver));
		debugf(TEXT("D3D adapter description : %s"), appFromAnsi(ident.Description));
		debugf(TEXT("D3D adapter id          : 0x%04X:0x%04X"), ident.VendorId, ident.DeviceId);

		//Identifying vendor based on vendor id number
		if (ident.VendorId == 0x1002) {
			m_isATI = true;
		}
		else if (ident.VendorId == 0x10DE) {
			m_isNVIDIA = true;
		}
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

	m_usingAA = false;
	m_curAAEnable = true;
	m_defAAEnable = true;
	m_initNumAASamples = NumAASamples;

	//Select AA mode
	if (UseAA) {
		D3DMULTISAMPLE_TYPE MultiSampleType;

		switch (NumAASamples) {
		case  0: MultiSampleType = D3DMULTISAMPLE_NONE; break;
		case  1: MultiSampleType = D3DMULTISAMPLE_NONE; break;
		case  2: MultiSampleType = D3DMULTISAMPLE_2_SAMPLES; break;
		case  3: MultiSampleType = D3DMULTISAMPLE_3_SAMPLES; break;
		case  4: MultiSampleType = D3DMULTISAMPLE_4_SAMPLES; break;
		case  5: MultiSampleType = D3DMULTISAMPLE_5_SAMPLES; break;
		case  6: MultiSampleType = D3DMULTISAMPLE_6_SAMPLES; break;
		case  7: MultiSampleType = D3DMULTISAMPLE_7_SAMPLES; break;
		case  8: MultiSampleType = D3DMULTISAMPLE_8_SAMPLES; break;
		case  9: MultiSampleType = D3DMULTISAMPLE_9_SAMPLES; break;
		case 10: MultiSampleType = D3DMULTISAMPLE_10_SAMPLES; break;
		case 11: MultiSampleType = D3DMULTISAMPLE_11_SAMPLES; break;
		case 12: MultiSampleType = D3DMULTISAMPLE_12_SAMPLES; break;
		case 13: MultiSampleType = D3DMULTISAMPLE_13_SAMPLES; break;
		case 14: MultiSampleType = D3DMULTISAMPLE_14_SAMPLES; break;
		case 15: MultiSampleType = D3DMULTISAMPLE_15_SAMPLES; break;
		case 16: MultiSampleType = D3DMULTISAMPLE_16_SAMPLES; break;
		default:
			MultiSampleType = D3DMULTISAMPLE_NONE;
		}
		m_d3dpp.MultiSampleType = MultiSampleType;

		hResult = m_d3d9->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_d3dpp.BackBufferFormat, m_d3dpp.Windowed, m_d3dpp.MultiSampleType, NULL);
		if (FAILED(hResult)) {
			m_d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
		}
		hResult = m_d3d9->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_d3dpp.AutoDepthStencilFormat, m_d3dpp.Windowed, m_d3dpp.MultiSampleType, NULL);
		if (FAILED(hResult)) {
			m_d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
		}

		if (m_d3dpp.MultiSampleType != D3DMULTISAMPLE_NONE) {
			m_usingAA = true;
		}
	}

	//Set swap interval
	if (SwapInterval >= 0) {
		switch (SwapInterval) {
		case 0:
			if (m_d3dCaps.PresentationIntervals & D3DPRESENT_INTERVAL_IMMEDIATE) {
				m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
			}
			break;
		case 1:
			if (m_d3dCaps.PresentationIntervals & D3DPRESENT_INTERVAL_ONE) {
				m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
			}
			break;
		default:
			;
		}
	}


	//Set increased back buffer count if using triple buffering
	if (UseTripleBuffering) {
		m_d3dpp.BackBufferCount = 2;
	}

	//Set initial HW/SW vertex processing preference
	m_doSoftwareVertexInit = false;
	if (UseSoftwareVertexProcessing) {
		m_doSoftwareVertexInit = true;
	}

	//Try HW vertex init if not forcing SW vertex init
	if (!m_doSoftwareVertexInit) {
		bool tryDefaultRefreshRate = true;
		DWORD behaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;

		//Check if should use pure device
		if (UsePureDevice && (m_d3dCaps.DevCaps & D3DDEVCAPS_PUREDEVICE)) {
			behaviorFlags |= D3DCREATE_PUREDEVICE;
		}

		//Possibly attempt to set refresh rate if fullscreen
		if (!m_d3dpp.Windowed && (RefreshRate > 0)) {
			//Attempt to create with specific refresh rate
			m_d3dpp.FullScreen_RefreshRateInHz = RefreshRate;
			hResult = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, behaviorFlags, &m_d3dpp, &m_d3dDevice);
			//Try again if triple buffering failed
			if (FAILED(hResult) && UseTripleBuffering && (m_d3dpp.BackBufferCount != 2)) {
				hResult = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, behaviorFlags, &m_d3dpp, &m_d3dDevice);
			}
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
			//Try again if triple buffering failed
			if (FAILED(hResult) && UseTripleBuffering && (m_d3dpp.BackBufferCount != 2)) {
				hResult = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, behaviorFlags, &m_d3dpp, &m_d3dDevice);
			}
			if (FAILED(hResult)) {
				debugf(NAME_Init, TEXT("Failed to create D3D device with hardware vertex processing (0x%08X)"), hResult);
				m_doSoftwareVertexInit = true;
			}
		}
	}
	//Try SW vertex init if forced earlier or if HW vertex init failed
	if (m_doSoftwareVertexInit) {
		bool tryDefaultRefreshRate = true;
		DWORD behaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

		//Possibly attempt to set refresh rate if fullscreen
		if (!m_d3dpp.Windowed && (RefreshRate > 0)) {
			//Attempt to create with specific refresh rate
			m_d3dpp.FullScreen_RefreshRateInHz = RefreshRate;
			hResult = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, behaviorFlags, &m_d3dpp, &m_d3dDevice);
			//Try again if triple buffering failed
			if (FAILED(hResult) && UseTripleBuffering && (m_d3dpp.BackBufferCount != 2)) {
				hResult = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, behaviorFlags, &m_d3dpp, &m_d3dDevice);
			}
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
			//Try again if triple buffering failed
			if (FAILED(hResult) && UseTripleBuffering && (m_d3dpp.BackBufferCount != 2)) {
				hResult = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd, behaviorFlags, &m_d3dpp, &m_d3dDevice);
			}
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

	//Get other defaults
	if (!GConfig->GetInt(g_pSection, TEXT("Brightness"), Brightness)) Brightness = 0;

	//Debug parameter listing
	if (DebugBit(DEBUG_BIT_BASIC)) {
		#define UTGLR_DEBUG_SHOW_PARAM_REG(_name) DbgPrintInitParam(TEXT(#_name), _name)
		#define UTGLR_DEBUG_SHOW_PARAM_DCV(_name) DbgPrintInitParam(TEXT(#_name), DCV._name)

		UTGLR_DEBUG_SHOW_PARAM_REG(LODBias);
		UTGLR_DEBUG_SHOW_PARAM_REG(GammaOffset);
		UTGLR_DEBUG_SHOW_PARAM_REG(GammaOffsetRed);
		UTGLR_DEBUG_SHOW_PARAM_REG(GammaOffsetGreen);
		UTGLR_DEBUG_SHOW_PARAM_REG(GammaOffsetBlue);
		UTGLR_DEBUG_SHOW_PARAM_REG(Brightness);
		UTGLR_DEBUG_SHOW_PARAM_REG(GammaCorrectScreenshots);
		UTGLR_DEBUG_SHOW_PARAM_REG(OneXBlending);
		UTGLR_DEBUG_SHOW_PARAM_REG(MinLogTextureSize);
		UTGLR_DEBUG_SHOW_PARAM_REG(MaxLogTextureSize);
		UTGLR_DEBUG_SHOW_PARAM_REG(MaxAnisotropy);
		UTGLR_DEBUG_SHOW_PARAM_REG(TMUnits);
		UTGLR_DEBUG_SHOW_PARAM_REG(MaxTMUnits);
		UTGLR_DEBUG_SHOW_PARAM_REG(RefreshRate);
		UTGLR_DEBUG_SHOW_PARAM_REG(UsePrecache);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseTrilinear);
//		UTGLR_DEBUG_SHOW_PARAM_REG(UseVertexSpecular);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseS3TC);
		UTGLR_DEBUG_SHOW_PARAM_REG(Use16BitTextures);
		UTGLR_DEBUG_SHOW_PARAM_REG(Use565Textures);
		UTGLR_DEBUG_SHOW_PARAM_REG(NoFiltering);
		UTGLR_DEBUG_SHOW_PARAM_REG(DetailMax);
//		UTGLR_DEBUG_SHOW_PARAM_REG(UseDetailAlpha);
		UTGLR_DEBUG_SHOW_PARAM_REG(DetailClipping);
		UTGLR_DEBUG_SHOW_PARAM_REG(ColorizeDetailTextures);
		UTGLR_DEBUG_SHOW_PARAM_REG(SinglePassFog);
		UTGLR_DEBUG_SHOW_PARAM_DCV(SinglePassDetail);
//		UTGLR_DEBUG_SHOW_PARAM_REG(BufferActorTris);
//		UTGLR_DEBUG_SHOW_PARAM_REG(BufferClippedActorTris);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseSSE);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseSSE2);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseTexIdPool);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseTexPool);
		UTGLR_DEBUG_SHOW_PARAM_REG(DynamicTexIdRecycleLevel);
		UTGLR_DEBUG_SHOW_PARAM_REG(TexDXT1ToDXT3);
		UTGLR_DEBUG_SHOW_PARAM_REG(SwapInterval);
		UTGLR_DEBUG_SHOW_PARAM_REG(FrameRateLimit);
#if UTGLR_USES_SCENENODEHACK
		UTGLR_DEBUG_SHOW_PARAM_REG(SceneNodeHack);
#endif
		UTGLR_DEBUG_SHOW_PARAM_REG(SmoothMaskedTextures);
		UTGLR_DEBUG_SHOW_PARAM_REG(MaskedTextureHack);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseTripleBuffering);
		UTGLR_DEBUG_SHOW_PARAM_REG(UsePureDevice);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseSoftwareVertexProcessing);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseAA);
		UTGLR_DEBUG_SHOW_PARAM_REG(NumAASamples);
		UTGLR_DEBUG_SHOW_PARAM_REG(NoAATiles);
		UTGLR_DEBUG_SHOW_PARAM_REG(ZRangeHack);

		#undef UTGLR_DEBUG_SHOW_PARAM_REG
		#undef UTGLR_DEBUG_SHOW_PARAM_DCV
	}


#ifdef UTGLR_INCLUDE_SSE_CODE
	if (UseSSE) {
		if (!CPU_DetectSSE()) {
			UseSSE = 0;
		}
	}
	if (UseSSE2) {
		if (!CPU_DetectSSE2()) {
			UseSSE2 = 0;
		}
	}
#else
	UseSSE = 0;
	UseSSE2 = 0;
#endif
	if (DebugBit(DEBUG_BIT_BASIC)) dout << TEXT("utd3d9r: UseSSE = ") << UseSSE << std::endl;
	if (DebugBit(DEBUG_BIT_BASIC)) dout << TEXT("utd3d9r: UseSSE2 = ") << UseSSE2 << std::endl;

	SetGamma(Viewport->GetOuterUClient()->Brightness);

	//Restrict dynamic tex id recycle level range
	if (DynamicTexIdRecycleLevel < 10) DynamicTexIdRecycleLevel = 10;

	//Always use vertex specular unless caps check fails later
	UseVertexSpecular = 1;

	SupportsTC = UseS3TC;

	BufferActorTris = 1;
	BufferClippedActorTris = 1;

	UseDetailAlpha = 1;


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

	//Set 16-bit texture capability flags
	m_16BitTextureCap = true;
	m_565TextureCap = true;
	//Check RGBA5551
	hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_A1R5G5B5);
	if (FAILED(hResult)) {
		m_16BitTextureCap = false;
	}
	//Check RGB555
	hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_X1R5G5B5);
	if (FAILED(hResult)) {
		m_16BitTextureCap = false;
	}
	//Check RGB565
	hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_R5G6B5);
	if (FAILED(hResult)) {
		m_565TextureCap = false;
	}

	//Set alpha texture capability flag
	m_alphaTextureCap = true;
	hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format, 0, D3DRTYPE_TEXTURE, D3DFMT_A8);
	if (FAILED(hResult)) {
		m_alphaTextureCap = false;
	}

	//Set alpha to coverage capability flag
	m_supportsAlphaToCoverage = false;
	//ATI says all cards supporting the DX9 feature set support alpha to coverage
	//NVIDIA has a way to detect alpha to coverage support
	//In this case will start filtering based on if supports ps3.0
	if (m_d3dCaps.PixelShaderVersion >= D3DPS_VERSION(3,0)) {
		if (m_isATI) {
			//Supported on ATI DX9 class HW
			m_supportsAlphaToCoverage = true;
		}
		else if (m_isNVIDIA) {
			//Check if alpha to coverage supported for NVIDIA
			hResult = m_d3d9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
				D3DFMT_X8R8G8B8, 0, D3DRTYPE_SURFACE, (D3DFORMAT)MAKEFOURCC('A', 'T', 'O', 'C'));
			if (FAILED(hResult)) {
			}
			else {
				m_supportsAlphaToCoverage = true;
			}
		}
	}



	// Validate flags.

	//Special extensions validation for init only config pass
	if (!m_dxt1TextureCap) SupportsTC = 0;
#ifdef UTGLR_UNREAL_227_BUILD
	if (!m_dxt3TextureCap) SupportsTC = 0;
	if (!m_dxt5TextureCap) SupportsTC = 0;
#endif

	//DCV refresh
	ConfigValidate_RefreshDCV();

	//Required extensions config validation pass
	ConfigValidate_RequiredExtensions();


	if (!MaxTMUnits || (MaxTMUnits > MAX_TMUNITS)) {
		MaxTMUnits = MAX_TMUNITS;
	}

	TMUnits = m_d3dCaps.MaxSimultaneousTextures;
	debugf(TEXT("%i Texture Mapping Units found"), TMUnits);
	if (TMUnits > MaxTMUnits) {
		TMUnits = MaxTMUnits;
	}


	//Main config validation pass (after set TMUnits)
	ConfigValidate_Main();


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

	debugf(TEXT("UseDetailAlpha = %i"), UseDetailAlpha);


	//Set pointers to aligned memory
	MapDotArray = (FGLMapDot *)AlignMemPtr(m_MapDotArrayMem, VERTEX_ARRAY_ALIGN);


	// Verify hardware defaults.
	check(MinLogTextureSize >= 0);
	check(MaxLogTextureSize >= 0);
	check(MinLogTextureSize <= MaxLogTextureSize);

	// Flush textures.
	Flush(1);

	//Invalidate fixed texture ids
	m_pNoTexObj = NULL;
	m_pAlphaTexObj = NULL;

	//Initialize permanent rendering state, including allocation of some resources
	InitPermanentResourcesAndRenderingState();


	//Initialize previous lock variables
	PL_DetailTextures = DetailTextures;
	PL_OneXBlending = OneXBlending;
	PL_MaxLogUOverV = MaxLogUOverV;
	PL_MaxLogVOverU = MaxLogVOverU;
	PL_MinLogTextureSize = MinLogTextureSize;
	PL_MaxLogTextureSize = MaxLogTextureSize;
	PL_NoFiltering = NoFiltering;
	PL_UseTrilinear = UseTrilinear;
	PL_Use16BitTextures = Use16BitTextures;
	PL_Use565Textures = Use565Textures;
	PL_TexDXT1ToDXT3 = TexDXT1ToDXT3;
	PL_MaxAnisotropy = MaxAnisotropy;
	PL_SmoothMaskedTextures = SmoothMaskedTextures;
	PL_MaskedTextureHack = MaskedTextureHack;
	PL_LODBias = LODBias;
	PL_UseDetailAlpha = UseDetailAlpha;
	PL_SinglePassDetail = SinglePassDetail;
	PL_UseSSE = UseSSE;
	PL_UseSSE2 = UseSSE2;


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
	Flush(1);

	//Free fixed textures if they were allocated
	if (m_pNoTexObj) {
		m_pNoTexObj->Release();
		m_pNoTexObj = NULL;
	}
	if (m_pAlphaTexObj) {
		m_pAlphaTexObj->Release();
		m_pAlphaTexObj = NULL;
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


void UD3D9RenderDevice::ConfigValidate_RefreshDCV(void) {
	#define UTGLR_REFRESH_DCV(_name) _name = DCV._name

	UTGLR_REFRESH_DCV(SinglePassDetail);

	#undef UTGLR_REFRESH_DCV

	return;
}

void UD3D9RenderDevice::ConfigValidate_RequiredExtensions(void) {
	if (!(m_d3dCaps.TextureOpCaps & D3DTEXOPCAPS_BLENDCURRENTALPHA)) DetailTextures = 0;
	if (!(m_d3dCaps.TextureOpCaps & D3DTEXOPCAPS_BLENDCURRENTALPHA)) UseDetailAlpha = 0;
	if (!m_alphaTextureCap) UseDetailAlpha = 0;
	if (!(m_d3dCaps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC)) MaxAnisotropy = 0;
	if (!(m_d3dCaps.RasterCaps & D3DPRASTERCAPS_MIPMAPLODBIAS)) LODBias = 0;
	if (!m_dxt3TextureCap) TexDXT1ToDXT3 = 0;
	if (!m_16BitTextureCap) Use16BitTextures = 0;
	if (!m_565TextureCap) Use565Textures = 0;

	if (!(m_d3dCaps.TextureOpCaps & D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR)) SinglePassFog = 0;

	//Force 1X blending if no 2X modulation support
	if (!(m_d3dCaps.TextureOpCaps & D3DTEXOPCAPS_MODULATE2X)) OneXBlending = 0x1;	//Must use proper bit offset for Bool param

	return;
}

void UD3D9RenderDevice::ConfigValidate_Main(void) {
	//Detail alpha requires at least two texture units
	if (TMUnits < 2) UseDetailAlpha = 0;

	//Single pass detail texturing requires at least 4 texture units
	if (TMUnits < 4) SinglePassDetail = 0;
	//Single pass detail texturing requires detail alpha
	if (!UseDetailAlpha) SinglePassDetail = 0;

	//Limit maximum DetailMax
	if (DetailMax > 3) DetailMax = 3;

	return;
}


void UD3D9RenderDevice::InitPermanentResourcesAndRenderingState(void) {
	guard(InitPermanentResourcesAndRenderingState);

	unsigned int u;
	HRESULT hResult;

	//Set view matrix
	D3DMATRIX d3dView = { +1.0f,  0.0f,  0.0f,  0.0f,
						   0.0f, -1.0f,  0.0f,  0.0f,
						   0.0f,  0.0f, -1.0f,  0.0f,
						   0.0f,  0.0f,  0.0f, +1.0f };
	m_d3dDevice->SetTransform(D3DTS_VIEW, &d3dView);

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

#ifdef UTGLR_RUNE_BUILD
	m_d3dDevice->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
	FLOAT fFogStart = 0.0f;
	m_d3dDevice->SetRenderState(D3DRS_FOGSTART, *(DWORD *)&fFogStart);
	m_gpFogEnabled = false;
#endif

	m_d3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	m_d3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

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

	if (UseDetailAlpha) {	// vogel: alpha texture for better detail textures (no vertex alpha)
		InitAlphaTextureSafe();
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

	//Vertex and primary color
	hResult = m_d3dDevice->CreateVertexBuffer(sizeof(FGLVertexColor) * VERTEX_ARRAY_SIZE, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, vertexBufferPool, &m_d3dVertexColorBuffer, NULL);
	if (FAILED(hResult)) {
		appErrorf(TEXT("CreateVertexBuffer failed"));
	}

	//Secondary color
	hResult = m_d3dDevice->CreateVertexBuffer(sizeof(FGLSecondaryColor) * VERTEX_ARRAY_SIZE, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, vertexBufferPool, &m_d3dSecondaryColorBuffer, NULL);
	if (FAILED(hResult)) {
		appErrorf(TEXT("CreateVertexBuffer failed"));
	}

	//TexCoord
	for (u = 0; u < TMUnits; u++) {
		hResult = m_d3dDevice->CreateVertexBuffer(sizeof(FGLTexCoord) * VERTEX_ARRAY_SIZE, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, vertexBufferPool, &m_d3dTexCoordBuffer[u], NULL);
		if (FAILED(hResult)) {
			appErrorf(TEXT("CreateVertexBuffer failed"));
		}
	}


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


	//Initialize vertex buffer state tracking information
	m_curVertexBufferPos = 0;
	m_vertexColorBufferNeedsDiscard = false;
	m_secondaryColorBufferNeedsDiscard = false;
	for (u = 0; u < MAX_TMUNITS; u++) {
		m_texCoordBufferNeedsDiscard[u] = false;
	}


	//Set stream sources
	//Vertex and primary color
	hResult = m_d3dDevice->SetStreamSource(0, m_d3dVertexColorBuffer, 0, sizeof(FGLVertexColor));
	if (FAILED(hResult)) {
		appErrorf(TEXT("SetStreamSource failed"));
	}

	//Secondary Color
	hResult = m_d3dDevice->SetStreamSource(1, m_d3dSecondaryColorBuffer, 0, sizeof(FGLSecondaryColor));
	if (FAILED(hResult)) {
		appErrorf(TEXT("SetStreamSource failed"));
	}

	//TexCoord
	for (u = 0; u < TMUnits; u++) {
		hResult = m_d3dDevice->SetStreamSource(2 + u, m_d3dTexCoordBuffer[u], 0, sizeof(FGLTexCoord));
		if (FAILED(hResult)) {
			appErrorf(TEXT("SetStreamSource failed"));
		}
	}


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
	m_useAlphaToCoverageForMasked = false;
	m_alphaToCoverageEnabled = false;
	m_alphaTestEnabled = false;
	m_curPolyFlags = 0;
	m_curPolyFlags2 = 0;

	//Initialize color flags
	m_requestedColorFlags = 0;

	//Initialize Z range hack state
	m_useZRangeHack = false;
	m_nearZRangeHackProjectionActive = false;
	m_requestNearZRangeHackProjection = false;


	unguard;
}

void UD3D9RenderDevice::FreePermanentResources(void) {
	guard(FreePermanentResources);

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
	if (m_d3dSecondaryColorBuffer) {
		m_d3dSecondaryColorBuffer->Release();
		m_d3dSecondaryColorBuffer = NULL;
	}
	for (u = 0; u < TMUnits; u++) {
		if (m_d3dTexCoordBuffer[u]) {
			m_d3dTexCoordBuffer[u]->Release();
			m_d3dTexCoordBuffer[u] = NULL;
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

	if (NumDevices == 0) {
		g_gammaFirstTime = true;
		g_haveOriginalGammaRamp = false;
	}

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

	if (URenderDevice::Exec(Cmd, Ar)) {
		return 1;
	}
	if (ParseCommand(&Cmd, TEXT("DGL"))) {
		if (ParseCommand(&Cmd, TEXT("BUFFERTRIS"))) {
			BufferActorTris = !BufferActorTris;
			if (!UseVertexSpecular) BufferActorTris = 0;
			debugf(TEXT("BUFFERTRIS [%i]"), BufferActorTris);
			return 1;
		}
		else if (ParseCommand(&Cmd, TEXT("BUILD"))) {
			debugf(TEXT("D3D9 renderer built: ?????"));
			return 1;
		}
		else if (ParseCommand(&Cmd, TEXT("AA"))) {
			if (m_usingAA) {
				m_defAAEnable = !m_defAAEnable;
				debugf(TEXT("AA Enable [%u]"), (m_defAAEnable) ? 1 : 0);
			}
			return 1;
		}

		return 0;
	}
	else if (ParseCommand(&Cmd, TEXT("GetRes"))) {
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
# if UTGLR_USES_SCENENODEHACK
	m_sceneNodeHackCount = 0;
# endif
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


	//DCV refresh
	ConfigValidate_RefreshDCV();

	//Required extensions config validation pass
	ConfigValidate_RequiredExtensions();


	//Main config validation pass
	ConfigValidate_Main();


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
	if (Use16BitTextures != PL_Use16BitTextures) {
		PL_Use16BitTextures = Use16BitTextures;
		flushTextures = true;
	}
	if (Use565Textures != PL_Use565Textures) {
		PL_Use565Textures = Use565Textures;
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

		//Disable alpha to coverage if currently enabled
		if (m_alphaToCoverageEnabled) {
			m_alphaToCoverageEnabled = false;
			DisableAlphaToCoverageNoCheck();
		}
	}

	if (MaskedTextureHack != PL_MaskedTextureHack) {
		PL_MaskedTextureHack = MaskedTextureHack;
		flushTextures = true;
	}

	if (LODBias != PL_LODBias) {
		PL_LODBias = LODBias;
		SetTexLODBiasState(TMUnits);
	}

	if (DetailTextures != PL_DetailTextures) {
		PL_DetailTextures = DetailTextures;
		flushTextures = true;
	}

	if (UseDetailAlpha != PL_UseDetailAlpha) {
		PL_UseDetailAlpha = UseDetailAlpha;
		if (UseDetailAlpha) {
			InitAlphaTextureSafe();
		}
	}

	if (SinglePassDetail != PL_SinglePassDetail) {
		PL_SinglePassDetail = SinglePassDetail;
	}


	//Smooth masked textures bit controls alpha blend for masked textures
	m_smoothMaskedTexturesBit = 0;
	m_useAlphaToCoverageForMasked = false;
	if (SmoothMaskedTextures) {
		//Use alpha to coverage if using AA and enough samples, and if supported at all
		//Also requiring fragment program mode for alpha to coverage with D3D9
		m_smoothMaskedTexturesBit = PF_Masked;
	}


	if (UseSSE != PL_UseSSE) {
#ifdef UTGLR_INCLUDE_SSE_CODE
		if (UseSSE) {
			if (!CPU_DetectSSE()) {
				UseSSE = 0;
			}
		}
#else
		UseSSE = 0;
#endif
		PL_UseSSE = UseSSE;
	}
	if (UseSSE2 != PL_UseSSE2) {
#ifdef UTGLR_INCLUDE_SSE_CODE
		if (UseSSE2) {
			if (!CPU_DetectSSE2()) {
				UseSSE2 = 0;
			}
		}
#else
		UseSSE2 = 0;
#endif
		PL_UseSSE2 = UseSSE2;
	}


	//Initialize buffer verts proc pointers
	m_pBuffer3BasicVertsProc = Buffer3BasicVerts;
	m_pBuffer3ColoredVertsProc = Buffer3ColoredVerts;
	m_pBuffer3FoggedVertsProc = Buffer3FoggedVerts;

#ifdef UTGLR_INCLUDE_SSE_CODE
	//Initialize SSE buffer verts proc pointers
	if (UseSSE) {
		m_pBuffer3ColoredVertsProc = Buffer3ColoredVerts_SSE;
		m_pBuffer3FoggedVertsProc = Buffer3FoggedVerts_SSE;
	}
	if (UseSSE2) {
		m_pBuffer3ColoredVertsProc = Buffer3ColoredVerts_SSE2;
		m_pBuffer3FoggedVertsProc = Buffer3FoggedVerts_SSE2;
	}
#endif //UTGLR_INCLUDE_SSE_CODE

	m_pBuffer3VertsProc = NULL;

	//Initialize buffer detail texture data proc pointer
	m_pBufferDetailTextureDataProc = &UD3D9RenderDevice::BufferDetailTextureData;
#ifdef UTGLR_INCLUDE_SSE_CODE
	if (UseSSE2) {
		m_pBufferDetailTextureDataProc = &UD3D9RenderDevice::BufferDetailTextureData_SSE2;
	}
#endif //UTGLR_INCLUDE_SSE_CODE


	//Precalculate the cutoff for buffering actor triangles based on config settings
	if (!BufferActorTris) {
		m_bufferActorTrisCutoff = 0;
	}
	else if (!BufferClippedActorTris) {
		m_bufferActorTrisCutoff = 3;
	}
	else {
		m_bufferActorTrisCutoff = 10;
	}

	//Precalculate detail texture color
	if (ColorizeDetailTextures) {
		m_detailTextureColor4ub = 0x00408040;
	}
	else {
		m_detailTextureColor4ub = 0x00808080;
	}

	//Precalculate mask for MaskedTextureHack based on if it's enabled
	m_maskedTextureHackMask = (MaskedTextureHack) ? TEX_CACHE_ID_FLAG_MASKED : 0;

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
		Flush(1);
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

	this->currentFrame = Frame;

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

	//Decide whether or not to use Z range hack
	m_useZRangeHack = false;
	if (ZRangeHack && !GIsEditor) {
		m_useZRangeHack = true;
	}

	// Set projection.
	if (Frame->Viewport->IsOrtho()) {
		//Don't use Z range hack if ortho projection
		m_useZRangeHack = false;

		SetOrthoProjection();
	}
	else {
		SetProjectionStateNoCheck(false);
	}

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

	SetDefaultAAState();
	SetDefaultProjectionState();
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
#if defined UTGLR_DX_BUILD || defined UTGLR_RUNE_BUILD
		FLOAT curFrameTimestamp;
#else
		FTime curFrameTimestamp;
#endif
		float timeDiff;
		float rcpFrameRateLimit;

		//First time timer init if necessary
		InitFrameRateLimitTimerSafe();

		curFrameTimestamp = appSeconds();
		timeDiff = curFrameTimestamp - m_prevFrameTimestamp;
		m_prevFrameTimestamp = curFrameTimestamp;

		rcpFrameRateLimit = 1.0f / FrameRateLimit;
		if (timeDiff < rcpFrameRateLimit) {
			float sleepTime;

			sleepTime = rcpFrameRateLimit - timeDiff;
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
# if UTGLR_USES_SCENENODEHACK
	dout << TEXT("Scene node hack count = ") << m_sceneNodeHackCount << std::endl;
# endif
	dout << TEXT("VB flush count = ") << m_vbFlushCount << std::endl;
	dout << TEXT("Stat 0 count = ") << m_stat0Count << std::endl;
	dout << TEXT("Stat 1 count = ") << m_stat1Count << std::endl;
#endif


	unguard;
}

void UD3D9RenderDevice::Flush(UBOOL AllowPrecache) {
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

	if (AllowPrecache && UsePrecache && !GIsEditor) {
		PrecacheOnFlip = 1;
	}

	SetGamma(Viewport->GetOuterUClient()->Brightness);

	unguard;
}


void UD3D9RenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet) {
	return;/*
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: DrawComplexSurface = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::DrawComplexSurface);

	EndBuffering();

# if UTGLR_USES_SCENENODEHACK
	if (SceneNodeHack) {
		if ((Frame->X != m_sceneNodeX) || (Frame->Y != m_sceneNodeY)) {
#ifdef D3D9_DEBUG
			m_sceneNodeHackCount++;
#endif
			SetSceneNode(Frame);
		}
	}
# endif

	SetDefaultAAState();
	SetDefaultProjectionState();
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
	//Sets m_csPolyCount
	//m_csPtCount set later from return value
	//Sets MultiDrawFirstArray and MultiDrawCountArray
	INT numVerts;
	numVerts = BufferStaticComplexSurfaceGeometry(Facet);

	//Reject invalid surfaces early
	if (numVerts == 0) {
		return;
	}

	//Save number of points
	m_csPtCount = numVerts;

	//See if detail texture should be drawn
	//FogMap and DetailTexture are mutually exclusive effects
	bool drawDetailTexture = false;
	if ((DetailTextures != 0) && Surface.DetailTexture && !Surface.FogMap) {
		drawDetailTexture = true;
	}

	//Check for detail texture
	if (drawDetailTexture == true) {
		DWORD anyIsNearBits;

		//Buffer detail texture data
		anyIsNearBits = (this->*m_pBufferDetailTextureDataProc)(380.0f);

		//Do not draw detail texture if no vertices are near
		if (anyIsNearBits == 0) {
			drawDetailTexture = false;
		}
	}
	
	//FCoords coord = FCoords(Frame->Uncoords);
	//FCoords coordRot = FCoords(FVector(0,0,0), coord.XAxis, coord.YAxis, coord.ZAxis);
	//FMatrix fMat = FMatrixFromFCoords(Frame->Uncoords);
	FCoords FC = Frame->Uncoords;
	FMatrix fMat;
	fMat.XPlane = FPlane(FC.XAxis.X, FC.XAxis.Y, FC.XAxis.Z, FC.Origin.X);
	fMat.YPlane = FPlane(FC.ZAxis.X, FC.ZAxis.Y, FC.ZAxis.Z, FC.Origin.Z);
	fMat.ZPlane = FPlane(FC.YAxis.X, FC.YAxis.Y, FC.YAxis.Z, FC.Origin.Y);
	fMat.WPlane = FPlane(0.f, 0.f, 0.f, 1.f);
	//dout << "coord: " << *coord.String() << std::endl;
	D3DMATRIX view = {
		fMat.M(0,0), fMat.M(0,1), fMat.M(0,2), fMat.M(0,3),
		fMat.M(1,0), fMat.M(1,1), fMat.M(1,2), fMat.M(1,3),
		fMat.M(2,0), fMat.M(2,1), fMat.M(2,2), fMat.M(2,3),
		fMat.M(3,0), fMat.M(3,1), fMat.M(3,2), fMat.M(3,3),
	};
	/*D3DMATRIX view = {
		fMat.XPlane.X, fMat.YPlane.X, fMat.ZPlane.X, fMat.WPlane.X,
		fMat.XPlane.Y, fMat.YPlane.Y, fMat.ZPlane.Y, fMat.WPlane.Y,
		fMat.XPlane.Z, fMat.YPlane.Z, fMat.ZPlane.Z, fMat.WPlane.Z,
		fMat.XPlane.W, fMat.YPlane.W, fMat.ZPlane.W, fMat.WPlane.W,
	};*/
	/*D3DMATRIX view = {
		fMat.XPlane.X, fMat.XPlane.Y, fMat.XPlane.Z, fMat.XPlane.W,
		fMat.YPlane.X, fMat.YPlane.Y, fMat.YPlane.Z, fMat.YPlane.W,
		fMat.ZPlane.X, fMat.ZPlane.Y, fMat.ZPlane.Z, fMat.ZPlane.W,
		fMat.WPlane.X, fMat.WPlane.Y, fMat.WPlane.Z, fMat.WPlane.W,
	};/


	char buf[1024];
	std::sprintf(buf, "{\n%f, %f, %f, %f\n%f, %f, %f, %f\n%f, %f, %f, %f\n%f, %f, %f, %f\n}", view._11, view._12, view._13, view._14, view._21, view._22, view._23, view._24, view._31, view._32, view._33, view._34, view._41, view._42, view._43, view._44);
	//dout << buf << std::endl;

	DirectX::XMStoreFloat4x4(
		(DirectX::XMFLOAT4X4*)&view,
		DirectX::XMMatrixLookAtRH(
			DirectX::XMVectorSet(200.f, 200.f, 200.f, 0.f),
			DirectX::g_XMZero,
			DirectX::XMVectorSet(0, 0, 1, 0)
		)
	);

	m_d3dDevice->SetTransform(D3DTS_VIEW, &view);

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

	// Draw detail texture overlaid, in a separate pass.
	if (drawDetailTexture == true) {
		bool singlePassDetail = false;

		//Check if single pass detail mode is enabled and if can use it
		if (SinglePassDetail) {
			//Only attempt single pass detail if single texture rendering wasn't forced earlier
			if (!m_rpForceSingle) {
				//Single pass detail must be done with one or two normal passes
				if ((m_rpPassCount == 1) || (m_rpPassCount == 2)) {
					singlePassDetail = true;
				}
			}
		}

		if (singlePassDetail) {
			RenderPasses_SingleOrDualTextureAndDetailTexture(*Surface.DetailTexture);
		}
		else {
			RenderPasses();

			bool clipDetailTexture = (DetailClipping != 0);

			if (m_rpMasked) {
				//Cannot use detail texture clipping with masked mode
				//It will not work with m_d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_EQUAL);
				clipDetailTexture = false;

				if (m_rpSetDepthEqual == false) {
					m_d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_EQUAL);
					m_rpSetDepthEqual = true;
				}
			}
			DrawDetailTexture(*Surface.DetailTexture, clipDetailTexture);
		}
	}
	else {
		RenderPasses();
	}

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
			if ((m_curVertexBufferPos + NumPts) >= VERTEX_ARRAY_SIZE) {
				FlushVertexBuffers();
			}

			//Lock vertexColor and texCoord0 buffers
			LockVertexColorBuffer();
			LockTexCoordBuffer(0);

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
			m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos, NumPts - 2);

			//Advance vertex buffer position
			m_curVertexBufferPos += NumPts;
		}
	}

	if (m_rpSetDepthEqual == true) {
		m_d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	}

	unclockFast(ComplexCycles);
	unguard;//*/
}

#ifdef UTGLR_RUNE_BUILD
void UD3D9RenderDevice::PreDrawFogSurface() {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: PreDrawFogSurface = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::PreDrawFogSurface);

	EndBuffering();

	SetDefaultAAState();
	SetDefaultProjectionState();
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
		if ((m_curVertexBufferPos + NumPts) >= VERTEX_ARRAY_SIZE) {
			FlushVertexBuffers();
		}

		//Lock vertexColor and texCoord0 buffers
		LockVertexColorBuffer();
		LockTexCoordBuffer(0);

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
	return;
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: DrawGouraudPolygonOld = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::DrawGouraudPolygonOld);
	clockFast(GouraudCycles);

	//Decide if should request near Z range hack projection
	bool requestNearZRangeHackProjection = false;
	if (m_useZRangeHack && (GUglyHackFlags & 0x1)) {
		requestNearZRangeHackProjection = true;
	}
	//Set projection state
	SetProjectionState(requestNearZRangeHackProjection);

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
	if ((m_curVertexBufferPos + NumPts) >= VERTEX_ARRAY_SIZE) {
		FlushVertexBuffers();
	}

	//Lock vertexColor and texCoord0 buffers
	//Lock secondary color buffer if fog
	LockVertexColorBuffer();
	if (drawFog) {
		LockSecondaryColorBuffer();
	}
	LockTexCoordBuffer(0);

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
#ifdef UTGLR_RUNE_BUILD
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
	m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos, NumPts - 2);

	//Advance vertex buffer position
	m_curVertexBufferPos += NumPts;

#ifdef UTGLR_DEBUG_ACTOR_WIREFRAME
	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
#endif

	unclockFast(GouraudCycles);
	unguard;
}

void UD3D9RenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span) {
	return;
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utd3d9r: DrawGouraudPolygon = " << si++ << std::endl;
}
#endif
	guard(UD3D9RenderDevice::DrawGouraudPolygon);

	//DrawGouraudPolygon only uses PolyFlags2 locally
	DWORD PolyFlags2 = 0;

	EndBufferingExcept(BV_TYPE_GOURAUD_POLYS);

# if UTGLR_USES_SCENENODEHACK
	if (SceneNodeHack) {
		if ((Frame->X != m_sceneNodeX) || (Frame->Y != m_sceneNodeY)) {
#ifdef D3D9_DEBUG
			m_sceneNodeHackCount++;
#endif
			SetSceneNode(Frame);
		}
	}
# endif

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

	if (NumPts > m_bufferActorTrisCutoff) {
		EndBuffering();

		SetDefaultAAState();
		//No need to set default projection state here as DrawGouraudPolygonOld will set its own projection state
		//No need to set default stream state here as DrawGouraudPolygonOld will set its own stream state
		SetDefaultTextureState();

		DrawGouraudPolygonOld(Frame, Info, Pts, NumPts, PolyFlags, Span);

		return;
	}

	//Load texture cache id
	QWORD CacheID = Info.CacheID;

	//Only attempt to alter texture cache id on certain textures
	if ((CacheID & 0xFF) == 0xE0) {
		//Alter texture cache id if masked texture hack is enabled and texture is masked
		CacheID |= ((PolyFlags & PF_Masked) ? TEX_CACHE_ID_FLAG_MASKED : 0) & m_maskedTextureHackMask;

		//Check for 16 bit texture option
		if (Use16BitTextures) {
			if (Info.Palette && (Info.Palette[128].A == 255)) {
				CacheID |= TEX_CACHE_ID_FLAG_16BIT;
			}
		}
	}

	//Decide if should request near Z range hack projection
	if (m_useZRangeHack && (GUglyHackFlags & 0x1)) {
		PolyFlags2 |= PF2_NEAR_Z_RANGE_HACK;
	}

	//Check if need to start new poly buffering
	//Make sure enough entries are left in the vertex buffers
	//based on the current position when it was locked
	if ((m_curPolyFlags != PolyFlags) ||
		(m_curPolyFlags2 != PolyFlags2) ||
		(TexInfo[0].CurrentCacheID != CacheID) ||
		((m_curVertexBufferPos + m_bufferedVerts + NumPts) >= (VERTEX_ARRAY_SIZE - 14)) ||
		(m_bufferedVerts == 0))
	{
		//Flush any previously buffered gouraud polys
		EndBuffering();

		//Check if vertex buffer flush is required
		if ((m_curVertexBufferPos + m_bufferedVerts + NumPts) >= (VERTEX_ARRAY_SIZE - 14)) {
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
		m_curPolyFlags2 = PolyFlags2;

		//Set request near Z range hack projection flag
		m_requestNearZRangeHackProjection = (PolyFlags2 & PF2_NEAR_Z_RANGE_HACK) ? true : false;

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
		LockVertexColorBuffer();
		if (m_requestedColorFlags & CF_FOG_MODE) {
			LockSecondaryColorBuffer();
		}
		LockTexCoordBuffer(0);

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

	//DrawTile does not use PolyFlags2
	const DWORD PolyFlags2 = 0;

	// stijn: fix for invisible actor icons in ortho viewports
	if (GIsEditor &&
		Frame->Viewport->Actor &&
		(Frame->Viewport->IsOrtho() || Abs(Z) <= SMALL_NUMBER))
	{
		Z = 1.f;
	}

	EndBufferingExcept(BV_TYPE_TILES);

# if UTGLR_USES_SCENENODEHACK
	if (SceneNodeHack) {
		if ((Frame->X != m_sceneNodeX) || (Frame->Y != m_sceneNodeY)) {
#ifdef D3D9_DEBUG
			m_sceneNodeHackCount++;
#endif
			SetSceneNode(Frame);
		}
	}
# endif

	//Adjust Z coordinate if Z range hack is active
	if (m_useZRangeHack) {
		if ((Z >= 0.5f) && (Z < 8.0f)) {
			Z = (((Z - 0.5f) / 7.5f) * 4.0f) + 4.0f;
		}
	}

	FLOAT PX1 = X - Frame->FX2;
	FLOAT PX2 = PX1 + XL;
	FLOAT PY1 = Y - Frame->FY2;
	FLOAT PY2 = PY1 + YL;

	FLOAT RPX1 = m_RFX2 * PX1;
	FLOAT RPX2 = m_RFX2 * PX2;
	FLOAT RPY1 = m_RFY2 * PY1;
	FLOAT RPY2 = m_RFY2 * PY2;
	if (!Frame->Viewport->IsOrtho()) {
		RPX1 *= Z;
		RPX2 *= Z;
		RPY1 *= Z;
		RPY2 *= Z;
	}

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
		/*
		triPts[0].x = RPX1;
		triPts[0].y = RPY1;
		triPts[0].z = Z;

		triPts[1].x = RPX2;*/
		triPts[1].y = RPY2;/*
		triPts[1].z = Z;
		*/
		triPts[2].x = RPX1;/*
		triPts[2].y = RPY2;
		triPts[2].z = Z;
		*/
		m_gclip.SelectDrawTri(Frame, triPts);

		return;
	}

	if (1) {
		//Load texture cache id
		QWORD CacheID = Info.CacheID;

		//Only attempt to alter texture cache id on certain textures
		if ((CacheID & 0xFF) == 0xE0) {
			//Alter texture cache id if masked texture hack is enabled and texture is masked
			CacheID |= ((PolyFlags & PF_Masked) ? TEX_CACHE_ID_FLAG_MASKED : 0) & m_maskedTextureHackMask;

			//Check for 16 bit texture option
			if (Use16BitTextures) {
				if (Info.Palette && (Info.Palette[128].A == 255)) {
					CacheID |= TEX_CACHE_ID_FLAG_16BIT;
				}
			}
		}

		//Check if need to start new tile buffering
		if ((m_curPolyFlags != PolyFlags) ||
			(m_curPolyFlags2 != PolyFlags2) ||
			(TexInfo[0].CurrentCacheID != CacheID) ||
			((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_ARRAY_SIZE - 6)) ||
			(m_bufferedVerts == 0))
		{
			//Flush any previously buffered tiles
			EndBuffering();

			//Check if vertex buffer flush is required
			if ((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_ARRAY_SIZE - 6)) {
				FlushVertexBuffers();
			}

			//Start tile buffering
			StartBuffering(BV_TYPE_TILES);

			//Update current poly flags (before possible local modification)
			m_curPolyFlags = PolyFlags;
			m_curPolyFlags2 = PolyFlags2;

			//Set default texture state
			SetDefaultTextureState();

			SetBlend(PolyFlags);
			SetTextureNoPanBias(0, Info, PolyFlags);

			if (PolyFlags & PF_Modulated) {
				m_requestedColorFlags = 0;
			}
			else {
				m_requestedColorFlags = CF_COLOR_ARRAY;
			}

			//Lock vertexColor and texCoord0 buffers
			LockVertexColorBuffer();
			LockTexCoordBuffer(0);

			//Set stream state
			SetDefaultStreamState();
		}

		//Get tile color
		DWORD tileColor;
		tileColor = 0xFFFFFFFF;
		if (!(PolyFlags & PF_Modulated)) {
			if (UseSSE2) {
#ifdef UTGLR_INCLUDE_SSE_CODE
				static __m128 fColorMul = { 255.0f, 255.0f, 255.0f, 0.0f };
				__m128 fColorMulReg;
				__m128 fColor;
				__m128 fAlpha;
				__m128i iColor;

				fColorMulReg = fColorMul;
				fColor = _mm_loadu_ps(&Color.X);
				fColor = _mm_mul_ps(fColor, fColorMulReg);

				//RGBA to BGRA
				fColor = _mm_shuffle_ps(fColor, fColor,  _MM_SHUFFLE(3, 0, 1, 2));

				fAlpha = _mm_setzero_ps();
				fAlpha = _mm_move_ss(fAlpha, fColorMulReg);
#if UTGLR_USES_ALPHABLEND
				if (PolyFlags & PF_AlphaBlend) {
					if (Info.Texture->Alpha > 0.f)
						fAlpha = _mm_mul_ss(fAlpha, _mm_load_ss(&Info.Texture->Alpha));
					else
						fAlpha = _mm_mul_ss(fAlpha, _mm_load_ss(&Color.W));
				}
#endif
				fAlpha = _mm_shuffle_ps(fAlpha, fAlpha,  _MM_SHUFFLE(0, 1, 1, 1));

				fColor = _mm_or_ps(fColor, fAlpha);

				iColor = _mm_cvtps_epi32(fColor);
				iColor = _mm_packs_epi32(iColor, iColor);
				iColor = _mm_packus_epi16(iColor, iColor);

				tileColor = _mm_cvtsi128_si32(iColor);
#endif
			}
			else {
#ifdef UTGLR_USES_ALPHABLEND
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
		const DWORD PolyFlags2 = 0;

		//Check if need to start new line buffering
		if ((m_curPolyFlags != PolyFlags) ||
			(m_curPolyFlags2 != PolyFlags2) ||
			(TexInfo[0].CurrentCacheID != TEX_CACHE_ID_NO_TEX) ||
			((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_ARRAY_SIZE - 2)) ||
			(m_bufferedVerts == 0))
		{
			//Flush any previously buffered lines
			EndBuffering();

			//Check if vertex buffer flush is required
			if ((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_ARRAY_SIZE - 2)) {
				FlushVertexBuffers();
			}

			//Start line buffering
			StartBuffering(BV_TYPE_LINES);

			SetDefaultAAState();
			SetDefaultProjectionState();
			SetDefaultStreamState();
			SetDefaultTextureState();

			//Update current poly flags
			m_curPolyFlags = PolyFlags;
			m_curPolyFlags2 = PolyFlags2;

			//Set blending and no texture for lines
			SetBlend(PolyFlags);
			SetNoTexture(0);

			//Lock vertexColor and texCoord0 buffers
			LockVertexColorBuffer();
			LockTexCoordBuffer(0);
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
	const DWORD PolyFlags2 = 0;

	//Check if need to start new line buffering
	if ((m_curPolyFlags != PolyFlags) ||
		(m_curPolyFlags2 != PolyFlags2) ||
		(TexInfo[0].CurrentCacheID != TEX_CACHE_ID_NO_TEX) ||
		((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_ARRAY_SIZE - 2)) ||
		(m_bufferedVerts == 0))
	{
		//Flush any previously buffered lines
		EndBuffering();

		//Check if vertex buffer flush is required
		if ((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_ARRAY_SIZE - 2)) {
			FlushVertexBuffers();
		}

		//Start line buffering
		StartBuffering(BV_TYPE_LINES);

		SetDefaultAAState();
		SetDefaultProjectionState();
		SetDefaultStreamState();
		SetDefaultTextureState();

		//Update current poly flags
		m_curPolyFlags = PolyFlags;
		m_curPolyFlags2 = PolyFlags2;

		//Set blending and no texture for lines
		SetBlend(PolyFlags);
		SetNoTexture(0);

		//Lock vertexColor and texCoord0 buffers
		LockVertexColorBuffer();
		LockTexCoordBuffer(0);
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
	const DWORD PolyFlags2 = 0;

	//Check if need to start new point buffering
	if ((m_curPolyFlags != PolyFlags) ||
		(m_curPolyFlags2 != PolyFlags2) ||
		(TexInfo[0].CurrentCacheID != TEX_CACHE_ID_NO_TEX) ||
		((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_ARRAY_SIZE - 6)) ||
		(m_bufferedVerts == 0))
	{
		//Flush any previously buffered points
		EndBuffering();

		//Check if vertex buffer flush is required
		if ((m_curVertexBufferPos + m_bufferedVerts) >= (VERTEX_ARRAY_SIZE - 6)) {
			FlushVertexBuffers();
		}

		//Start point buffering
		StartBuffering(BV_TYPE_POINTS);

		SetDefaultAAState();
		SetDefaultProjectionState();
		SetDefaultStreamState();
		SetDefaultTextureState();

		//Update current poly flags
		m_curPolyFlags = PolyFlags;
		m_curPolyFlags2 = PolyFlags2;

		//Set blending and no texture for points
		SetBlend(PolyFlags);
		SetNoTexture(0);

		//Lock vertexColor and texCoord0 buffers
		LockVertexColorBuffer();
		LockTexCoordBuffer(0);
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


D3DCOLORVALUE hsvToRgb(unsigned char h, unsigned char s, unsigned char v) {
	float H = h / 255.0f;
	float S = s / 255.0f;
	float V = v / 255.0f;

	float C = V * S;
	float X = C * (1 - std::abs(fmod(H * 6, 2) - 1));
	float m = V - C;

	D3DCOLORVALUE rgbColor;

	if (0 <= H && H < 1 / 6.0) {
		rgbColor.r = C; rgbColor.g = X; rgbColor.b = 0;
	}
	else if (1 / 6.0 <= H && H < 2 / 6.0) {
		rgbColor.r = X; rgbColor.g = C; rgbColor.b = 0;
	}
	else if (2 / 6.0 <= H && H < 3 / 6.0) {
		rgbColor.r = 0; rgbColor.g = C; rgbColor.b = X;
	}
	else if (3 / 6.0 <= H && H < 4 / 6.0) {
		rgbColor.r = 0; rgbColor.g = X; rgbColor.b = C;
	}
	else if (4 / 6.0 <= H && H < 5 / 6.0) {
		rgbColor.r = X; rgbColor.g = 0; rgbColor.b = C;
	}
	else {
		rgbColor.r = C; rgbColor.g = 0; rgbColor.b = X;
	}

	rgbColor.r += m;
	rgbColor.g += m;
	rgbColor.b += m;

	// Set alpha value to maximum
	rgbColor.a = 1.0f;

	return rgbColor;
}

std::wostream& operator<<(std::wostream& os, const D3DMATRIX& mat) {
	os << std::fixed << std::setprecision(2);
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			os << mat.m[i][j] << L' ';
		}
		os << L'\n';
	}
	return os;
}

void UD3D9RenderDevice::SetStaticBsp(FStaticBspInfoBase& staticBspInfo) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
	{
		static int si;
		dout << L"utd3d9r: SetStaticBsp = " << si++ << std::endl;
	}
#endif
	guard(UD3D9RenderDevice::SetStaticBsp);

	staticBspInfo.Update();
//	StaticBspData.TimeSeconds = StaticBspInfo.Level->TimeSeconds.GetFloat();

	if ( GIsEditor )
		return;

	static int lightCount;
	if (staticBspInfo.SourceGeometryChanged) {
		dout << "StaticBspInfo changed!" << std::endl;
		staticBspInfo.SourceGeometryChanged = 0;
		for (int i = 0; i < lightCount; i++) {
			D3DLIGHT9 light = D3DLIGHT9();
			light.Type = D3DLIGHT_POINT;
			light.Position = D3DVECTOR{ 0, 0, 0 };
			light.Diffuse = D3DCOLORVALUE{ 0, 255, 255, 0 };
			light.Specular = light.Diffuse;
			light.Range = 0;
			HRESULT res = m_d3dDevice->SetLight(i, &light);
			assert(res == D3D_OK);
			res = m_d3dDevice->LightEnable(i, false);
			assert(res == D3D_OK);
		}
	}

	staticBspInfo.ComputeStaticGeometry( EStaticBspMode::STATIC_BSP_PerSurf );

	// This render device prefers precalculated light/atlas UV's
	staticBspInfo.ComputeLightUV();


	EndBuffering();
	SetDefaultAAState();
	SetDefaultProjectionState();

	D3DMATRIX oldView;
	m_d3dDevice->GetTransform(D3DTS_VIEW, &oldView);
	D3DMATRIX oldProj;
	m_d3dDevice->GetTransform(D3DTS_PROJECTION, &oldProj);

	D3DMATRIX proj;
	DirectX::XMStoreFloat4x4(
		(DirectX::XMFLOAT4X4*)&proj,
		DirectX::XMMatrixPerspectiveFovLH(Viewport->Actor->FovAngle * PI / 180.0f, (float)m_sceneNodeX / (float)m_sceneNodeY, 0.5f, 65536.0f)
	);

	m_d3dDevice->SetTransform(D3DTS_PROJECTION, &proj);

	FCoords coord = FCoords(currentFrame->Coords);
	FVector origin = coord.Origin;
	FVector forward =  FVector(0.0f, 0.0f, 1.0f).TransformVectorBy(currentFrame->Uncoords);
	FVector up = FVector(0.0f, -1.0f, 0.0f).TransformVectorBy(currentFrame->Uncoords);

	D3DMATRIX view = D3DMATRIX();
	DirectX::XMStoreFloat4x4(
		(DirectX::XMFLOAT4X4*)&view,
		DirectX::XMMatrixLookToLH(
			DirectX::XMVectorSet(origin.X, origin.Y, origin.Z, 0.f),
			DirectX::XMVectorSet(forward.X, forward.Y, forward.Z, 0.0f),
			DirectX::XMVectorSet(up.X, up.Y, up.Z, 0.0f)
		)
	);

	m_d3dDevice->SetTransform(D3DTS_VIEW, &view);

	m_d3dDevice->SetRenderState(D3DRS_LIGHTING, TRUE);

	lightCount = 0;

	for (int i = 0; i < staticBspInfo.Level->Actors.Num(); i++) {
		AActor* actor = staticBspInfo.Level->Actors(i);
		if (actor && actor->IsA(ALight::StaticClass())) {
			ALight* light = (ALight*)actor;
			D3DLIGHT9 lightInfo = D3DLIGHT9();
			lightInfo.Type = D3DLIGHT_POINT;
			lightInfo.Position = D3DVECTOR{ light->Location.X, light->Location.Y, light->Location.Z };
			lightInfo.Diffuse = hsvToRgb(light->LightHue, 0xFF - light->LightSaturation, light->LightBrightness);
			lightInfo.Specular = lightInfo.Diffuse;
			lightInfo.Range = 1000;
			HRESULT res = m_d3dDevice->SetLight(i, &lightInfo);
			assert(res == D3D_OK);
			res = m_d3dDevice->LightEnable(i, true);
			assert(res == D3D_OK);
			lightCount++;
		}
	}
	assert(lightCount <= m_d3dCaps.MaxActiveLights);
	typedef std::tuple<UTexture*, DWORD> SurfKey;
	std::map<SurfKey, std::vector<FStaticBspSurf>> surfaceMap{};
	for (int i = 0; i < staticBspInfo.SurfList.Num(); i++) {
		FStaticBspSurf surface = staticBspInfo.SurfList(i);
		if (surface.Texture == nullptr || (surface.PolyFlags & (PF_Invisible | PF_FakeBackdrop)))
			continue;
		surfaceMap[std::make_tuple(surface.Texture, surface.PolyFlags)].push_back(surface);
	}

	for (std::pair<SurfKey, std::vector<FStaticBspSurf>> entry : surfaceMap) {
		UTexture* texture = std::get<0>(entry.first);
		DWORD polyFlags = std::get<1>(entry.first);

		m_csPtCount = BufferStaticSurfaceGeometry(staticBspInfo, entry.second);

		//Initialize render passes state information
		m_rpPassCount = 0;
		m_rpTMUnits = TMUnits;
		m_rpForceSingle = false;
		m_rpMasked = ((polyFlags & PF_Masked) == 0) ? false : true;
		m_rpColor = 0xFFFFFFFF;

		FTextureInfo texInfo = FTextureInfo();
		texture->Lock(texInfo, currentFrame->Viewport->CurrentTime, -1, this);

		AddRenderPass(&texInfo, polyFlags & ~PF_FlatShaded, 0.0f);

		RenderPasses();

		texture->Unlock(texInfo);
	}

	m_d3dDevice->SetTransform(D3DTS_VIEW, &oldView);
	m_d3dDevice->SetTransform(D3DTS_PROJECTION, &oldProj);
	m_d3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

	unguard;
}

INT UD3D9RenderDevice::BufferStaticSurfaceGeometry(const FStaticBspInfoBase& staticBspInfo, const std::vector<FStaticBspSurf>& surfaces) {
	INT Index = 0;

	// Buffer "static" geometry.
	m_csPolyCount = 0;
	FGLMapDot* pMapDot = &MapDotArray[0];
	FGLVertex* pVertex = &m_csVertexArray[0];

	for (FStaticBspSurf surface : surfaces) {
		// I was promised to be given triangles
		assert(surface.VertexCount % 3 == 0);
		int tris = surface.VertexCount / 3;

		for (int i = 0; i < tris; i++) {
			INT numPts = 3;

			DWORD csPolyCount = m_csPolyCount;
			MultiDrawFirstArray[csPolyCount] = Index;
			MultiDrawCountArray[csPolyCount] = numPts;
			m_csPolyCount = csPolyCount + 1;

			Index += numPts;
			if (Index > VERTEX_ARRAY_SIZE) {
				return 0;
			}
			for (int j = 0; j < numPts; j++) {
				FStaticBspVertex vert = staticBspInfo.VertList(surface.VertexStart + ((i * 3) + j));

				pMapDot->u = vert.TextureU + surface.PanU;
				pMapDot->v = vert.TextureV + surface.PanV;
				pMapDot++;

				pVertex->x = vert.Point.X;
				pVertex->y = vert.Point.Y;
				pVertex->z = vert.Point.Z;
				pVertex++;
			};
		}
	}

	return Index;
}

void UD3D9RenderDevice::DrawStaticBspNode(INT iNode, FSurfaceInfo& Surface) {
	OutputDebugStringW(L"DrawStaticBspNode()!\n");
};

void UD3D9RenderDevice::DrawStaticBspSurf(INT iSurf, FSurfaceInfo& Surface) {
	OutputDebugStringW(L"DrawStaticBspSurf()!\n");
};

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


	//Gamma correct screenshots if the option is true and the gamma ramp was set successfully
	if (GammaCorrectScreenshots && m_setGammaRampSucceeded) {
		INT DestSizeX = Viewport->SizeX;
		INT DestSizeY = Viewport->SizeY;
		FByteGammaRamp gammaByteRamp;
		BuildGammaRamp(SavedGammaCorrection, SavedGammaCorrection, SavedGammaCorrection, Brightness, gammaByteRamp);
		for (y = 0; y < DestSizeY; y++) {
			for (x = 0; x < DestSizeX; x++) {
				Pixels[x + y * DestSizeX].R = gammaByteRamp.red[Pixels[x + y * DestSizeX].R];
				Pixels[x + y * DestSizeX].G = gammaByteRamp.green[Pixels[x + y * DestSizeX].G];
				Pixels[x + y * DestSizeX].B = gammaByteRamp.blue[Pixels[x + y * DestSizeX].B];
			}
		}
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

		SetDefaultAAState();
		SetDefaultProjectionState();
		SetDefaultStreamState();
		SetDefaultTextureState();

		SetBlend(PF_Highlighted);
		SetNoTexture(0);

		FPlane tempPlane = FPlane(FlashFog.X, FlashFog.Y, FlashFog.Z, 1.0f - Min(FlashScale.X * 2.0f, 1.0f));
		DWORD flashColor = FPlaneTo_BGRA(&tempPlane);

		FLOAT RFX2 = m_RProjZ;
		FLOAT RFY2 = m_RProjZ * m_Aspect;

		//Adjust Z coordinate if Z range hack is active
		FLOAT ZCoord = 1.0f;
		if (m_useZRangeHack) {
			ZCoord = (((ZCoord - 0.5f) / 7.5f) * 4.0f) + 4.0f;
		}

		//Make sure at least 4 entries are left in the vertex buffers
		if ((m_curVertexBufferPos + 4) >= VERTEX_ARRAY_SIZE) {
			FlushVertexBuffers();
		}

		//Lock vertexColor and texCoord0 buffers
		LockVertexColorBuffer();
		LockTexCoordBuffer(0);

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

		pVertexColorArray[0].x = RFX2 * (-1.0f * ZCoord);
		pVertexColorArray[0].y = RFY2 * (-1.0f * ZCoord);
		pVertexColorArray[0].z = ZCoord;
		pVertexColorArray[0].color = flashColor;

		pVertexColorArray[1].x = RFX2 * (+1.0f * ZCoord);
		pVertexColorArray[1].y = RFY2 * (-1.0f * ZCoord);
		pVertexColorArray[1].z = ZCoord;
		pVertexColorArray[1].color = flashColor;

		pVertexColorArray[2].x = RFX2 * (+1.0f * ZCoord);
		pVertexColorArray[2].y = RFY2 * (+1.0f * ZCoord);
		pVertexColorArray[2].z = ZCoord;
		pVertexColorArray[2].color = flashColor;

		pVertexColorArray[3].x = RFX2 * (-1.0f * ZCoord);
		pVertexColorArray[3].y = RFY2 * (+1.0f * ZCoord);
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
	//TODO: m_16BitTextureCap
	case TEXF_R5G6B5: return m_565TextureCap && false; // Renderer is missing code to upload these textures directly.
	default:          return false;
	}
}

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

//This function is safe to call multiple times to initialize once
void UD3D9RenderDevice::InitAlphaTextureSafe(void) {
	guard(UD3D9RenderDevice::InitAlphaTexture);
	unsigned int u;
	HRESULT hResult;
	D3DLOCKED_RECT lockRect;
	BYTE *pTex;

	//Return early if already initialized
	if (m_pAlphaTexObj != 0) {
		return;
	}

	//Create the texture
	hResult = m_d3dDevice->CreateTexture(256, 1, 1, 0, D3DFMT_A8, D3DPOOL_MANAGED, &m_pAlphaTexObj, NULL);
	if (FAILED(hResult)) {
		appErrorf(TEXT("CreateTexture (alpha) failed"));
	}

	//Lock texture level 0
	if (FAILED(m_pAlphaTexObj->LockRect(0, &lockRect, NULL, D3DLOCK_NOSYSLOCK))) {
		appErrorf(TEXT("Texture lock failed"));
	}

	//Write texture
	pTex = (BYTE *)lockRect.pBits;
	for (u = 0; u < 256; u++) {
		pTex[u] = 255 - u;
	}

	//Unlock texture level 0
	if (FAILED(m_pAlphaTexObj->UnlockRect(0))) {
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

void UD3D9RenderDevice::SetAlphaTextureNoCheck(INT Multi) {
	guard(UD3D9RenderDevice::SetAlphaTexture);

	// Set alpha gradient texture.
	clockFast(BindCycles);

	//Set texture
	m_d3dDevice->SetTexture(Multi, m_pAlphaTexObj);

	//Set filter
	SetTexFilter(Multi, CT_MIN_FILTER_LINEAR | CT_MIP_FILTER_NONE | CT_MAG_FILTER_LINEAR_NOT_POINT_BIT | CT_ADDRESS_CLAMP_NOT_WRAP_BIT);

	TexInfo[Multi].CurrentCacheID = TEX_CACHE_ID_ALPHA_TEX;
	TexInfo[Multi].pBind = NULL;

	unclockFast(BindCycles);

	unguard;
}

//This function must use Tex.CurrentCacheID and NEVER use Info.CacheID to reference the texture cache id
//This makes it work with the masked texture hack code

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
	if (!existingBind || Info.bRealtimeChanged) {
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

				default:
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
		case TEXF_DXT3: 
			pBind->texType = TEX_TYPE_COMPRESSED_DXT3; 
			pBind->texFormat = D3DFMT_DXT3;
			break;

		case TEXF_DXT5: 
			pBind->texType = TEX_TYPE_COMPRESSED_DXT5; 
			pBind->texFormat = D3DFMT_DXT5;
			break;

		default:
			;
		}
	}
	if (pBind->texType != TEX_TYPE_NONE) {
		//Using compressed texture
	}
	else if (FIsPalettizedFormat(Info.Format)) {
		pBind->texType = TEX_TYPE_HAS_PALETTE;
		pBind->texFormat = D3DFMT_A8R8G8B8;
		//Check if texture should be 16-bit
		if (PolyFlags & PF_Memorized) {
			pBind->texFormat = (PolyFlags & PF_Masked) ? D3DFMT_A1R5G5B5 : ((Use565Textures) ? D3DFMT_R5G6B5 : D3DFMT_X1R5G5B5);
		}
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

void UD3D9RenderDevice::EnableAlphaToCoverageNoCheck(void) {
	//Vendor specific enable alpha to coverage
	if (m_isATI) {
		m_d3dDevice->SetRenderState(D3DRS_POINTSIZE, MAKEFOURCC('A', '2', 'M', '1'));
	}
	else if (m_isNVIDIA) {
		m_d3dDevice->SetRenderState(D3DRS_ADAPTIVETESS_Y, (D3DFORMAT)MAKEFOURCC('A', 'T', 'O', 'C'));
	}

	return;
}

void UD3D9RenderDevice::DisableAlphaToCoverageNoCheck(void) {
	//Vendor specific disable alpha to coverage
	if (m_isATI) {
		m_d3dDevice->SetRenderState(D3DRS_POINTSIZE, MAKEFOURCC('A', '2', 'M', '0'));
	}
	else if (m_isNVIDIA) {
		m_d3dDevice->SetRenderState(D3DRS_ADAPTIVETESS_Y, D3DFMT_UNKNOWN);
	}

	return;
}

void UD3D9RenderDevice::SetBlendNoCheck(DWORD blendFlags) {
	guardSlow(UD3D9RenderDevice::SetBlend);

	// Detect changes in the blending modes.
	DWORD Xor = m_curBlendFlags ^ blendFlags;

	//Save copy of current blend flags
	DWORD curBlendFlags = m_curBlendFlags;

	//Update main copy of current blend flags early
	m_curBlendFlags = blendFlags;

	const DWORD GL_BLEND_FLAG_BITS = PF_Translucent | PF_Modulated | PF_Highlighted | PF_AlphaBlend;
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
				m_d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR);
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

			if (m_useAlphaToCoverageForMasked) {
				m_alphaToCoverageEnabled = true;
				EnableAlphaToCoverageNoCheck();
			}
		}
		else {
			//Disable alpha test
			m_fsBlendInfo[0] = 0.0f;

			m_alphaTestEnabled = false;
			m_d3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

			if (m_useAlphaToCoverageForMasked) {
				m_alphaToCoverageEnabled = false;
				DisableAlphaToCoverageNoCheck();
			}
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
void UD3D9RenderDevice::SetAAStateNoCheck(bool AAEnable) {
	//Save new AA state
	m_curAAEnable = AAEnable;

#ifdef D3D9_DEBUG
	m_AASwitchCount++;
#endif

	//Set new AA state
	m_d3dDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, (AAEnable) ? TRUE : FALSE);

	return;
}


void UD3D9RenderDevice::SetProjectionStateNoCheck(bool requestNearZRangeHackProjection) {
	float left, right, bottom, top, zNear, zFar;
	float invRightMinusLeft, invTopMinusBottom, invNearMinusFar;

	//Save new Z range hack projection state
	m_nearZRangeHackProjectionActive = requestNearZRangeHackProjection;

	//Set default zNearVal
	FLOAT zNearVal = 0.5f;

	FLOAT zScaleVal = 1.0f;
	if (requestNearZRangeHackProjection) {
#ifdef UTGLR_DEBUG_Z_RANGE_HACK_WIREFRAME
		m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
#endif

		zScaleVal = 0.125f;
		zNearVal = 0.5;
	}
	else {
#ifdef UTGLR_DEBUG_Z_RANGE_HACK_WIREFRAME
		m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
#endif

		if (m_useZRangeHack) {
			zNearVal = 4.0f;
		}
	}

	left = -m_RProjZ * zNearVal;
	right = +m_RProjZ * zNearVal;
	bottom = -m_Aspect*m_RProjZ * zNearVal;
	top = +m_Aspect*m_RProjZ * zNearVal;
	zNear = 1.0f * zNearVal;

	//Set zFar
	zFar = 65536.0f; // stijn: increased from 32k to 64k for UT v469b
	if (requestNearZRangeHackProjection) {
		zFar *= zScaleVal;
	}

	invRightMinusLeft = 1.0f / (right - left);
	invTopMinusBottom = 1.0f / (top - bottom);
	invNearMinusFar = 1.0f / (zNear - zFar);

	D3DMATRIX d3dProj;
	d3dProj.m[0][0] = 2.0f * zNear * invRightMinusLeft;
	d3dProj.m[0][1] = 0.0f;
	d3dProj.m[0][2] = 0.0f;
	d3dProj.m[0][3] = 0.0f;

	d3dProj.m[1][0] = 0.0f;
	d3dProj.m[1][1] = 2.0f * zNear * invTopMinusBottom;
	d3dProj.m[1][2] = 0.0f;
	d3dProj.m[1][3] = 0.0f;

	d3dProj.m[2][0] = 1.0f / (FLOAT)m_sceneNodeX;
	d3dProj.m[2][1] = -1.0f / (FLOAT)m_sceneNodeY;
	d3dProj.m[2][2] = zScaleVal * (zFar * invNearMinusFar);
	d3dProj.m[2][3] = -1.0f;

	d3dProj.m[3][0] = 0.0f;
	d3dProj.m[3][1] = 0.0f;
	d3dProj.m[3][2] = zScaleVal * zScaleVal * (zNear * zFar * invNearMinusFar);
	d3dProj.m[3][3] = 0.0f;

	m_d3dDevice->SetTransform(D3DTS_PROJECTION, &d3dProj);

	return;
}

void UD3D9RenderDevice::SetOrthoProjection(void) {
	float left, right, bottom, top, zNear, zFar;
	float invRightMinusLeft, invTopMinusBottom, invNearMinusFar;
	D3DMATRIX d3dProj;

	//Save new Z range hack projection state
	m_nearZRangeHackProjectionActive = false;

	left = -m_RProjZ;
	right = +m_RProjZ;
	bottom = -m_Aspect*m_RProjZ;
	top = +m_Aspect*m_RProjZ;
	zNear = 1.0f * 0.5f;
	zFar = 65536.0f; // stijn: increased from 32k to 64k for UT v469b

	invRightMinusLeft = 1.0f / (right - left);
	invTopMinusBottom = 1.0f / (top - bottom);
	invNearMinusFar = 1.0f / (zNear - zFar);

	d3dProj.m[0][0] = 2.0f * invRightMinusLeft;
	d3dProj.m[0][1] = 0.0f;
	d3dProj.m[0][2] = 0.0f;
	d3dProj.m[0][3] = 0.0f;

	d3dProj.m[1][0] = 0.0f;
	d3dProj.m[1][1] = 2.0f * invTopMinusBottom;
	d3dProj.m[1][2] = 0.0f;
	d3dProj.m[1][3] = 0.0f;

	d3dProj.m[2][0] = 0.0f;
	d3dProj.m[2][1] = 0.0f;
	d3dProj.m[2][2] = 1.0f * invNearMinusFar;
	d3dProj.m[2][3] = 0.0f;

	d3dProj.m[3][0] = -1.0f / (FLOAT)m_sceneNodeX;
	d3dProj.m[3][1] = 1.0f / (FLOAT)m_sceneNodeY;
	d3dProj.m[3][2] = zNear * invNearMinusFar;
	d3dProj.m[3][3] = 1.0f;

	m_d3dDevice->SetTransform(D3DTS_PROJECTION, &d3dProj);

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


	for (INT PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++) {
		assert(MultiDrawCountArray[PolyNum] == 3);
		//m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos + MultiDrawFirstArray[PolyNum], MultiDrawCountArray[PolyNum] - 2);
	}
	m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, m_curVertexBufferPos, m_csPolyCount);

#ifdef UTGLR_DEBUG_WORLD_WIREFRAME
	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);

	SetBlend(PF_Modulated);

	for (PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++) {
		m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos + MultiDrawFirstArray[PolyNum], MultiDrawCountArray[PolyNum] - 2);
	}

	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
#endif

	//Advance vertex buffer position
	m_curVertexBufferPos += m_csPtCount;

#if 0
{
	dout << L"utd3d9r: PassCount = " << m_rpPassCount << std::endl;
}
#endif
	m_rpPassCount = 0;


	unguard;
}

void UD3D9RenderDevice::RenderPassesExec_SingleOrDualTextureAndDetailTexture(FTextureInfo &DetailTextureInfo) {
	guard(UD3D9RenderDevice::RenderPassesExec_SingleOrDualTextureAndDetailTexture);

	//Some render passes paths may use fragment program

	//The dual texture and detail texture path can never be executed if single pass rendering were forced earlier
	//The depth function will never need to be changed due to single pass rendering here

	//Call the render passes no check setup dual texture and detail texture proc
	RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture(DetailTextureInfo);

	//Single texture rendering does not need to be forced here since the detail texture is always the last pass


	for (INT PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++) {
		m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos + MultiDrawFirstArray[PolyNum], MultiDrawCountArray[PolyNum] - 2);
	}

#ifdef UTGLR_DEBUG_WORLD_WIREFRAME
	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);

	SetBlend(PF_Modulated);

	for (PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++) {
		m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos + MultiDrawFirstArray[PolyNum], MultiDrawCountArray[PolyNum] - 2);
	}

	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
#endif

	//Advance vertex buffer position
	m_curVertexBufferPos += m_csPtCount;

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

	//Make sure at least m_csPtCount entries are left in the vertex buffers
	if ((m_curVertexBufferPos + m_csPtCount) >= VERTEX_ARRAY_SIZE) {
		FlushVertexBuffers();
	}

	//Lock vertexColor and texCoord buffers
	LockVertexColorBuffer();
	t = 0;
	do {
		LockTexCoordBuffer(t);
	} while (++t < m_rpPassCount);

	//Write vertex and color
	const FGLVertex *pSrcVertexArray = m_csVertexArray;
	FGLVertexColor *pVertexColorArray = m_pVertexColorArray;
	DWORD rpColor = m_rpColor;
	i = m_csPtCount;
	do {
		pVertexColorArray->x = pSrcVertexArray->x;
		pVertexColorArray->y = pSrcVertexArray->y;
		pVertexColorArray->z = pSrcVertexArray->z;
		pVertexColorArray->color = rpColor;
		pSrcVertexArray++;
		pVertexColorArray++;
	} while (--i != 0);

	//Write texCoord
	t = 0;
	do {
		FLOAT UPan = TexInfo[t].UPan;
		FLOAT VPan = TexInfo[t].VPan;
		FLOAT UMult = TexInfo[t].UMult;
		FLOAT VMult = TexInfo[t].VMult;
		const FGLMapDot *pMapDot = MapDotArray;
		FGLTexCoord *pTexCoord = m_pTexCoordArray[t];

		INT ptCounter = m_csPtCount;
		do {
			pTexCoord->u = (pMapDot->u - UPan) * UMult;
			pTexCoord->v = (pMapDot->v - VPan) * VMult;

			pMapDot++;
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

//Must be called with (m_rpPassCount > 0)
void UD3D9RenderDevice::RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture(FTextureInfo &DetailTextureInfo) {
	INT i;
	INT t;
	FLOAT NearZ  = 380.0f;
	FLOAT RNearZ = 1.0f / NearZ;

	//Two extra texture units used for detail texture
	m_rpPassCount += 2;

	SetBlend(MultiPass.TMU[0].PolyFlags);

	//Surface texture must be 2X blended
	//Also force PF_Modulated for the TexEnv stage
	MultiPass.TMU[0].PolyFlags |= (PF_Modulated | PF_FlatShaded);

	//Detail texture uses first two texture units
	//Other textures use last two texture units
	i = 2;
	do {
		if (i != 0) {
			SetTexEnv(i, MultiPass.TMU[i - 2].PolyFlags);
		}

		SetTexture(i, *MultiPass.TMU[i - 2].Info, MultiPass.TMU[i - 2].PolyFlags, MultiPass.TMU[i - 2].PanBias);
	} while (++i < m_rpPassCount);

	SetAlphaTexture(0);

	SetTexEnv(1, PF_Memorized);
	SetTextureNoPanBias(1, DetailTextureInfo, PF_Modulated);

	//Set stream state based on number of texture units in use
	SetStreamState(m_standardNTextureVertexDecl[m_rpPassCount - 1]);

	//Check for additional enabled texture units that should be disabled
	DisableSubsequentTextures(m_rpPassCount);

	//Make sure at least m_csPtCount entries are left in the vertex buffers
	if ((m_curVertexBufferPos + m_csPtCount) >= VERTEX_ARRAY_SIZE) {
		FlushVertexBuffers();
	}

	//Lock vertexColor and texCoord buffers
	LockVertexColorBuffer();
	t = 0;
	do {
		LockTexCoordBuffer(t);
	} while (++t < m_rpPassCount);

	//Write vertex and color
	const FGLVertex *pSrcVertexArray = m_csVertexArray;
	FGLVertexColor *pVertexColorArray = m_pVertexColorArray;
	DWORD detailColor = m_detailTextureColor4ub | 0xFF000000;
	i = m_csPtCount;
	do {
		pVertexColorArray->x = pSrcVertexArray->x;
		pVertexColorArray->y = pSrcVertexArray->y;
		pVertexColorArray->z = pSrcVertexArray->z;
		pVertexColorArray->color = detailColor;
		pSrcVertexArray++;
		pVertexColorArray++;
	} while (--i != 0);

	//Alpha texture for detail texture uses texture unit 0
	{
		INT t = 0;
		const FGLVertex *pVertex = &m_csVertexArray[0];
		FGLTexCoord *pTexCoord = m_pTexCoordArray[t];

		INT ptCounter = m_csPtCount;
		do {
			pTexCoord->u = pVertex->z * RNearZ;
			pTexCoord->v = 0.5f;

			pVertex++;
			pTexCoord++;
		} while (--ptCounter != 0);
	}
	//Detail texture uses texture unit 1
	//Remaining 1 or 2 textures use texture units 2 and 3
	t = 1;
	do {
		FLOAT UPan = TexInfo[t].UPan;
		FLOAT VPan = TexInfo[t].VPan;
		FLOAT UMult = TexInfo[t].UMult;
		FLOAT VMult = TexInfo[t].VMult;
		const FGLMapDot *pMapDot = &MapDotArray[0];
		FGLTexCoord *pTexCoord = m_pTexCoordArray[t];

		INT ptCounter = m_csPtCount;
		do {
			pTexCoord->u = (pMapDot->u - UPan) * UMult;
			pTexCoord->v = (pMapDot->v - VPan) * VMult;

			pMapDot++;
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

//Modified this routine to always set up detail texture state
//It should only be called if at least one polygon will be detail textured
void UD3D9RenderDevice::DrawDetailTexture(FTextureInfo &DetailTextureInfo, bool clipDetailTexture) {
	//Setup detail texture state
	SetBlend(PF_Modulated);

	//Set detail alpha mode flag
	bool detailAlphaMode = ((clipDetailTexture == false) && UseDetailAlpha) ? true : false;

	if (detailAlphaMode) {
		SetAlphaTexture(0);
		//TexEnv 0 is PF_Modulated by default

		SetTextureNoPanBias(1, DetailTextureInfo, PF_Modulated);
		SetTexEnv(1, PF_Memorized);

		//Set stream state for two textures
		SetStreamState(m_standardNTextureVertexDecl[1]);

		//Check for additional enabled texture units that should be disabled
		DisableSubsequentTextures(2);
	}
	else {
		SetTextureNoPanBias(0, DetailTextureInfo, PF_Modulated);
		SetTexEnv(0, PF_Memorized);

		if (clipDetailTexture == true) {
			FLOAT fDepthBias = -1.0f;
			m_d3dDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, *(DWORD *)&fDepthBias);
		}

		//Set stream state for one texture
		SetStreamState(m_standardNTextureVertexDecl[0]);

		//Check for additional enabled texture units that should be disabled
		DisableSubsequentTextures(1);
	}


	//Get detail texture color
	DWORD detailColor = m_detailTextureColor4ub;

	INT detailPassNum = 0;
	FLOAT NearZ  = 380.0f;
	FLOAT RNearZ = 1.0f / NearZ;
	FLOAT DetailScale = 1.0f;
	do {
		//Set up new NearZ and rescan points if subsequent pass
		if (detailPassNum > 0) {
			//Adjust NearZ and detail texture scaling
			NearZ /= 4.223f;
			RNearZ *= 4.223f;
			DetailScale *= 4.223f;

			//Rescan points
			(this->*m_pBufferDetailTextureDataProc)(NearZ);
		}

		//Calculate scaled UMult and VMult for detail texture based on mode
		FLOAT DetailUMult;
		FLOAT DetailVMult;
		if (detailAlphaMode) {
			DetailUMult = TexInfo[1].UMult * DetailScale;
			DetailVMult = TexInfo[1].VMult * DetailScale;
		}
		else {
			DetailUMult = TexInfo[0].UMult * DetailScale;
			DetailVMult = TexInfo[0].VMult * DetailScale;
		}

		INT Index = 0;

		INT *pNumPts = &MultiDrawCountArray[0];
		DWORD *pDetailTextureIsNear = DetailTextureIsNearArray;
		for (DWORD PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++, pNumPts++, pDetailTextureIsNear++) {
			DWORD NumPts = *pNumPts;
			DWORD isNearBits = *pDetailTextureIsNear;

			//Skip the polygon if it will not be detail textured
			if (isNearBits == 0) {
				Index += NumPts;
				continue;
			}
			INT StartIndex = Index;

			DWORD allPtsBits = ~(~0U << NumPts);
			//Detail alpha mode
			if (detailAlphaMode) {
				//Make sure at least NumPts entries are left in the vertex buffers
				if ((m_curVertexBufferPos + NumPts) >= VERTEX_ARRAY_SIZE) {
					FlushVertexBuffers();
				}

				//Lock vertexColor, texCoord0, and texCoord1 buffers
				LockVertexColorBuffer();
				LockTexCoordBuffer(0);
				LockTexCoordBuffer(1);

				FGLTexCoord *pTexCoord0 = m_pTexCoordArray[0];
				FGLTexCoord *pTexCoord1 = m_pTexCoordArray[1];
				FGLVertexColor *pVertexColorArray = m_pVertexColorArray;
				const FGLVertex *pSrcVertexArray = &m_csVertexArray[StartIndex];
				for (INT i = 0; i < NumPts; i++) {
					FLOAT U = MapDotArray[Index].u;
					FLOAT V = MapDotArray[Index].v;

					FLOAT PointZ_Times_RNearZ = pSrcVertexArray[i].z * RNearZ;
					pTexCoord0[i].u = PointZ_Times_RNearZ;
					pTexCoord0[i].v = 0.5f;
					pTexCoord1[i].u = (U - TexInfo[1].UPan) * DetailUMult;
					pTexCoord1[i].v = (V - TexInfo[1].VPan) * DetailVMult;

					pVertexColorArray[i].x = pSrcVertexArray[i].x;
					pVertexColorArray[i].y = pSrcVertexArray[i].y;
					pVertexColorArray[i].z = pSrcVertexArray[i].z;
					pVertexColorArray[i].color = detailColor | 0xFF000000;

					Index++;
				}

				//Unlock vertexColor,texCoord0, and texCoord1 buffers
				UnlockVertexColorBuffer();
				UnlockTexCoordBuffer(0);
				UnlockTexCoordBuffer(1);

				//Draw the triangles
				m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos, NumPts - 2);

				//Advance vertex buffer position
				m_curVertexBufferPos += NumPts;
			}
			//Otherwise, no clipping required, or clipping required, but DetailClipping not enabled
			else if ((clipDetailTexture == false) || (isNearBits == allPtsBits)) {
				//Make sure at least NumPts entries are left in the vertex buffers
				if ((m_curVertexBufferPos + NumPts) >= VERTEX_ARRAY_SIZE) {
					FlushVertexBuffers();
				}

				//Lock vertexColor and texCoord0 buffers
				LockVertexColorBuffer();
				LockTexCoordBuffer(0);

				FGLTexCoord *pTexCoord = m_pTexCoordArray[0];
				FGLVertexColor *pVertexColorArray = m_pVertexColorArray;
				const FGLVertex *pSrcVertexArray = &m_csVertexArray[StartIndex];
				for (INT i = 0; i < NumPts; i++) {
					FLOAT U = MapDotArray[Index].u;
					FLOAT V = MapDotArray[Index].v;

					pTexCoord[i].u = (U - TexInfo[0].UPan) * DetailUMult;
					pTexCoord[i].v = (V - TexInfo[0].VPan) * DetailVMult;

					pVertexColorArray[i].x = pSrcVertexArray[i].x;
					pVertexColorArray[i].y = pSrcVertexArray[i].y;
					pVertexColorArray[i].z = pSrcVertexArray[i].z;
					DWORD alpha = appRound((1.0f - (Clamp(pSrcVertexArray[i].z, 0.0f, NearZ) * RNearZ)) * 255.0f);
					pVertexColorArray[i].color = detailColor | (alpha << 24);

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
			//Otherwise, clipping required and DetailClipping enabled
			else {
				//Make sure at least (NumPts * 2) entries are left in the vertex buffers
				if ((m_curVertexBufferPos + (NumPts * 2)) >= VERTEX_ARRAY_SIZE) {
					FlushVertexBuffers();
				}

				//Lock vertexColor and texCoord0 buffers
				LockVertexColorBuffer();
				LockTexCoordBuffer(0);

				DWORD NextIndex = 0;
				DWORD isNear_i_bit = 1U << (NumPts - 1);
				DWORD isNear_j_bit = 1U;
				FGLTexCoord *pTexCoord = m_pTexCoordArray[0];
				for (INT i = 0, j = NumPts - 1; i < NumPts; j = i++, isNear_j_bit = isNear_i_bit, isNear_i_bit >>= 1) {
					const FGLVertex &Point = m_csVertexArray[Index];
					FLOAT U = MapDotArray[Index].u;
					FLOAT V = MapDotArray[Index].v;

					if (((isNear_i_bit & isNearBits) != 0) && ((isNear_j_bit & isNearBits) == 0)) {
						const FGLVertex &PrevPoint = m_csVertexArray[StartIndex + j];
						FLOAT PrevU = MapDotArray[StartIndex + j].u;
						FLOAT PrevV = MapDotArray[StartIndex + j].v;

						FLOAT dist = PrevPoint.z - Point.z;
						FLOAT m = 1.0f;
						if (dist > 0.001f) {
							m = (NearZ - Point.z) / dist;
						}
						FGLVertexColor *pVertexColor = &m_pVertexColorArray[NextIndex];
						pVertexColor->x = (m * (PrevPoint.x - Point.x)) + Point.x;
						pVertexColor->y = (m * (PrevPoint.y - Point.y)) + Point.y;
						pVertexColor->z = NearZ;
						DWORD alpha = 0;
						pVertexColor->color = detailColor | (alpha << 24);

						pTexCoord[NextIndex].u = ((m * (PrevU - U)) + U - TexInfo[0].UPan) * DetailUMult;
						pTexCoord[NextIndex].v = ((m * (PrevV - V)) + V - TexInfo[0].VPan) * DetailVMult;

						NextIndex++;
					}

					if ((isNear_i_bit & isNearBits) != 0) {
						pTexCoord[NextIndex].u = (U - TexInfo[0].UPan) * DetailUMult;
						pTexCoord[NextIndex].v = (V - TexInfo[0].VPan) * DetailVMult;

						FGLVertexColor *pVertexColor = &m_pVertexColorArray[NextIndex];
						pVertexColor->x = Point.x;
						pVertexColor->y = Point.y;
						pVertexColor->z = Point.z;
						DWORD alpha = appRound((1.0f - (Clamp(Point.z, 0.0f, NearZ) * RNearZ)) * 255.0f);
						pVertexColor->color = detailColor | (alpha << 24);

						NextIndex++;
					}

					if (((isNear_i_bit & isNearBits) == 0) && ((isNear_j_bit & isNearBits) != 0)) {
						const FGLVertex &PrevPoint = m_csVertexArray[StartIndex + j];
						FLOAT PrevU = MapDotArray[StartIndex + j].u;
						FLOAT PrevV = MapDotArray[StartIndex + j].v;

						FLOAT dist = Point.z - PrevPoint.z;
						FLOAT m = 1.0f;
						if (dist > 0.001f) {
							m = (NearZ - PrevPoint.z) / dist;
						}
						FGLVertexColor *pVertexColor = &m_pVertexColorArray[NextIndex];
						pVertexColor->x = (m * (Point.x - PrevPoint.x)) + PrevPoint.x;
						pVertexColor->y = (m * (Point.y - PrevPoint.y)) + PrevPoint.y;
						pVertexColor->z = NearZ;
						DWORD alpha = 0;
						pVertexColor->color = detailColor | (alpha << 24);

						pTexCoord[NextIndex].u = ((m * (U - PrevU)) + PrevU - TexInfo[0].UPan) * DetailUMult;
						pTexCoord[NextIndex].v = ((m * (V - PrevV)) + PrevV - TexInfo[0].VPan) * DetailVMult;

						NextIndex++;
					}

					Index++;
				}

				//Unlock vertexColor and texCoord0 buffers
				UnlockVertexColorBuffer();
				UnlockTexCoordBuffer(0);

				//Draw the triangles
				m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, m_curVertexBufferPos, NextIndex - 2);

				//Advance vertex buffer position
				m_curVertexBufferPos += NextIndex;
			}
		}
	} while (++detailPassNum < DetailMax);


	//Clear detail texture state
	if (detailAlphaMode) {
		//TexEnv 0 was left in default state of PF_Modulated
	}
	else {
		SetTexEnv(0, PF_Modulated);

		if (clipDetailTexture == true) {
			FLOAT fDepthBias = 0.0f;
			m_d3dDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, *(DWORD *)&fDepthBias);
		}
	}

	return;
}


INT UD3D9RenderDevice::BufferStaticComplexSurfaceGeometry(const FSurfaceFacet& Facet) {
	INT Index = 0;

	// Buffer "static" geometry.
	m_csPolyCount = 0;
	FGLMapDot* pMapDot = &MapDotArray[0];
	FGLVertex* pVertex = &m_csVertexArray[0];
	for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next) {
		//Skip if no points
		INT NumPts = Poly->NumPts;
		if (NumPts <= 0) {
			continue;
		}

		DWORD csPolyCount = m_csPolyCount;
		MultiDrawFirstArray[csPolyCount] = Index;
		MultiDrawCountArray[csPolyCount] = NumPts;
		m_csPolyCount = csPolyCount + 1;

		Index += NumPts;
		if (Index > VERTEX_ARRAY_SIZE) {
			return 0;
		}
		FTransform** pPts = &Poly->Pts[0];
		do {
			const FVector& Point = (*pPts++)->Point;//.TransformPointBy(currentFrame->Uncoords);

			pMapDot->u = (Facet.MapCoords.XAxis | Point) - m_csUDot;
			pMapDot->v = (Facet.MapCoords.YAxis | Point) - m_csVDot;
			pMapDot++;

			pVertex->x = Point.X;
			pVertex->y = Point.Y;
			pVertex->z = Point.Z;
			pVertex++;
		} while (--NumPts != 0);
	}

	return Index;
}

DWORD UD3D9RenderDevice::BufferDetailTextureData(FLOAT NearZ) {
	DWORD *pDetailTextureIsNear = DetailTextureIsNearArray;
	DWORD anyIsNearBits = 0;

	FGLVertex *pVertex = &m_csVertexArray[0];
	INT *pNumPts = &MultiDrawCountArray[0];
	DWORD csPolyCount = m_csPolyCount;
	do {
		INT NumPts = *pNumPts++;
		DWORD isNear = 0;

		do {
			isNear <<= 1;
			if (pVertex->z < NearZ) {
				isNear |= 1;
			}
			pVertex++;
		} while (--NumPts != 0);

		*pDetailTextureIsNear++ = isNear;
		anyIsNearBits |= isNear;
	} while (--csPolyCount != 0);

	return anyIsNearBits;
}

#ifdef UTGLR_INCLUDE_SSE_CODE
__declspec(naked) DWORD UD3D9RenderDevice::BufferDetailTextureData_SSE2(FLOAT NearZ) {
	__asm {
		movd xmm0, [esp+4]

		push esi
		push edi

		lea esi, [ecx]this.m_csVertexArray
		lea edx, [ecx]this.MultiDrawCountArray
		lea edi, [ecx]this.DetailTextureIsNearArray

		pxor xmm1, xmm1

		mov ecx, [ecx]this.m_csPolyCount

		poly_count_loop:
			mov eax, [edx]
			add edx, 4

			pxor xmm2, xmm2

			num_pts_loop:
				movss xmm3, [esi+8]
				add esi, TYPE FGLVertex

				pslld xmm2, 1

				cmpltss xmm3, xmm0
				psrld xmm3, 31

				por xmm2, xmm3

				dec eax
				jne num_pts_loop

			movd [edi], xmm2
			add edi, 4

			por xmm1, xmm2

			dec ecx
			jne poly_count_loop

		movd eax, xmm1

		pop edi
		pop esi

		ret 4
	}
}
#endif //UTGLR_INCLUDE_SSE_CODE

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
	SetDefaultAAState();
	//EndGouraudPolygonBufferingNoCheck sets its own projection state
	//Stream state set when start buffering
	//Default texture state set when start buffering

	clockFast(GouraudCycles);

	//Set projection state
	SetProjectionState(m_requestNearZRangeHackProjection);

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
	m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, m_curVertexBufferPos, m_bufferedVerts / 3);

	//Advance vertex buffer position
	m_curVertexBufferPos += m_bufferedVerts;

#ifdef UTGLR_DEBUG_ACTOR_WIREFRAME
	m_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
#endif

	unclockFast(GouraudCycles);
}

void UD3D9RenderDevice::EndTileBufferingNoCheck(void) {
	if (NoAATiles) {
		SetDisabledAAState();
	}
	else {
		SetDefaultAAState();
	}
	SetDefaultProjectionState();
	//Stream state set when start buffering
	//Default texture state set when start buffering

	clockFast(TileCycles);

	//Unlock vertexColor and texCoord0 buffers
	UnlockVertexColorBuffer();
	UnlockTexCoordBuffer(0);

	//Set view matrix
	/*D3DMATRIX d3dView = { +1.0f,  0.0f,  0.0f,  0.0f,
						   0.0f, -1.0f,  0.0f,  0.0f,
						   0.0f,  0.0f, -1.0f,  0.0f,
						   0.0f,  0.0f,  0.0f, +1.0f };
	m_d3dDevice->SetTransform(D3DTS_VIEW, &d3dView);*/

	//Draw the quads (stored as triangles)
	m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, m_curVertexBufferPos, m_bufferedVerts / 3);

	//Advance vertex buffer position
	m_curVertexBufferPos += m_bufferedVerts;

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
	m_d3dDevice->DrawPrimitive(D3DPT_LINELIST, m_curVertexBufferPos, m_bufferedVerts / 2);

	//Advance vertex buffer position
	m_curVertexBufferPos += m_bufferedVerts;
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
	m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, m_curVertexBufferPos, m_bufferedVerts / 3);

	//Advance vertex buffer position
	m_curVertexBufferPos += m_bufferedVerts;
}


// Static variables.
INT UD3D9RenderDevice::NumDevices = 0;
INT UD3D9RenderDevice::LockCount = 0;

HMODULE UD3D9RenderDevice::hModuleD3d9 = NULL;
LPDIRECT3DCREATE9 UD3D9RenderDevice::pDirect3DCreate9 = NULL;

bool UD3D9RenderDevice::g_gammaFirstTime = false;
bool UD3D9RenderDevice::g_haveOriginalGammaRamp = false;

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
