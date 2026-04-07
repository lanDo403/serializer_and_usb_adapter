/*
* FT600 API Usage Demo App
*
* Copyright (C) 2015 FTDI Chip
*
*/

#include "stdafx.h"


#define LOOPBACK_DATA 4096





///////////////////////////////////////////////////////////////////////////////////
// Demonstrates configuring the timeout values for read endpoints.
// FT_Create                        Open a device
// FT_GetPipeTimeout		    Reads the timeout value assocaited with an endpoint.
// FT_SetPipeTimeout		    Sets a timeout value to an endpoint.
// FT_Close                         Close device handle
///////////////////////////////////////////////////////////////////////////////////
BOOL ReadTimeoutTest()
{
	FT_STATUS ftStatus = FT_OK;
	FT_HANDLE ftHandle;
	ULONG TimeoutInMs = 0, OrgTimeout = 0;
	ULONG BytesTransferred;
	LARGE_INTEGER StartingTime, EndingTime, Frequency;
	UCHAR ucPipeID = 0x82; /* BULK IN endpoint address*/
	UCHAR acReadBuf[LOOPBACK_DATA] = { 0 };
	BOOL bResult = TRUE;


	//
	// Open a device
	//
	ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
	if (FT_FAILED(ftStatus))
	{
		bResult = FALSE;
		return FALSE;
	}

	//
	// Get the timeout value associated with the endpoint.
	// 
	ftStatus = FT_GetPipeTimeout(ftHandle,
		ucPipeID,
		&OrgTimeout);
	if (FT_FAILED(ftStatus))
	{
		CMD_LOG(_T("\t FT_GetPipeTimeout failed\n"));
		bResult = FALSE;
		goto exit;
	}
	CMD_LOG(_T("\t Get timeout : %d (milliseconds)\n"),
		OrgTimeout);
	//
	// Lets set a new timeout value for the endpoint.
	//
	ftStatus = FT_SetPipeTimeout(ftHandle,
		ucPipeID,
		1000);
	if (FT_FAILED(ftStatus))
	{
		CMD_LOG(_T("\t FT_SetPipeTimeout failed\n"));
		bResult = FALSE;
		goto exit;
	}

	//
	// Get the new timeout value associated with the endpoint.
	// 
	ftStatus = FT_GetPipeTimeout(ftHandle, 
		ucPipeID, 
		&TimeoutInMs);
	if (FT_FAILED(ftStatus))
	{
		CMD_LOG(_T("\t FT_GetPipeTimeout failed\n"));
		bResult = FALSE;
		goto exit;
	}
	CMD_LOG(_T("\t Get timeout : %d (milliseconds)\n"), TimeoutInMs);
	//
	// Now lets do a FT_ReadPipe and see the behaviour on timeout.
	// When timeout is disabled, and FT_ReadPipe is done, if there is no data,
	// the call will hang.
	// Now lets try with above set timeout.
	// Taking the system ticks to see how long FT_ReadPipe was hanging.
	//
	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&StartingTime);

	ftStatus = FT_ReadPipe(ftHandle,
		ucPipeID,
		acReadBuf,
		LOOPBACK_DATA,
		&BytesTransferred,
		NULL);
	
	QueryPerformanceCounter(&EndingTime);

	CMD_LOG(_T("\t FT_ReadPipe return status %d BytesReturned: %d after %d MilliSeconds\n"),
		ftStatus,
		BytesTransferred,
		(((EndingTime.QuadPart - StartingTime.QuadPart) * 1000) / (Frequency.QuadPart)));

	if (FT_FAILED(ftStatus))
	{
		ftStatus = FT_AbortPipe(ftHandle, ucPipeID);
		if (FT_FAILED(ftStatus))
		{
			CMD_LOG(_T("\t FT_AbortPipe failed\n"));
			bResult = FALSE;
			goto exit;
		}
	}
	
exit:
	//
	// Lets restore the original timeout value
	//
	ftStatus = FT_SetPipeTimeout(ftHandle,
		ucPipeID,
		OrgTimeout);
	if (FT_FAILED(ftStatus))
	{
		CMD_LOG(_T("\t FT_SetPipeTimeout failed\n"));
		bResult = FALSE;
	}

	//
	// Close device handle
	//
	FT_Close(ftHandle);
	ftHandle = NULL;


	return bResult;
}

