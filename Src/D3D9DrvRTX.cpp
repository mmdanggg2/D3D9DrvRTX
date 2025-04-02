/*=============================================================================
	D3D9Drv.cpp: Unreal D3D9 support precompiled header generator.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by Chris Dohnal

=============================================================================*/

// For TCHAR def
#include "Windows.h"

#include "D3D9DrvRTX.h"

/*-----------------------------------------------------------------------------
	Unreal package implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_PACKAGE(D3D9DrvRTX);

FName FNAME_D3D9DrvRTX(TEXT("D3D9DrvRTX"), FNAME_Intrinsic);
EName NAME_D3D9DrvRTX = (EName)FNAME_D3D9DrvRTX.GetIndex();

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
