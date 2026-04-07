/*
 * FT600 API Usage Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#include "stdafx.h"



#define NUM_CPUCLOCK 4
#define NUM_FIFOCLOCK 4
#define NUM_FIFOMODE 2
#define NUM_BCCONFIGURATION 4
#define NUM_CHANNELCONFIG 5

CONST TCHAR* g_szFIFOClock[NUM_FIFOCLOCK] = 
{
    _T("100 MHz"),
    _T("66 MHz"),
    _T("50 MHz"),
    _T("40 MHz"),
};

CONST TCHAR* g_szFIFOMode[NUM_FIFOMODE] = 
{
    _T("245 Mode"),
    _T("600 Mode"),
};

CONST TCHAR* g_szBCDConfiguration[NUM_BCCONFIGURATION] = 
{
    _T("00 (GPIO1=0, GPIO2=0)"),
    _T("01 (GPIO1=0, GPIO2=1)"),
    _T("10 (GPIO1=1, GPIO2=0)"),
    _T("11 (GPIO1=1, GPIO2=1)"),
};

extern CONST TCHAR* g_szChannelConfig[NUM_CHANNELCONFIG] = 
{
    _T("4 Channels"),
    _T("2 Channels"),
    _T("1 Channel"),
    _T("1 OUT Pipe"),
    _T("1 IN Pipe"),
};



static 
BOOL SetStringDescriptors(
	UCHAR* pStringDescriptors, 
	ULONG ulSize,
	CONST CHAR* pManufacturer,
	CONST CHAR* pProductDescription,
	CONST CHAR* pSerialNumber
	)
{
	LONG lLen = 0;
	UCHAR bLen = 0;
	UCHAR* pPtr = pStringDescriptors;

	
	if (ulSize != 128 || pStringDescriptors == NULL)
	{
		return FALSE;
	}

	if (pManufacturer == NULL || pProductDescription == NULL || pSerialNumber == NULL)
	{
		return FALSE;
	}


	//
	// Verify input parameters
	//
	{
		// Manufacturer
		// Should be 15 bytes maximum
		// Should be printable characters
		lLen = strlen(pManufacturer);
		if (lLen < 1 || lLen >= 16)
		{
			return FALSE;
		}
		for (LONG i = 0; i < lLen; i++)
		{
			if (!isprint(pManufacturer[i]))
			{
				return FALSE;
			}
		}

		// Product Description
		// Should be 31 bytes maximum
		// Should be printable characters
		lLen = strlen(pProductDescription);
		if (lLen < 1 || lLen >= 32)
		{
			return FALSE;
		}
		for (LONG i = 0; i < lLen; i++)
		{
			if (!isprint(pProductDescription[i]))
			{
				return FALSE;
			}
		}

		// Serial Number
		// Should be 15 bytes maximum
		// Should be alphanumeric characters
		lLen = strlen(pSerialNumber);
		if (lLen < 1 || lLen >= 16)
		{
			return FALSE;
		}
		for (LONG i = 0; i < lLen; i++)
		{
			if (!isalnum(pSerialNumber[i]))
			{
				return FALSE;
			}
		}
	}


	//
	// Construct the string descriptors
	//
	{
		// Manufacturer
		bLen = strlen(pManufacturer);
		pPtr[0] = bLen * 2 + 2;
		pPtr[1] = 0x03;
		for (LONG i = 2, j = 0; i < pPtr[0]; i += 2, j++)
		{
			pPtr[i] = pManufacturer[j];
			pPtr[i + 1] = '\0';
		}
		pPtr += pPtr[0];

		// Product Description
		bLen = strlen(pProductDescription);
		pPtr[0] = bLen * 2 + 2;
		pPtr[1] = 0x03;
		for (LONG i = 2, j = 0; i < pPtr[0]; i += 2, j++)
		{
			pPtr[i] = pProductDescription[j];
			pPtr[i + 1] = '\0';
		}
		pPtr += pPtr[0];

		// Serial Number
		bLen = strlen(pSerialNumber);
		pPtr[0] = bLen * 2 + 2;
		pPtr[1] = 0x03;
		for (LONG i = 2, j = 0; i < pPtr[0]; i += 2, j++)
		{
			pPtr[i] = pSerialNumber[j];
			pPtr[i + 1] = '\0';
		}
	}

	return TRUE;
}


///////////////////////////////////////////////////////////////////////////////////
// Demonstrates setting of chip configuration from scratch
// FT_Create                    Open a device
// FT_SetChipConfiguration      Write configuration to chip's flash memory
// FT_Close                     Close device handle
///////////////////////////////////////////////////////////////////////////////////
BOOL SetChipConfigurationTest()
{
	FT_STATUS ftStatus = FT_OK;
	FT_HANDLE ftHandle;
	BOOL bRet = FALSE;
	FT_60XCONFIGURATION oConfigurationData = { 0 };


	//
	// Open a device index
	//
	ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
	if (FT_FAILED(ftStatus))
	{
		return FALSE;
	}

	//
	// Set the chip configuration structure
	//
	{
		memset(&oConfigurationData, 0, sizeof(oConfigurationData));

		// Default values
		oConfigurationData.VendorID = CONFIGURATION_DEFAULT_VENDORID;
		oConfigurationData.ProductID = CONFIGURATION_DEFAULT_PRODUCTID_601;
		oConfigurationData.PowerAttributes = CONFIGURATION_DEFAULT_POWERATTRIBUTES;
		oConfigurationData.PowerConsumption = CONFIGURATION_DEFAULT_POWERCONSUMPTION;
		oConfigurationData.FIFOClock = CONFIGURATION_DEFAULT_FIFOCLOCK;
		oConfigurationData.BatteryChargingGPIOConfig = CONFIGURATION_DEFAULT_BATTERYCHARGING;
		oConfigurationData.MSIO_Control = CONFIGURATION_DEFAULT_MSIOCONTROL;
		oConfigurationData.GPIO_Control = CONFIGURATION_DEFAULT_GPIOCONTROL;
		oConfigurationData.bInterval = 9;
		oConfigurationData.Reserved2 = 0;
		oConfigurationData.FlashEEPROMDetection = 0;

		// Customize
		oConfigurationData.FIFOMode = CONFIGURATION_FIFO_MODE_600;
		oConfigurationData.ChannelConfig = CONFIGURATION_CHANNEL_CONFIG_1;
		oConfigurationData.OptionalFeatureSupport = CONFIGURATION_OPTIONAL_FEATURE_DISABLECANCELSESSIONUNDERRUN;
		bRet = SetStringDescriptors(oConfigurationData.StringDescriptors, sizeof(oConfigurationData.StringDescriptors),
			"MyCompany", "This Is My Product Description", "1234567890ABCde");
		if (!bRet)
		{
			FT_Close(ftHandle);
			return FALSE;
		}
	}

	//
	// Set chip configuration using the structure created
	// 
	ftStatus = FT_SetChipConfiguration(ftHandle, &oConfigurationData);
	if (ftStatus == FT_INVALID_PARAMETER)
	{
		FT_Close(ftHandle);
		return FALSE;
	}

	FT_Close(ftHandle);
	Sleep(5000);

	printf("\n\tThe test may need some time for the driver to install\n");
	printf("\tsince the change in configuration involved all descriptors.\n");
	printf("\tPlease wait until the driver has been installed.\n");
	printf("\tOpen Device Manager to check the status of installation.\n\n\t");
	system("pause");

	//
	// Verify if the setting of chip configuration took effect properly
	//
	{
		FT_60XCONFIGURATION oReadConfigurationData = { 0 };

		ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
		if (FT_FAILED(ftStatus))
		{
			return FALSE;
		}

		ftStatus = FT_GetChipConfiguration(ftHandle, &oReadConfigurationData);
		if (FT_FAILED(ftStatus))
		{
			FT_Close(ftHandle);
			return FALSE;
		}

		// FlashEEPROMDetection is read-only value
		// So we don't have to compare it
		// We set it same as the value read before comparing the configuration structure
		oReadConfigurationData.FlashEEPROMDetection = oConfigurationData.FlashEEPROMDetection;

		if (memcmp(&oReadConfigurationData, &oConfigurationData, sizeof(oReadConfigurationData)))
		{
			FT_Close(ftHandle);
			return FALSE;
		}

		FT_Close(ftHandle);
	}

	printf("\tTest successful.\n");
	return TRUE;
}


///////////////////////////////////////////////////////////////////////////////////
// Demonstrates modifying current setting of chip configuration
// FT_Create                    Open a device
// FT_GetChipConfiguration      Read configuration from chip's flash memory
// FT_SetChipConfiguration      Write configuration to chip's flash memory
//                              FTDI_CFG - query and set chip configuration
// FT_Close                     Close device handle
///////////////////////////////////////////////////////////////////////////////////
BOOL ModifyChipConfigurationTest()
{
    FT_STATUS ftStatus = FT_OK;
    FT_HANDLE ftHandle;


	//
	// Open a device handle
	//
	ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
	if (FT_FAILED(ftStatus))
	{
		return FALSE;
	}


    //
    // Get chip configuration
    //
    FT_60XCONFIGURATION oConfigurationData = {0};
    ftStatus = FT_GetChipConfiguration(ftHandle, &oConfigurationData);
    if (FT_FAILED(ftStatus))
    {
        FT_Close(ftHandle);
        return FALSE;
    }
    ShowConfiguration(&oConfigurationData, TRUE);


    //
    // Set chip configuration
    //
#if USE_245_MODE
    oConfigurationData.FIFOMode = CONFIGURATION_FIFO_MODE_245;
    oConfigurationData.ChannelConfig = CONFIGURATION_CHANNEL_CONFIG_1;
#else // USE_245_MODE
    oConfigurationData.FIFOMode = CONFIGURATION_FIFO_MODE_600;
    oConfigurationData.ChannelConfig = CONFIGURATION_CHANNEL_CONFIG_4;
#endif // USE_245_MODE

    oConfigurationData.OptionalFeatureSupport = CONFIGURATION_OPTIONAL_FEATURE_DISABLECANCELSESSIONUNDERRUN;
    ftStatus = FT_SetChipConfiguration(ftHandle, &oConfigurationData);
    if (FT_FAILED(ftStatus))
    {
        FT_Close(ftHandle);
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


    //
    // Open a device
    //
    ftHandle = NULL;
	ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
	if (FT_FAILED(ftStatus))
    {
        return FALSE;
    }

    //
    // Get chip configuration
    //
    memset(&oConfigurationData, 0, sizeof(oConfigurationData));
    ftStatus = FT_GetChipConfiguration(ftHandle, &oConfigurationData);
    if (FT_FAILED(ftStatus))
    {
        FT_Close(ftHandle);
        return FALSE;
    }
    ShowConfiguration(&oConfigurationData, TRUE);

    //
    //  Check chip configuration
    //
    if (oConfigurationData.OptionalFeatureSupport != CONFIGURATION_OPTIONAL_FEATURE_DISABLECANCELSESSIONUNDERRUN)
    {
        FT_Close(ftHandle);
        return FALSE;
    }
#if USE_245_MODE
    if (oConfigurationData.FIFOMode != CONFIGURATION_FIFO_MODE_245 ||
        oConfigurationData.ChannelConfig != CONFIGURATION_CHANNEL_CONFIG_1 )
#else // USE_245_MODE
    if (oConfigurationData.FIFOMode != CONFIGURATION_FIFO_MODE_600 ||
        oConfigurationData.ChannelConfig != CONFIGURATION_CHANNEL_CONFIG_4 )
#endif // USE_245_MODE
    {
        FT_Close(ftHandle);
        return FALSE;
    }


    //
    // Close device handle
    //
    FT_Close(ftHandle);

    return TRUE;
}


///////////////////////////////////////////////////////////////////////////////////
//
// Default Chip Configuration
//
// FT_60XCONFIGURATION DefaultChipConfiguration =
// {
//    // Device Descriptor
//    0x0403,                 // VendorID
//    0x601F,                 // ProductID
//
//    // String Descriptors
//    {
//        0x0A,               // Manufacturer string length
//        0x03,               // Descriptor type
//        'F' , '\0','T' , '\0','D' , '\0','I' , '\0',
//        0x38,               // Product description string length
//        0x03,               // Descriptor type
//        'F' ,'\0','T' ,'\0','D' ,'\0','I' ,'\0',' ' ,'\0',
//        'S' ,'\0','u' ,'\0','p' ,'\0','e' ,'\0','r' ,'\0',
//        'S' ,'\0','p' ,'\0','e' ,'\0','e' ,'\0','d' ,'\0',
//        '-' ,'\0',
//        'F' ,'\0','I' ,'\0','F' ,'\0','O' ,'\0',' ' ,'\0',
//        'B' ,'\0','r' ,'\0','i' ,'\0','d' ,'\0','g' ,'\0','e' ,'\0',
//        0x1A,               // Serial Number String length
//        0x03,               // Descriptor type
//        '0' ,'\0','0' ,'\0','0' ,'\0','0' ,'\0','0' ,'\0','0','\0',
//        '0' ,'\0','0' ,'\0','0' ,'\0','0' ,'\0','0' ,'\0','1' ,'\0',
//    },
//
//    // Configuration Descriptor
//    0,                      // Reserved
//    0xE0,                   // PowerAttributes
//    96,                     // PowerConsumption (0x0060=96mA)
//
//    // Data Transfer Configuration
//    0,                      // Reserved2 
//    FIFO_CLK_100,           // FIFOClock
//    FIFO_MODE_600,          // FIFOMode
//    CHANNEL_CONFIG_4,       // ChannelConfig
//
//    // Optional Feature Support
//    0x0000,                 // OptionalFeatures
//    0xE4,                   // BatteryChargingGPIOConfig
//    0x0,                    // FlashEEPROMDetection
//
//    // MSIO and GPIO Configuration
//    0x10800,                // MSIOControl (32-bit)
//    0x0,                    // GPIOControl (32-bit)
// };
//
// When the chip is set to default chip configuration, the 2 GPIO pins can be set to high or low 
// to change the FIFO mode (FIFOMode) and Channel configuration (ChannelConfig).
//
//      GPIO 1  GPIO 0  FIFOMode        ChannelConfig
//      0       0       FIFO_MODE_245   CHANNEL_CONFIG_1
//      0       1       FIFO_MODE_600   CHANNEL_CONFIG_1
//      1       0       FIFO_MODE_600   CHANNEL_CONFIG_2
//      1       1       FIFO_MODE_600   CHANNEL_CONFIG_4
//
///////////////////////////////////////////////////////////////////////////////////
BOOL ResetChipConfigurationTest()
{
    FT_STATUS ftStatus = FT_OK;
    FT_HANDLE ftHandle;


    //
    // Open a device
    //
	ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
	if (FT_FAILED(ftStatus))
    {
        return FALSE;
    }


    //
    // Set configuration
    // Providing NULL ptConfiguration resets the chip configuration to default
    // When device is in default chconfiguration, note that the GPIO pins can affect it
    //
    ftStatus = FT_SetChipConfiguration(ftHandle, NULL);
    if (FT_FAILED(ftStatus))
    {
        FT_Close(ftHandle);
        return FALSE;
    }


    //
    // Close device handle
    //
    FT_Close(ftHandle);
    ftHandle = NULL;


    //
    // After setting configuration, device will reboot
    // Wait for about 5 seconds for device and driver be ready
    //
    Sleep(5000);

    CMD_LOG(_T("\n"));
    return TRUE;
}


static FT_60XCONFIGURATION oSaveConfigurationData = { 0 };

BOOL SaveChipConfigurationTest()
{
	FT_STATUS ftStatus = FT_OK;
	FT_HANDLE ftHandle;


	//
	// Open a device
	//
	ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
	if (FT_FAILED(ftStatus))
	{
		return FALSE;
	}


	//
	// Set chip configuration
	//
	ftStatus = FT_GetChipConfiguration(ftHandle, &oSaveConfigurationData);
	if (FT_FAILED(ftStatus))
	{
		FT_Close(ftHandle);
		return FALSE;
	}


	//
	// Close device handle
	//
	FT_Close(ftHandle);
	ftHandle = NULL;

	CMD_LOG(_T("\n"));
	return TRUE;
}

BOOL RevertChipConfigurationTest()
{
	FT_STATUS ftStatus = FT_OK;
	FT_HANDLE ftHandle;


	//
	// Open a device
	//
	ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
	if (FT_FAILED(ftStatus))
	{
		return FALSE;
	}


	//
	// Get chip configuration
	//
	ftStatus = FT_SetChipConfiguration(ftHandle, &oSaveConfigurationData);
	if (FT_FAILED(ftStatus))
	{
		FT_Close(ftHandle);
		return FALSE;
	}


	//
	// Close device handle
	//
	FT_Close(ftHandle);
	ftHandle = NULL;

	Sleep(2000);

	CMD_LOG(_T("\n"));
	return TRUE;
}


///////////////////////////////////////////////////////////////////////////////////
//
//Below is a description of the FT_60XCONFIGURATION chip configuration structure
//
//VendorID                  Vendor identification as specified in idVendor field of USB Device Descriptor. 
//                          This should match the VID in Windows installation file (INF).
//ProductID                 Product identification as specified in idProduct field of USB Device Descriptor. 
//                          This should match the PID in Windows installation file (INF).
//StringDescriptors         Concatenated String Descriptors for Manufacturer, Product Description and Serial Number 
//                          as specified in USB Device Descriptor. 
//PowerAttributes           Power attributes as specified in bmAttributes field of USB Configuration Descriptor. 
//                          Bit 5 indicates if device supports Remote Wakeup capability while 
//                          Bit 6 indicates if device is self-powered or bus-powered. 
//                          Note that Bit 7 should always be 1 based on the USB specification
//PowerConsumption          Maximum power consumption as specified in bMaxPower field of USB Configuration Descriptor.
//                          Note that a value of 0xC means a maximum power consumption of 0xC * 8 if USB 3 and 0xC * 2 if USB 2. 
//FIFOClock                 Clock speed of the FIFO in MHz (100, 66, 50, 40)
//                          Refer to FIFO_CLK enumeration
//FIFOMode                  Mode of the FIFO (245 or 600)
//                          Refer to FIFO_MODE enumeration
//ChannelConfig             Number of channels or pipes. A channel is equivalent to 
//                          2 pipes – 1 for OUT and 1 for IN. (4 channels, 2 channels, 1 channel, 1 OUT pipe, 1 IN pipe)
//                          Refer to CHANNEL_CONFIG enumeration
//OptionalFeatureSupport    Bitmap indicating the optional feature support
//                          15 - 6  : Reserved
//                          5       : Enable Notification Message for Ch4 IN (Default value = 0)
//                                    When this bit it set, notification message will be enabled for the IN pipe of channel 4. 
//                                    Refer to Bit 2 for more information.
//                          4       : Enable Notification Message for Ch3 IN (Default value = 0)
//                                    When this bit it set, notification message will be enabled for the IN pipe of channel 3. 
//                                    Refer to Bit 2 for more information.
//                          3       : Enable Notification Message for Ch2 IN (Default value = 0)
//                                    When this bit it set, notification message will be enabled for the IN pipe of channel 2. 
//                                    Refer to Bit 2 for more information.
//                          2       : Enable Notification Message for Ch1 IN (Default value = 0)
//                                    When this bit it set, notification message will be enabled for the IN pipe of channel 1. 
//                                    Host application will not actively read on this pipe, instead will register a callback function. 
//                                    The callback function will be called when there is data available for the pipe. 
//                                    This feature is intended for unexpected short packets(maximum of 4kb), 
//                                    such as error status information from the FIFO master to host application.
//                                    For example, for a camera device, user can select 2 channel configuration 
//                                    because it needs 2 IN pipes – 1 for camera data, 1 for initial handshake / error status information.
//                                    Notification message should be used for the control / error status information pipe but not for the camera data pipe.
//                          1       : Disable Cancel Session On Underrun (Default value = 0)
//                                    When this bit is set, firmware will not cancel or invalidate IN request from host application 
//                                    when it receives underrun condition from the FIFO master and if the packet size received from FIFO master 
//                                    is a multiple of USB max packet size(USB3 : 1024, USB2 : 512).
//                                    By default, firmware always cancels or invalidates IN requests from host application 
//                                    when it receives underrun condition from the FIFO master.
//                                    Underrun condition happens when FIFO master provides data less than the FIFOSegmentSize. FIFOSegmentSize is as follows :
//                                    1KB : CHANNEL_CONFIG_4
//                                    2KB : CHANNEL_CONFIG_2
//                                    4KB : CHANNEL_CONFIG_1
//                                    8KB : CHANNEL_CONFIG_1_OUTPIPE
//                                    16KB: CHANNEL_CONFIG_1_INPIPE
//                          0       : Enable Battery Charging Detection (Default value = 0)
//                                    When this bit is set, the 2 GPIOs will be configured to indicate the type of power source the device is connected to. 
//                                    The GPIO setting is indicated by BatteryChargingGPIOConfig.
//BatteryChargingGPIOConfig           Bitmap indicating the type of power source the device is detected to by the Battery Charging module of the firmware.
//                                    7 - 6 : DCP Dedicated Charging Port
//                                    5 - 4 : CDP Charging Downstream Port
//                                    3 - 2 : SDP Standard Downstream Port
//                                    1 - 0 : Unknown/Off
//                                    Default setting : 11100100b  (0xE4)
//                                    7 - 6 : DCP = 11b     (GPIO1 = 1 GPIO0 = 1)
//                                    5 - 4 : CDP = 10b     (GPIO1 = 1 GPIO0 = 0)
//                                    3 - 2 : SDP = 01b     (GPIO1 = 0 GPIO0 = 1)
//                                    1 - 0 : Unknown/Off = 00b (GPIO1 = 0 GPIO0 = 0)
//FlashEEPROMDetection                Bitmap indicating status of chip configuration initialization
//                                    7 : GPIO 1 status if Bit 5 is set(High = 1, Low = 0)
//                                    6 : GPIO 0 status if Bit 5 is set(High = 1, Low = 0)
//                                    5 : Is GPIO used as configuration input ? (Yes = 1, No = 0)
//                                    4 : Is custom memory use ? (Custom = 1, Default = 0)
//                                    3 : Is custom configuration data checksum invalid ? (Invalid = 1, Valid = 0)
//                                    2 : Is custom configuration data invalid ? (Invalid = 1, Valid = 0)
//                                    1 : Is Memory Not Exist ? (Not Exists = 1, Exist 0)
//                                    0 : Is ROM ? (ROM = 1, Flash = 0)
//MSIO_Control                        Configuration to control the drive strengths of the FIFO pins 
//GPIO_Control                        Configuration to control the drive strengths of the GPIO pins
//
///////////////////////////////////////////////////////////////////////////////////
VOID ShowConfiguration(CONST PVOID a_pvConfigurationData, BOOL a_bRead)
{
    CONST PFT_60XCONFIGURATION a_pConfigurationData = (PFT_60XCONFIGURATION) a_pvConfigurationData;

	CMD_LOG(_T("\n"));
    CMD_LOG(_T("\tDevice Descriptor\n"));
    CMD_LOG(_T("\tVendorID                 : 0x%04X\n"),      a_pConfigurationData->VendorID);
    CMD_LOG(_T("\tProductID                : 0x%04X\n"),      a_pConfigurationData->ProductID);

    TCHAR ucTemp[128] = {0};
    UCHAR* pOffset = NULL;
	CMD_LOG(_T("\n"));
	CMD_LOG(_T("\tString Descriptor\n"));
    pOffset = (UCHAR*)&a_pConfigurationData->StringDescriptors[0];
    memset(ucTemp, 0, sizeof(ucTemp));
    if (pOffset[0]-2)
    {
        memcpy(ucTemp, &pOffset[2], pOffset[0]-2);
    }
    CMD_LOG(_T("\tManufacturer             : %s\n"),      ucTemp);
    pOffset += pOffset[0];
    memset(ucTemp, 0, sizeof(ucTemp));
    if (pOffset[0]-2)
    {
        memcpy(ucTemp, &pOffset[2], pOffset[0]-2);
    }
    CMD_LOG(_T("\tProduct Description      : %s\n"),      ucTemp);
    pOffset += pOffset[0];
    memset(ucTemp, 0, sizeof(ucTemp));
    if (pOffset[0]-2)
    {
        memcpy(ucTemp, &pOffset[2], pOffset[0]-2);
    }
    CMD_LOG(_T("\tSerial Number            : %s\n"),      ucTemp);

	CMD_LOG(_T("\n"));
	CMD_LOG(_T("\tConfiguration Descriptor\n"));
    CMD_LOG(_T("\tPowerAttributes          : %s %s\n"),       FT_IS_BUS_POWERED(a_pConfigurationData->PowerAttributes) ? _T("Self-powered ") : _T("Bus-powered "),
                                                            FT_IS_REMOTE_WAKEUP_ENABLED(a_pConfigurationData->PowerAttributes) ? _T("Remote wakeup ") : _T(""));
    CMD_LOG(_T("\tPowerConsumption         : %d\n"),          a_pConfigurationData->PowerConsumption);

	CMD_LOG(_T("\n"));
	CMD_LOG(_T("\tData Transfer\n"));
    CMD_LOG(_T("\tFIFOClock                : %s\n"),          g_szFIFOClock[a_pConfigurationData->FIFOClock]);
    CMD_LOG(_T("\tFIFOMode                 : %s\n"),          g_szFIFOMode[a_pConfigurationData->FIFOMode]);
    CMD_LOG(_T("\tChannelConfig            : %s\n"),          g_szChannelConfig[a_pConfigurationData->ChannelConfig]);

	CMD_LOG(_T("\n"));
	CMD_LOG(_T("\tOptional Features\n"));
    CMD_LOG(_T("\tDisableCancelOnUnderrun  : %d\n"),          (a_pConfigurationData->OptionalFeatureSupport & CONFIGURATION_OPTIONAL_FEATURE_DISABLECANCELSESSIONUNDERRUN) >> 1);
    CMD_LOG(_T("\tNotificationEnabled      : %d (%d %d %d %d)\n"),
                (a_pConfigurationData->OptionalFeatureSupport & CONFIGURATION_OPTIONAL_FEATURE_ENABLENOTIFICATIONMESSAGE_INCHALL) >> 2,
                (a_pConfigurationData->OptionalFeatureSupport & CONFIGURATION_OPTIONAL_FEATURE_ENABLENOTIFICATIONMESSAGE_INCH1) ? 1 : 0,
                (a_pConfigurationData->OptionalFeatureSupport & CONFIGURATION_OPTIONAL_FEATURE_ENABLENOTIFICATIONMESSAGE_INCH2) ? 1 : 0,
                (a_pConfigurationData->OptionalFeatureSupport & CONFIGURATION_OPTIONAL_FEATURE_ENABLENOTIFICATIONMESSAGE_INCH3) ? 1 : 0,
                (a_pConfigurationData->OptionalFeatureSupport & CONFIGURATION_OPTIONAL_FEATURE_ENABLENOTIFICATIONMESSAGE_INCH4) ? 1 : 0
                                                            );
    CMD_LOG(_T("\tBatteryChargingEnabled   : %d\n"),          a_pConfigurationData->OptionalFeatureSupport & CONFIGURATION_OPTIONAL_FEATURE_ENABLEBATTERYCHARGING);
    CMD_LOG(_T("\tBCGPIOPinConfig          : 0x%02X\n"),      a_pConfigurationData->BatteryChargingGPIOConfig);

    if (a_bRead)
    {
        CMD_LOG(_T("\tFlashEEPROMDetection     : 0x%02X\n\n"),a_pConfigurationData->FlashEEPROMDetection);
        CMD_LOG(_T("\t\tMemory Type            : %s\n"),      a_pConfigurationData->FlashEEPROMDetection & (1<<CONFIGURATION_FLASH_ROM_BIT_ROM)                       ? _T("ROM")         : _T("Flash"));
        CMD_LOG(_T("\t\tMemory Detected        : %s\n"),      a_pConfigurationData->FlashEEPROMDetection & (1<<CONFIGURATION_FLASH_ROM_BIT_MEMORY_NOTEXIST)           ? _T("Not Exists")  : _T("Exists"));
        CMD_LOG(_T("\t\tCustom Config Validity : %s\n"),      a_pConfigurationData->FlashEEPROMDetection & (1<<CONFIGURATION_FLASH_ROM_BIT_CUSTOMDATA_INVALID)        ? _T("Invalid")     : _T("Valid"));
        CMD_LOG(_T("\t\tCustom Config Checksum : %s\n"),      a_pConfigurationData->FlashEEPROMDetection & (1<<CONFIGURATION_FLASH_ROM_BIT_CUSTOMDATACHKSUM_INVALID)  ? _T("Invalid")     : _T("Valid"));
        CMD_LOG(_T("\t\tGPIO Input             : %s\n"),      a_pConfigurationData->FlashEEPROMDetection & (1<<CONFIGURATION_FLASH_ROM_BIT_GPIO_INPUT)                ? _T("Used")        : _T("Ignore"));
        CMD_LOG(_T("\t\tConfig Used            : %s\n"),      a_pConfigurationData->FlashEEPROMDetection & (1<<CONFIGURATION_FLASH_ROM_BIT_CUSTOM)                    ? _T("CUSTOM")      : _T("DEFAULT"));
        if (a_pConfigurationData->FlashEEPROMDetection & (1<<CONFIGURATION_FLASH_ROM_BIT_GPIO_INPUT))
        {
            CMD_LOG(_T("\t\tGPIO Input             : %s\n"),  a_pConfigurationData->FlashEEPROMDetection & (1<<CONFIGURATION_FLASH_ROM_BIT_GPIO_INPUT)  ? _T("Used")        : _T("Ignore"));
            CMD_LOG(_T("\t\tGPIO 0                 : %s\n"),  a_pConfigurationData->FlashEEPROMDetection & (1<<CONFIGURATION_FLASH_ROM_BIT_GPIO_0)      ? _T("High")        : _T("Low"));
            CMD_LOG(_T("\t\tGPIO 1                 : %s\n"),  a_pConfigurationData->FlashEEPROMDetection & (1<<CONFIGURATION_FLASH_ROM_BIT_GPIO_1)      ? _T("High")        : _T("Low"));
        }
    }

	CMD_LOG(_T("\n"));
	CMD_LOG(_T("\tMSIO and GPIO configuration\n"));
    CMD_LOG(_T("\tMSIOControl              : 0x%08X\n"),      a_pConfigurationData->MSIO_Control);
    CMD_LOG(_T("\tGPIOControl              : 0x%08X\n"),      a_pConfigurationData->GPIO_Control);

    CMD_LOG(_T("\n"));
}


