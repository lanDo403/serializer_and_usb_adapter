/*
* FT600 API Usage Demo App
*
* Copyright (C) 2015 FTDI Chip
*
*/

#include "stdafx.h"





///////////////////////////////////////////////////////////////////////////////////
// Demonstrates configuring idle timeout values for usb selective suspend
// FT_Create                        Open a device
// FT_GetSuspendTimeout				Reads the current idle timeout value.
// FT_SetSuspendTimeout				Sets the new idle timeout value.
// FT_Close                         Close device handle
///////////////////////////////////////////////////////////////////////////////////
BOOL IdleTimeoutForSuspendTest()
{
	FT_STATUS ftStatus = FT_OK;
	FT_HANDLE ftHandle;
	ULONG CurrentTimeout = 0, OrgTimeout = 0;
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
	// Get the current idle timeout value
	// 
	ftStatus = FT_GetSuspendTimeout(ftHandle,
		&OrgTimeout);
	if (FT_FAILED(ftStatus))
	{
		CMD_LOG(_T("\t FT_GetSuspendTimeout failed\n"));
		bResult = FALSE;
		goto exit;
	}
	CMD_LOG(_T("\t Get idle timeout : %d \n"),
		OrgTimeout);
	//
	// Lets set a new timeout value for the endpoint. set 60 seconds.
	//
	ftStatus = FT_SetSuspendTimeout(ftHandle,
		60);
	if (FT_FAILED(ftStatus))
	{
		CMD_LOG(_T("\t FT_SetSuspendTimeout failed :  %d \n"), GetLastError());
		bResult = FALSE;
		goto exit;
	}

	//
	// Get the new idle timeout.
	// 
	ftStatus = FT_GetSuspendTimeout(ftHandle,
		&CurrentTimeout);
	if (FT_FAILED(ftStatus))
	{
		CMD_LOG(_T("\t FT_GetSuspendTimeout failed\n"));
		bResult = FALSE;
		goto exit;
	}
	CMD_LOG(_T("\t new idle timeout : %d \n"), CurrentTimeout);
	

exit:
	// Lets restore original values.
	//
	ftStatus = FT_SetSuspendTimeout(ftHandle,
		OrgTimeout);
	if (FT_FAILED(ftStatus))
	{
		CMD_LOG(_T("\t FT_SetSuspendTimeout failed to restore default values\n"));
		bResult = FALSE;
	}
	//
	// Close device handle
	//
	FT_Close(ftHandle);
	ftHandle = NULL;


	return bResult;
}

