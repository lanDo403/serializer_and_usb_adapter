/*
 * FT600 Data Streamer Demo App
 *
 * Copyright (C) 2016 FTDI Chip
 *
 */

#pragma once
#include <initguid.h>



#define DEVICE_VID							_T("0403")
#define DEFAULT_VALUE_OPENBY_DESC			"FTDI SuperSpeed-FIFO Bridge"
#define DEFAULT_VALUE_OPENBY_SERIAL			"000000000001"
#define DEFAULT_VALUE_OPENBY_INDEX			"0"

// {D1E8FE6A-AB75-4D9E-97D2-06FA22C7736C} // D3XX
DEFINE_GUID(GUID_DEVINTERFACE_FOR_D3XX,
	0xd1e8fe6a, 0xab75, 0x4d9e, 0x97, 0xd2, 0x6, 0xfa, 0x22, 0xc7, 0x73, 0x6c);

#define FT600_VID   0x0403
#define FT600_PID   0x601E
#define FT601_PID   0x601F

