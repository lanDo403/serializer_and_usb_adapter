/*
 * FT600 API Usage Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#include "stdafx.h"

static BOOL EnableBatteryChargingDetection();

///////////////////////////////////////////////////////////////////////////////////
// Demonstrates querying of battery charging detection status
// FT_Create                    Open a device
// FT_GetChipConfiguration      Read configuration from chip's flash memory
// FT_SetChipConfiguration      Write configuration to chip's flash memory
// FT_GetGPIO                   Get the status of GPIO pins
// FT_Close                     Close device handle
///////////////////////////////////////////////////////////////////////////////////
BOOL BatteryChargingDetectionTest()
{
    FT_STATUS ftStatus = FT_OK;
    FT_HANDLE ftHandle = NULL;
	UINT32 u32GPIO_data = 0;
	BOOL bStatus = TRUE;


    //
    // Enable battery charging detection
    //
    if (!EnableBatteryChargingDetection())
    {
        return FALSE;
    }

	//
	// Open a device
	//
	ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
    if (FT_FAILED(ftStatus))
    {
        return FALSE;
    }

    
    //
    // Get GPIO status
    //
    
	ftStatus = FT_ReadGPIO(ftHandle, &u32GPIO_data);
    if (FT_FAILED(ftStatus))
    {
        bStatus = FALSE;
        goto exit;
    }   
    

    //
    // Check the GPIO status
    // BatteryChargingGPIOConfig Default setting : 11100100b  (0xE4)
    // 7 - 6 : DCP = 11b         (GPIO1 = 1 GPIO0 = 1)
    // 5 - 4 : CDP = 10b         (GPIO1 = 1 GPIO0 = 0)
    // 3 - 2 : SDP = 01b         (GPIO1 = 0 GPIO0 = 1)
    // 1 - 0 : Unknown/Off = 00b (GPIO1 = 0 GPIO0 = 0)
    // Since device is connected to a host machine, then we should get SDP (GPIO1 = 0 GPIO0 = 1)
	UCHAR ucBatteryChargingType = (UCHAR)u32GPIO_data;
    switch(ucBatteryChargingType)
    {
        case CONFIGURATION_DEFAULT_BATTERYCHARGING_TYPE_DCP:
        {
            CMD_LOG(_T("\tDCP - Dedicated Charging Port\n"));
            bStatus = FALSE;
            break;
        }
        case CONFIGURATION_DEFAULT_BATTERYCHARGING_TYPE_CDP:
        {
            CMD_LOG(_T("\tCDP - Charging Downstream Port\n"));
            bStatus = FALSE;
            break;
        }
        case CONFIGURATION_DEFAULT_BATTERYCHARGING_TYPE_SDP:
        {
            CMD_LOG(_T("\tSDP - Standard Downstream Port\n"));
            break;
        }
        case CONFIGURATION_DEFAULT_BATTERYCHARGING_TYPE_OFF:
        {
            CMD_LOG(_T("\tUNKNOWN\n"));
            bStatus = FALSE;
            break;
        }
    }


exit:

    //
    // Close device handle
    //
    FT_Close(ftHandle);

    return bStatus;
}

static BOOL EnableBatteryChargingDetection()
{
    FT_STATUS ftStatus = FT_OK;
    FT_HANDLE ftHandle;
    FT_60XCONFIGURATION oConfigurationData = {0};


	//
	// Open a device
	//
	ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
    if (FT_FAILED(ftStatus))
    {
        return FALSE;
    }


    //
    // Get configuration
    //
    ftStatus = FT_GetChipConfiguration(ftHandle, &oConfigurationData);
    if (FT_FAILED(ftStatus))
    {
        FT_Close(ftHandle);
        return FALSE;
    }


    //
    // Enable battery charging detection
    // BatteryChargingGPIOConfig Default setting : 11100100b  (0xE4)
    // 7 - 6 : DCP = 11b         (GPIO1 = 1 GPIO0 = 1)
    // 5 - 4 : CDP = 10b         (GPIO1 = 1 GPIO0 = 0)
    // 3 - 2 : SDP = 01b         (GPIO1 = 0 GPIO0 = 1)
    // 1 - 0 : Unknown/Off = 00b (GPIO1 = 0 GPIO0 = 0)
    //
    oConfigurationData.OptionalFeatureSupport = CONFIGURATION_OPTIONAL_FEATURE_ENABLEBATTERYCHARGING;
    oConfigurationData.BatteryChargingGPIOConfig = CONFIGURATION_DEFAULT_BATTERYCHARGING;


    //
    // Set configuration
    //
    ftStatus = FT_SetChipConfiguration(ftHandle, &oConfigurationData);
    if (FT_FAILED(ftStatus))
    {
        FT_Close(ftHandle);
        Sleep(5000);
        return FALSE;
    }


    //
    // Close device handle
    //
    FT_Close(ftHandle);


    //
    // After setting configuration, device will reboot
    // Wait for about 5 seconds for device and driver be ready
    //
    Sleep(5000);

    return TRUE;
}