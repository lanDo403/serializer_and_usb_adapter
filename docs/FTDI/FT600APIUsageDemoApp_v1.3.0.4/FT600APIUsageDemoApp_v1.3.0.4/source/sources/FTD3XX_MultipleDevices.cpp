/*
* FT600 API Usage Demo App
*
* Copyright (C) 2015 FTDI Chip
*
*/

#include "stdafx.h"
#include <process.h> // for _beginthreadex()



#define FTDI_VID                        0x0403
#define FTDI_PID_600                    0x601E
#define FTDI_PID_601                    0x601F
#define FTDI_CURRENT_FIRMWARE_VERSION   0x0102
#define NUM_DEVICES                     2
#define BUFFER_SIZE                     16777216 // 16MB
#define NUM_ITERATIONS                  10



typedef struct _THREADPARAM
{
	FT_HANDLE ftHandle;
	ULONG ulBufSize;
	ULONG ulIterations;
	UCHAR iDevice;
	BOOL bStatus;

} THREADPARAM, *PTHREADPARAM;



static UINT DoLoopbackTest(VOID* a_pvThreadParam);


///////////////////////////////////////////////////////////////////////////////////
// Demonstrates opening multiple devices
// FT_Create            Open a device handle by INDEX
// FT_Close             Close device handle
///////////////////////////////////////////////////////////////////////////////////
BOOL OpenMultipleDevicesTest()
{
	FT_STATUS ftStatus = FT_OK;
	FT_HANDLE ftHandle[NUM_DEVICES] = { NULL };
	USHORT uwVID = 0;
	USHORT uwPID = 0;
	ULONG index = 0;
	BOOL bResult = TRUE;


	//
	// Check if 2 devices are connected
	//
	DWORD dwNumDevicesConnected = 0;
	dwNumDevicesConnected = GetNumberOfDevicesConnected();
	if (dwNumDevicesConnected != NUM_DEVICES)
	{
		CMD_LOG(_T("Please connect 2 devices to the machine\n"));
		return FALSE;
	}


	//
	// Display serial numbers and product descriptions of both devices connected
	//
	DisplayNumberOfDevicesConnected();
	DisplaySerialNumbersOfDevicesConnected(dwNumDevicesConnected);
	DisplayProductDescriptionOfDevicesConnected(dwNumDevicesConnected);
	DisplayDeviceInfoList();
	DisplayDeviceInfoDetail(dwNumDevicesConnected);


	//
	// Open a device handle for 2 devices connected
	//
	// Open FT_OPEN_BY_SERIAL_NUMBER
	//   Customers are expected to change the serial number of each of its devices
	//   This will distinguish one of their ft60x devices to their other ft60x devices
	// Open FT_OPEN_BY_PRODUCT_DESCRIPTION
	//   Customers are expected to change the product description of each of its devices
	//   This will distinguish their ft60x devices to other ft60x devices of other companies
	//

	for (int i = 0; i < sizeof(ftHandle) / sizeof(ftHandle[0]); i++)
	{
		printf("\nFT_Create\n");
#if 1
		char SerialNumber[16] = { 0 };
		if (!GetDeviceSerialNumber(i, SerialNumber))
		{
			return FALSE;
		}
		printf("\tDevice[%d]\n\tSerialNumber: %s\n", i, SerialNumber);

		ftStatus = FT_Create(SerialNumber, FT_OPEN_BY_SERIAL_NUMBER, &ftHandle[i]);
#else
		char Description[32] = { 0 };
		if (!GetDeviceDescription(i, Description))
		{
			return FALSE;
		}
		printf("\n\tDevice[%d] Description: %s\n", i, Description);

		ftStatus = FT_Create(Description, FT_OPEN_BY_DESCRIPTION, &ftHandle[i]);
#endif
		if (FT_FAILED(ftStatus))
		{
			bResult = FALSE;
			goto exit;
		}
		printf("\tHandle: 0x%X\n", ftHandle[i]);

		ftStatus = FT_GetVIDPID(ftHandle[i], &uwVID, &uwPID);
		if (FT_FAILED(ftStatus))
		{
			bResult = FALSE;
			goto exit;
		}
		printf("\tVendor ID: 0x%04X\n\tProduct ID: 0x%04X\n", uwVID, uwPID);

		if (uwVID != FTDI_VID ||
			(uwPID != FTDI_PID_600 && uwPID != FTDI_PID_601))
		{
			bResult = FALSE;
			goto exit;
		}
	}


	//
	// Do nothing, just sleep
	//
	for (int i = 0; i < 3; i++)
	{
		Sleep(1000);
	}


exit:

	for (int i = 0; i < sizeof(ftHandle) / sizeof(ftHandle[0]); i++)
	{
		if (ftHandle[i])
		{
			FT_Close(ftHandle[i]);
		}
	}

	return bResult;
}


