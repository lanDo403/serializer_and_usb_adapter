/*
 * FT600 API Usage Demo App
 *
 * Copyright (C) 2016 FTDI Chip
 *
 */

#include "stdafx.h"



#define FTDI_VID                        0x0403
#define FTDI_PID_600                    0x601E
#define FTDI_PID_601                    0x601F
#define FTDI_CURRENT_FIRMWARE_VERSION   0x0102

BOOL ValidateDevice(FT_HANDLE ftHandle);




///////////////////////////////////////////////////////////////////////////////////
// Demonstrates opening/verifying/closing of a device handle:
// FT_ListDevices			Get the number of devices connected
// FT_Create				Open a device handle
// FT_GetVIDPID				Get device VID and PID
// FT_GetFirmwareVersion	Send control transfer to control pipe
// FT_Close					Close device handle
///////////////////////////////////////////////////////////////////////////////////
BOOL OpenCloseTest()
{
    FT_STATUS ftStatus = FT_OK;
    FT_HANDLE ftHandle = NULL;
 
	DWORD dwNumDevicesConnected = 0;


	//
	// Display information of devices connected
	//
	DisplayDeviceInfoList();


	//
	// Get the number of devices connected
	//
	dwNumDevicesConnected = GetNumberOfDevicesConnected();
	if (dwNumDevicesConnected == 0)
	{
		CMD_LOG(_T("\n\tNo device is connected to the machine! Please connect a device.\n"));
		return FALSE;
	}
	else if (dwNumDevicesConnected > 1)
	{
		CMD_LOG(_T("\n\t%d devices are connected to the machine! Please ensure only 1 device is connected.\n"), dwNumDevicesConnected);
		return FALSE;
	}


    //
    // Open a device by SERIAL NUMBER
	// Opening by SERIAL NUMBER supports multiple device scenarios
	// because customers are expected to provide unique SERIAL NUMBER for each item of their product
    //
	{
		CMD_LOG(_T("\n"));
		CMD_LOG(_T("\tOpen by SERIAL NUMBER.\n"));

		char SerialNumber[16] = { 0 };
		if (!GetDeviceSerialNumber(0, SerialNumber))
		{
			return FALSE;
		}

		ftStatus = FT_Create(SerialNumber, FT_OPEN_BY_SERIAL_NUMBER, &ftHandle);
		if (FT_FAILED(ftStatus))
		{
			return FALSE;
		}
		if (!ValidateDevice(ftHandle))
		{
			FT_Close(ftHandle);
			return FALSE;
		}
		FT_Close(ftHandle);
	}

	//
	// Open a device by DESCRIPTION
	// Opening by DESCRIPTION supports multiple device scenarios
	// because customers are expected to provide unique DESCRIPTION for each product
	//
	{
		CMD_LOG(_T("\n"));
		CMD_LOG(_T("\tOpen by DESCRIPTION.\n"));

		char Description[32] = { 0 };
		if (!GetDeviceDescription(0, Description))
		{
			return FALSE;
		}

		ftStatus = FT_Create(Description, FT_OPEN_BY_DESCRIPTION, &ftHandle);
		if (FT_FAILED(ftStatus))
		{
			return FALSE;
		}
		if (!ValidateDevice(ftHandle))
		{
			FT_Close(ftHandle);
			return FALSE;
		}
		FT_Close(ftHandle);
	}

	//
	// Open a device by INDEX
	// Opening by INDEX should only be used if devices connected to the machine has similar SERIAL NUMBER and DESCRIPTON.
	//
	{
		CMD_LOG(_T("\n"));
		CMD_LOG(_T("\tOpen by INDEX.\n"));

		ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
		if (FT_FAILED(ftStatus))
		{
			return FALSE;
		}
		if (!ValidateDevice(ftHandle))
		{
			FT_Close(ftHandle);
			return FALSE;
		}
		FT_Close(ftHandle);
	}

	//
	// Open a device by GUID
	// Opening by GUID will always open the first device detected (not necessarily the first device connected)
	// It is similar to opening by INDEX 0.
	//
	{
		CMD_LOG(_T("\n"));
		CMD_LOG(_T("\tOpen by GUID.\n"));

		GUID DeviceGUID[2] = { 0 };
		memcpy(&DeviceGUID[0], &GUID_DEVINTERFACE_FOR_D3XX, sizeof(GUID));

		ftStatus = FT_Create(&DeviceGUID[0], FT_OPEN_BY_GUID, &ftHandle);
		if (FT_FAILED(ftStatus))
		{
			return FALSE;
		}
		if (!ValidateDevice(ftHandle))
		{
			FT_Close(ftHandle);
			return FALSE;
		}
		FT_Close(ftHandle);
	}


    return TRUE;
}


