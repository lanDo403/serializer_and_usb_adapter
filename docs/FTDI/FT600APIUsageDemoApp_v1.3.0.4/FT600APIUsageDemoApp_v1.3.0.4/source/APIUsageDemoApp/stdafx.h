// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>
#include <Windows.h>
#include "..\sources\FTD3XX_Test.h"
#include "..\sources\FTD3XX_Logger.h"


//
// Define when linking with static library
// Undefine when linking with dynamic library
//
#define FTD3XX_STATIC


//
// Include D3XX library
//
#include "..\sources\FTD3XXLibrary\FTD3XX.h"
#pragma comment(lib, "FTD3XX.lib")


#include <initguid.h>
// {D1E8FE6A-AB75-4D9E-97D2-06FA22C7736C} // D3XX
DEFINE_GUID(GUID_DEVINTERFACE_FOR_D3XX,
	0xd1e8fe6a, 0xab75, 0x4d9e, 0x97, 0xd2, 0x6, 0xfa, 0x22, 0xc7, 0x73, 0x6c);
// {219D0508-57A8-4ff5-97A1-BD86587C6C7E} // D2XX
DEFINE_GUID(GUID_DEVINTERFACE_FOR_D2XX,
    0x219d0508, 0x57a8, 0x4ff5, 0x97, 0xa1, 0xbd, 0x86, 0x58, 0x7c, 0x6c, 0x7e);