///////////////////////////////////////////////////////////////////////////////////
// Demonstrates simultaneous loopback transfers on multiple devices
// FT_Create            Open a device handle by INDEX
// FT_Close             Close device handle
///////////////////////////////////////////////////////////////////////////////////
typedef unsigned(__stdcall *PTHREAD_START) (void *);
BOOL SimultaneousLoopbackOnMultipleDevicesTest()
{
	FT_STATUS ftStatus = FT_OK;
	FT_HANDLE ftHandle[NUM_DEVICES] = { NULL };
	HANDLE hThread[NUM_DEVICES] = { NULL };
	THREADPARAM oThreadParam[NUM_DEVICES] = { 0 };
	USHORT uwVID = 0;
	USHORT uwPID = 0;
	ULONG index = 0;
	BOOL bResult = TRUE;


	//
	// Open a device handle for 2 devices connected
	//
	// Open FT_OPEN_BY_INDEX
	//   If there are two FT60X devices connected to the machine, user should use this.
	//   The parameter is a pointer to a ULONG which specifies the device to open
	//

	for (int i = 0; i < NUM_DEVICES; i++)
	{
		printf("\nFT_Create\n");
#if 1
		char SerialNumber[16] = { 0 };
		if (!GetDeviceSerialNumber(i, SerialNumber))
		{
			return FALSE;
		}
		printf("\tDevice[%d]\n\tSerialNumber: %s\n", i, SerialNumber);

		ftStatus = FT_Create(SerialNumber, FT_OPEN_BY_SERIAL_NUMBER, &ftHandle[i]);
#else
		char Description[32] = { 0 };
		if (!GetDeviceDescription(i, Description))
		{
			return FALSE;
		}
		printf("\n\tDevice[%d] Description: %s\n", i, Description);

		ftStatus = FT_Create(Description, FT_OPEN_BY_DESCRIPTION, &ftHandle[i]);
#endif
		if (FT_FAILED(ftStatus))
		{
			bResult = FALSE;
			goto exit;
		}
		printf("\tHandle: 0x%X\n", ftHandle[i]);

		ftStatus = FT_GetVIDPID(ftHandle[i], &uwVID, &uwPID);
		if (FT_FAILED(ftStatus))
		{
			bResult = FALSE;
			goto exit;
		}
		printf("\tVendor ID: 0x%04X\n\tProduct ID: 0x%04X\n", uwVID, uwPID);

		if (uwVID != FTDI_VID ||
			(uwPID != FTDI_PID_600 && uwPID != FTDI_PID_601))
		{
			bResult = FALSE;
			goto exit;
		}
	}


	//
	// Create threads for each device
	// Separate the loops so that multithreading will be more evident
	//
	for (int i = 0; i < NUM_DEVICES; i++)
	{
		oThreadParam[i].ftHandle = ftHandle[i];
		oThreadParam[i].iDevice = i;
		oThreadParam[i].bStatus = TRUE;
		oThreadParam[i].ulBufSize = BUFFER_SIZE;
		oThreadParam[i].ulIterations = NUM_ITERATIONS;
	}
	for (int i = 0; i < NUM_DEVICES; i++)
	{
		hThread[i] = (HANDLE)::_beginthreadex(NULL, 0, (PTHREAD_START)DoLoopbackTest, &oThreadParam[i], 0, NULL);
	}


	//
	// Wait for all threads to finish and check the status of each thread
	//
	WaitForMultipleObjects(NUM_DEVICES, hThread, TRUE, INFINITE);
	for (UCHAR i = 0; i< NUM_DEVICES; i++)
	{
		if (!oThreadParam[i].bStatus)
		{
			bResult = FALSE;
		}
	}


exit:

	for (int i = 0; i < NUM_DEVICES; i++)
	{
		if (ftHandle[i])
		{
			FT_Close(ftHandle[i]);
		}
	}

	return bResult;
}



