/*
* FT600 API Usage Demo App
*
* Copyright (C) 2015 FTDI Chip
*
*/

#include "stdafx.h"



///////////////////////////////////////////////////////////////////////////////////
// Demonstrates configuring the timeout values for read endpoints.
// FT_Create                        Open a device
// FT_EnableGPIO					Configures GPIO.
// FT_ReadGPIO						Gets GPIO pin status
// FT_WriteGPIO						Sets GPIO pins to low or high.
// FT_Close                         Close device handle
///////////////////////////////////////////////////////////////////////////////////
BOOL GPIOTest()
{
	FT_STATUS ftStatus = FT_OK;
	FT_HANDLE ftHandle;
	BOOL bResult = TRUE;
	UINT32 u32Data = 0;

	//
	// Open a device
	//
	ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
	if (FT_FAILED(ftStatus))
	{
		bResult = FALSE;
		return FALSE;
	}

	// Get GPIO status
	ftStatus = FT_ReadGPIO(ftHandle, &u32Data);
	if (FT_FAILED(ftStatus))
	{
		CMD_LOG(_T("\t FT_ReadGPIO failed\n"));
		bResult = FALSE;
		goto exit;
	}

	CMD_LOG(_T("\t Initial GPIO bitmap : %d\n"), u32Data);

	CMD_LOG(_T("\t moving both the GPIOs to Output mode\n"));
	ftStatus = FT_EnableGPIO(ftHandle, 0x3, 0x3); //bit 0 and 1 both set.
	if (FT_FAILED(ftStatus))
	{
		CMD_LOG(_T("\t FT_EnableGPIO failed\n"));
		bResult = FALSE;
		goto exit;
	}

	// Get GPIO status
	ftStatus = FT_ReadGPIO(ftHandle, &u32Data);
	if (FT_FAILED(ftStatus))
	{
		CMD_LOG(_T("\t FT_ReadGPIO failed\n"));
		bResult = FALSE;
		goto exit;
	}
	CMD_LOG(_T("\t GPIO bitmap after EnableGPIO : %d\n"), u32Data);
#if 0
	ftStatus = FT_SetGPIOPull(ftHandle, 0x3, 0x3);
	if (FT_FAILED(ftStatus))
	{
		CMD_LOG(_T("\t FT_SetGPIOPull failed\n"));
		bResult = FALSE;
		goto exit;
	}
#endif
	CMD_LOG(_T("\t Making both the GPIO high\n"));
	// set both the GPIOs to high.
	ftStatus = FT_WriteGPIO(ftHandle, 0x3, 0x3);
	if (FT_FAILED(ftStatus))
	{
		CMD_LOG(_T("\t FT_WriteGPIO failed\n"));
		bResult = FALSE;
		goto exit;
	}

	// Get GPIO status
	ftStatus = FT_ReadGPIO(ftHandle, &u32Data);
	if (FT_FAILED(ftStatus))
	{
		CMD_LOG(_T("\t FT_ReadGPIO failed\n"));
		bResult = FALSE;
		goto exit;
	}
	CMD_LOG(_T("\t GPIO bitmap after FT_WriteGPIO : %d\n"), u32Data);

exit:
	//
	// Lets restore the original GPIO states. 
	//
	ftStatus = FT_EnableGPIO(ftHandle, 0x3, 0x3);
	if (FT_FAILED(ftStatus))
	{
		CMD_LOG(_T("\t%s FAILED! FT_EnableGPIO"), _T(__FUNCTION__));
		return FALSE;
	}

	/* Set the pins to LOW */
	ftStatus = FT_WriteGPIO(ftHandle,
		0x3, /* mask - GPIO_0 and GPIO_1 are being written*/
		0); /* value for GPIO_0 and GPIO_1 */
	if (FT_FAILED(ftStatus))
	{
		CMD_LOG(_T("\t%s FAILED! FT_WriteGPIO"), _T(__FUNCTION__));
		return FALSE;
	}


	//
	// Close device handle
	//
	FT_Close(ftHandle);
	ftHandle = NULL;


	return bResult;
}