///////////////////////////////////////////////////////////////////////////////////
// Gets the number of devices connected to the machine using FT_ListDevices
///////////////////////////////////////////////////////////////////////////////////
DWORD GetNumberOfDevicesConnected()
{
	FT_STATUS ftStatus = FT_OK;
	DWORD dwNumDevices = 0;

	ftStatus = FT_ListDevices(&dwNumDevices, NULL, FT_LIST_NUMBER_ONLY);
	if (FT_FAILED(ftStatus))
	{
		return 0;
	}

	return dwNumDevices;
}

///////////////////////////////////////////////////////////////////////////////////
// Gets the serial number of the specified device index using FT_ListDevices
///////////////////////////////////////////////////////////////////////////////////
BOOL GetDeviceSerialNumber(DWORD dwDeviceIndex, char* SerialNumber)
{
	FT_STATUS ftStatus = FT_OK;

	ftStatus = FT_ListDevices((PVOID)dwDeviceIndex, SerialNumber, FT_LIST_BY_INDEX | FT_OPEN_BY_SERIAL_NUMBER);
	if (FT_FAILED(ftStatus))
	{
		return FALSE;
	}

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////
// Gets the device description of the specified device index using FT_ListDevices
///////////////////////////////////////////////////////////////////////////////////
BOOL GetDeviceDescription(DWORD dwDeviceIndex, char* ProductDescription)
{
	FT_STATUS ftStatus = FT_OK;

	ftStatus = FT_ListDevices((PVOID)dwDeviceIndex, ProductDescription, FT_LIST_BY_INDEX | FT_OPEN_BY_DESCRIPTION);
	if (FT_FAILED(ftStatus))
	{
		return FALSE;
	}

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////
// Gets the serial number of all devices connected using FT_ListDevices
///////////////////////////////////////////////////////////////////////////////////
BOOL GetDevicesSerialNumbers(char** SerialNumbers)
{
	FT_STATUS ftStatus = FT_OK;
	DWORD dwNumDevices = 0;

	ftStatus = FT_ListDevices(SerialNumbers, &dwNumDevices, FT_LIST_ALL | FT_OPEN_BY_SERIAL_NUMBER);
	if (FT_FAILED(ftStatus))
	{
		return FALSE;
	}

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////
// Gets the device description of all devices connected using FT_ListDevices
///////////////////////////////////////////////////////////////////////////////////
BOOL GetDevicesProductDescriptions(char** ProductDescriptions)
{
	FT_STATUS ftStatus = FT_OK;
	DWORD dwNumDevices = 0;

	ftStatus = FT_ListDevices(ProductDescriptions, &dwNumDevices, FT_LIST_ALL | FT_OPEN_BY_DESCRIPTION);
	if (FT_FAILED(ftStatus))
	{
		return FALSE;
	}

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////
// Gets the information of all devices connected using FT_CreateDeviceInfoList and FT_GetDeviceInfoList
///////////////////////////////////////////////////////////////////////////////////
DWORD GetDevicesInfoList(FT_DEVICE_LIST_INFO_NODE **pptDevicesInfo)
{
	FT_STATUS ftStatus = FT_OK;
	DWORD dwNumDevices = 0;

	ftStatus = FT_CreateDeviceInfoList(&dwNumDevices);
	if (FT_FAILED(ftStatus))
	{
		return 0;
	}

	*pptDevicesInfo = (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE) * dwNumDevices);
	if (!(*pptDevicesInfo))
	{
		return 0;
	}

	ftStatus = FT_GetDeviceInfoList(*pptDevicesInfo, &dwNumDevices);
	if (FT_FAILED(ftStatus))
	{
		free(*pptDevicesInfo);
		*pptDevicesInfo = NULL;
		return 0;
	}

	return dwNumDevices;
}

///////////////////////////////////////////////////////////////////////////////////
// Gets the information of a specific device connected using FT_CreateDeviceInfoList and FT_GetDeviceInfoDetail
///////////////////////////////////////////////////////////////////////////////////
DWORD GetDeviceInfoDetail(DWORD dwDeviceIndex, DWORD* Flags, DWORD* Type, DWORD *ID, char* SerialNumber, char* Description, FT_HANDLE* Handle)
{
	FT_STATUS ftStatus = FT_OK;
	DWORD dwNumDevices = 0;

	ftStatus = FT_CreateDeviceInfoList(&dwNumDevices);
	if (FT_FAILED(ftStatus))
	{
		return 0;
	}

	if (dwDeviceIndex >= dwNumDevices)
	{
		return 0;
	}

	ftStatus = FT_GetDeviceInfoDetail(dwDeviceIndex, Flags, Type, ID, NULL, SerialNumber, Description, Handle);
	if (FT_FAILED(ftStatus))
	{
		return 0;
	}

	return dwNumDevices;
}

VOID DisplayDeviceInfoDetail(DWORD dwNumDevices)
{
	for (DWORD i = 0; i < dwNumDevices; i++)
	{
		DWORD Flags = 0;
		DWORD Type = 0;
		DWORD ID = 0;
		char SerialNumber[16] = { 0 };
		char Description[32] = { 0 };
		FT_HANDLE Handle = NULL;

		dwNumDevices = GetDeviceInfoDetail(i, &Flags, &Type, &ID, SerialNumber, Description, &Handle);
		if (dwNumDevices == 0)
		{
			return;
		}

		printf("\n\tFT_GetDeviceInfoDetail:\n\tDevice[%d]\n", i);
		printf("\t\tFlags: 0x%x [%s]\n", Flags, 
			Flags & FT_FLAGS_SUPERSPEED ? "USB 3" : 
			Flags & FT_FLAGS_HISPEED ? "USB 2" : "");
		printf("\t\tType: %d\n", Type);
		printf("\t\tID: 0x%08X\n", ID);
		if (Flags & FT_FLAGS_OPENED)
		{
			printf("\t\tftHandle: 0x%x [Device is being used by another process]\n", Handle);
		}
		printf("\t\tSerialNumber: %s\n", SerialNumber);
		printf("\t\tDescription: %s\n", Description);
	}
}

VOID DisplayDeviceInfoList()
{
	FT_DEVICE_LIST_INFO_NODE *ptDeviceInfo = NULL;
	DWORD dwNumDevices = 0;

	dwNumDevices = GetDevicesInfoList(&ptDeviceInfo);
	if (dwNumDevices == 0)
	{
		return;
	}

	for (DWORD i = 0; i < dwNumDevices; i++)
	{
		printf("\n\tFT_GetDeviceInfoList:\n\tDevice[%d]\n", i);
		printf("\t\tFlags: 0x%x [%s]\n", ptDeviceInfo[i].Flags,
			ptDeviceInfo[i].Flags & FT_FLAGS_SUPERSPEED ? "USB 3" : 
			ptDeviceInfo[i].Flags & FT_FLAGS_HISPEED ? "USB 2" : "");
		printf("\t\tType: %d\n", ptDeviceInfo[i].Type);
		printf("\t\tID: 0x%08X\n", ptDeviceInfo[i].ID);
		if (ptDeviceInfo[i].Flags & FT_FLAGS_OPENED)
		{
			printf("\t\tftHandle: 0x%x [Device is being used by another process]\n", ptDeviceInfo[i].ftHandle);
		}
		printf("\t\tSerialNumber: %s\n", ptDeviceInfo[i].SerialNumber);
		printf("\t\tDescription: %s\n", ptDeviceInfo[i].Description);
	}
}

VOID DisplaySerialNumbersOfDevicesConnected(DWORD dwNumDevices)
{
	// Method 1
	{
		char SerialNumber[16] = { 0 }; // SerialNumber is 15 bytes maximum, 1 byte for terminator

		for (DWORD dwDeviceIndex = 0; dwDeviceIndex < dwNumDevices; dwDeviceIndex++)
		{
			memset(SerialNumber, 0, sizeof(SerialNumber));
			if (!GetDeviceSerialNumber(dwDeviceIndex, SerialNumber))
			{
				return;
			}

			printf("\nFT_ListDevices: SerialNumber[%d]=%s\n", dwDeviceIndex, SerialNumber);
		}
	}

	// Method 2
	{
		char *SerialNumber[2 + 1] = { 0 }; // pointer to array of 3 pointers 
		char SerialNumber1[16] = { 0 }; // buffer for serial number of first device 
		char SerialNumber2[16] = { 0 }; // buffer for serial number of second device 

		// initialize the array of pointers 
		SerialNumber[0] = SerialNumber1;
		SerialNumber[1] = SerialNumber2;
		SerialNumber[2] = NULL; // last entry should be NULL

		if (!GetDevicesSerialNumbers(SerialNumber))
		{
			return;
		}

		printf("\nFT_ListDevices:\n");
		for (DWORD i = 0; i < dwNumDevices; i++)
		{
			printf("\tDevice[%d]\n\tSerialNumber:%s\n", i, SerialNumber[i]);
		}
	}
}

VOID DisplayNumberOfDevicesConnected()
{
	printf("\nFT_ListDevices: DevicesConnected=%d\n", GetNumberOfDevicesConnected());
}

VOID DisplayProductDescriptionOfDevicesConnected(DWORD dwNumDevices)
{
	// Method 1
	{
		char ProductDescription[32] = { 0 }; // ProductDescription is 31 bytes maximum, 1 byte for terminator

		for (DWORD dwDeviceIndex = 0; dwDeviceIndex < dwNumDevices; dwDeviceIndex++)
		{
			memset(ProductDescription, 0, sizeof(ProductDescription));
			if (!GetDeviceDescription(dwDeviceIndex, ProductDescription))
			{
				return;
			}

			printf("\nFT_ListDevices: ProductDescription[%d]=%s\n", dwDeviceIndex, ProductDescription);
		}
	}

	// Method 2
	{
		char *ProductDescription[2 + 1] = { 0 }; // pointer to array of 3 pointers 
		char ProductDescription1[32] = { 0 }; // buffer for description of first device 
		char ProductDescription2[32] = { 0 }; // buffer for description of second device 

		// initialize the array of pointers 
		ProductDescription[0] = ProductDescription1;
		ProductDescription[1] = ProductDescription2;
		ProductDescription[2] = NULL; // last entry should be NULL

		if (!GetDevicesProductDescriptions(ProductDescription))
		{
			return;
		}

		printf("\nFT_ListDevices:\n");
		for (DWORD i = 0; i < dwNumDevices; i++)
		{
			printf("\tDevice[%d]\n\tProductDescription:%s\n", i, ProductDescription[i]);
		}
	}
}

BOOL ValidateDevice(FT_HANDLE ftHandle)
{
	FT_STATUS ftStatus = FT_OK;
	USHORT uwVID = 0;
	USHORT uwPID = 0;


	//
	// Get and check device VID and PID
	// The default PID of FT60X is 0x0403
	// The default VID of FT60X is 0x601F if 32-bit, 0x601E if 16-bit
	//
	ftStatus = FT_GetVIDPID(ftHandle, &uwVID, &uwPID);
	if (FT_FAILED(ftStatus))
	{
		return FALSE;
	}
	CMD_LOG(_T("\t\tVendor ID: 0x%04X\n"), uwVID);
	CMD_LOG(_T("\t\tProduct ID: 0x%04X\n"), uwPID);


	//
	// Get firmware version
	// The current firmware version is 0x0102
	//
	ULONG ulFirmwareVersion = 0;
	ftStatus = FT_GetFirmwareVersion(ftHandle, &ulFirmwareVersion);
	if (FT_FAILED(ftStatus))
	{
		return FALSE;
	}
	CMD_LOG(_T("\t\tFirmware version: 0x%04x\n"), ulFirmwareVersion);
	if (ulFirmwareVersion != FTDI_CURRENT_FIRMWARE_VERSION)
	{
		CMD_LOG(_T("\t\tWARNING! Expected firmware version: 0x%04x\n"), FTDI_CURRENT_FIRMWARE_VERSION);
	}


	//
	// Get driver version
	//
	ULONG ulDriverVersion = 0;
	ftStatus = FT_GetDriverVersion(ftHandle, &ulDriverVersion);
	if (FT_FAILED(ftStatus))
	{
		return FALSE;
	}
	CMD_LOG(_T("\t\tDriver version: 0x%04x\n"), ulDriverVersion);


	//
	// Get library version
	//
	ULONG ulLibraryVersion = 0;
	ftStatus = FT_GetLibraryVersion(&ulLibraryVersion);
	if (FT_FAILED(ftStatus))
	{
		return FALSE;
	}
	CMD_LOG(_T("\t\tLibrary version: 0x%04x\n"), ulLibraryVersion);


	return TRUE;
}