///////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////
UINT DoLoopbackTest(VOID* a_pvThreadParam)
{
	FT_STATUS ftStatus = FT_OK;
	THREADPARAM* pParam = (PTHREADPARAM)a_pvThreadParam;
	UCHAR* acWriteBuf = new UCHAR[pParam->ulBufSize];
	UCHAR* acReadBuf = new UCHAR[pParam->ulBufSize];
	OVERLAPPED vOverlappedWrite = { 0 };
	OVERLAPPED vOverlappedRead = { 0 };
	BOOL bResult = TRUE;


	//
	// Create the overlapped io event for asynchronous transfers
	//

	ftStatus = FT_InitializeOverlapped(pParam->ftHandle, &vOverlappedWrite);
	if (FT_FAILED(ftStatus))
	{
		bResult = FALSE;
		goto exit;
	}

	ftStatus = FT_InitializeOverlapped(pParam->ftHandle, &vOverlappedRead);
	if (FT_FAILED(ftStatus))
	{
		bResult = FALSE;
		goto exit;
	}


	//
	// Write and read loopback transfer
	//

	for (DWORD i = 0; i<pParam->ulIterations; i++)
	{
		//
		// Write to channel 1 ep 0x02
		//
		memset(acWriteBuf, 0xAA+i, pParam->ulBufSize);
		ULONG ulBytesWritten = 0;
		CMD_LOG(_T("\n\tDevice[%d] Writing %d bytes!\n"), pParam->iDevice, pParam->ulBufSize);
		ftStatus = FT_WritePipe(pParam->ftHandle, 0x02, acWriteBuf, pParam->ulBufSize, &ulBytesWritten, &vOverlappedWrite);
		if (ftStatus != FT_IO_PENDING)
		{
			bResult = FALSE;
			goto exit;
		}

		//
		// Read from channel 1 ep 0x82
		//
		memset(acReadBuf, 0x55+i, pParam->ulBufSize);
		ULONG ulBytesRead = 0;
		CMD_LOG(_T("\n\tDevice[%d] Reading %d bytes!\n"), pParam->iDevice, pParam->ulBufSize);
		ftStatus = FT_ReadPipe(pParam->ftHandle, 0x82, acReadBuf, pParam->ulBufSize, &ulBytesRead, &vOverlappedRead);
		if (ftStatus != FT_IO_PENDING)
		{
			bResult = FALSE;
			goto exit;
		}

		//
		// Wait for write to finish
		//
		ftStatus = FT_GetOverlappedResult(pParam->ftHandle, &vOverlappedWrite, &ulBytesWritten, TRUE);
		if (FT_FAILED(ftStatus))
		{
			bResult = FALSE;
			goto exit;
		}
		if (ulBytesWritten != pParam->ulBufSize)
		{
			bResult = FALSE;
			goto exit;
		}
		CMD_LOG(_T("\n\tDevice[%d] Writing %d bytes DONE!\n"), pParam->iDevice, pParam->ulBufSize);

		//
		// Wait for read to finish
		//
		ftStatus = FT_GetOverlappedResult(pParam->ftHandle, &vOverlappedRead, &ulBytesRead, TRUE);
		if (FT_FAILED(ftStatus))
		{
			bResult = FALSE;
			goto exit;
		}
		if (ulBytesRead != pParam->ulBufSize)
		{
			bResult = FALSE;
			goto exit;
		}
		CMD_LOG(_T("\n\tDevice[%d] Reading %d bytes DONE!\n"), pParam->iDevice, pParam->ulBufSize);

        //
        // Verify if data written is same as data read
        //
        if (memcmp(acWriteBuf, acReadBuf, ulBytesRead))
        {
            bResult = FALSE;
            goto exit;
        }
	}


exit:

	//
	// Destroy the overlapped io events
	//
	FT_ReleaseOverlapped(pParam->ftHandle, &vOverlappedWrite);
	FT_ReleaseOverlapped(pParam->ftHandle, &vOverlappedRead);

	pParam->bStatus = bResult;
	delete[] acWriteBuf;
	delete[] acReadBuf;
	return 0;
}


