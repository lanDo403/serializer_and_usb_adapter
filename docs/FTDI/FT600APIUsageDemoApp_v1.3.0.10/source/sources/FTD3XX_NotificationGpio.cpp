/*
 * FT600 API Usage Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#include "stdafx.h"
#include <cmath> // for pow(x, y)



typedef struct _USER_CONTEXT
{
    CRITICAL_SECTION                    m_hCriticalSection;
    HANDLE                              m_hEventDataAvailable;
    FT_NOTIFICATION_CALLBACK_INFO_GPIO  m_tNotificationGpio;

} USER_CONTEXT, *PUSER_CONTEXT;


static VOID NotificationCallback(PVOID pvCallbackContext, E_FT_NOTIFICATION_CALLBACK_TYPE eCallbackType, PVOID pvCallbackInfo);
static BOOL DisableNotificationMessage();
static BOOL EnableBatteryChargingDetection();



///////////////////////////////////////////////////////////////////////////////////
// Demonstrates reading and writing to and from the 2 GPIO pins
// FT_Create                        Open a device handle
// FT_GetGPIO                       Get the status of GPIO pins
// FT_SetGPIO                       Set the status of GPIO pins
// FT_ClearNotificationCallback Clear the callback function for GPIO pins
// FT_Close                         Close device handle
///////////////////////////////////////////////////////////////////////////////////
BOOL NotificationGpioTest()
{
    FT_STATUS ftStatus = FT_OK;
    FT_HANDLE ftHandle;
    USHORT uwLatency = 0;
    UCHAR ucDirection = 0;
    UCHAR ucValue = 0;
    FT_PIPE_INFORMATION oNotificationPipeInfo;
    USHORT uwTimeGapMillisec = 0;
    USER_CONTEXT UserContext = {0};
    UCHAR ucIterations = 0;


    //
    // Disable notification message feature if enabled
    //
    if (!DisableNotificationMessage())
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
    // Initialize event and critical section
    //
    UserContext.m_hEventDataAvailable = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!UserContext.m_hEventDataAvailable)
    {
        FT_Close(ftHandle);
        return FALSE;
    }
    InitializeCriticalSection(&UserContext.m_hCriticalSection);


    //
    // Get the latency of the interrupt pipe, pipe 0x81 in interface 0
    //
    ftStatus = FT_GetPipeInformation(   ftHandle, 
                                        FT_RESERVED_INTERFACE_INDEX, 
                                        FT_RESERVED_PIPE_INDEX_NOTIFICATION, 
                                        &oNotificationPipeInfo
                                        );
    if (FT_FAILED(ftStatus))
    {
        FT_Close(ftHandle);
        return FALSE;
    }
    uwTimeGapMillisec = (USHORT)pow(2, oNotificationPipeInfo.Interval-1)*125/1000;
    //DEBUG(_T("\n\tInterval %d TimeGapMillisec %d\n"), oNotificationPipeInfo.Interval, uwTimeGapMillisec);


    //
    // Set GPIO status
    // Swap the values so we can check later if GPIO status really changed using FT_GetGPIO
    //
    {
        ucDirection = (FT_GPIO_DIRECTION_OUT << FT_GPIO_0) | (FT_GPIO_DIRECTION_OUT << FT_GPIO_1); // set both GPIO pins 0 and 1 to OUT 
        if (UserContext.m_tNotificationGpio.bGPIO0 && UserContext.m_tNotificationGpio.bGPIO1)
        {
            ucValue = (FT_GPIO_VALUE_LOW << FT_GPIO_0) | (FT_GPIO_VALUE_LOW << FT_GPIO_1); // set both GPIO pins 0 and 1 to 0
        }
        else
        {
            ucValue = (FT_GPIO_VALUE_HIGH << FT_GPIO_0) | (FT_GPIO_VALUE_HIGH << FT_GPIO_1); // set both GPIO pins 0 and 1 to 1
        }
        ftStatus = FT_SetGPIO(ftHandle, ucDirection, ucValue);
        if (FT_FAILED(ftStatus))
        {
            FT_Close(ftHandle);
            return FALSE;
        }
    }

    //
    // Get GPIO status
    // To get the latency for 1 second = 1000 milliseconds, use 1000 milliseconds divide by uwTimeGapMillisec
    // WaitForSingleObject will be triggered every 1 second
    //
    {
        ucIterations = 3;
        uwLatency = 1000/uwTimeGapMillisec; // compute for 1000 milliseconds 
        ucDirection = (FT_GPIO_DIRECTION_OUT << FT_GPIO_0) | (FT_GPIO_DIRECTION_OUT << FT_GPIO_1); // set both GPIO pins 0 and 1 to OUT 
        ftStatus = FT_GetGPIO(ftHandle, ucDirection, NotificationCallback, &UserContext, uwLatency);
        if (FT_FAILED(ftStatus))
        {
            FT_Close(ftHandle);
            return FALSE;
        }

        //
        // Wait for the callback function to be called which will set the event
        //
        for (UCHAR i=0; i<ucIterations; i++)
        {
            WaitForSingleObject(UserContext.m_hEventDataAvailable, INFINITE);

            EnterCriticalSection(&UserContext.m_hCriticalSection);
            DEBUG(_T("\n\tGPIO0: %d GPIO1: %d\n"), 
                UserContext.m_tNotificationGpio.bGPIO0, 
                UserContext.m_tNotificationGpio.bGPIO1
                );
            ResetEvent(UserContext.m_hEventDataAvailable);
            LeaveCriticalSection(&UserContext.m_hCriticalSection);
        }

        //
        // Clear/unregister the callback function
        //
        FT_ClearNotificationCallback(ftHandle);
    }


    //
    // Delete event and critical section
    //
    DeleteCriticalSection(&UserContext.m_hCriticalSection);
    if (UserContext.m_hEventDataAvailable)
    {
        CloseHandle(UserContext.m_hEventDataAvailable);
        UserContext.m_hEventDataAvailable = NULL;
    }


    //
    // Close device handle
    //
    FT_Close(ftHandle);

    return TRUE;
}


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
    USHORT uwLatency = 0;
    UCHAR ucDirection = 0;
    USER_CONTEXT UserContext = {0};
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
    // Initialize event and critical section
    //
    UserContext.m_hEventDataAvailable = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!UserContext.m_hEventDataAvailable)
    {
        bStatus = FALSE;
        goto exit;
    }
    InitializeCriticalSection(&UserContext.m_hCriticalSection);

    //
    // Get GPIO status
    //
    {
        uwLatency = 1;
        ucDirection = (FT_GPIO_DIRECTION_IN << FT_GPIO_0) | (FT_GPIO_DIRECTION_IN << FT_GPIO_1); // set both GPIO pins 0 and 1 to OUT 
        ftStatus = FT_GetGPIO(ftHandle, ucDirection, NotificationCallback, &UserContext, uwLatency);
        if (FT_FAILED(ftStatus))
        {
            bStatus = FALSE;
            goto exit;
        }

        //
        // Wait for the callback function to be called which will set the event
        //
        WaitForSingleObject(UserContext.m_hEventDataAvailable, INFINITE);
        EnterCriticalSection(&UserContext.m_hCriticalSection);
        DEBUG(_T("\n\tGPIO0: %d GPIO1: %d\n"), UserContext.m_tNotificationGpio.bGPIO0, UserContext.m_tNotificationGpio.bGPIO1);
        ResetEvent(UserContext.m_hEventDataAvailable);
        LeaveCriticalSection(&UserContext.m_hCriticalSection);

        //
        // Clear/unregister the callback function
        //
        FT_ClearNotificationCallback(ftHandle);
    }

    //
    // Check the GPIO status
    // BatteryChargingGPIOConfig Default setting : 11100100b  (0xE4)
    // 7 - 6 : DCP = 11b         (GPIO1 = 1 GPIO0 = 1)
    // 5 - 4 : CDP = 10b         (GPIO1 = 1 GPIO0 = 0)
    // 3 - 2 : SDP = 01b         (GPIO1 = 0 GPIO0 = 1)
    // 1 - 0 : Unknown/Off = 00b (GPIO1 = 0 GPIO0 = 0)
    // Since device is connected to a host machine, then we should get SDP (GPIO1 = 0 GPIO0 = 1)
    UCHAR ucBatteryChargingType = (UserContext.m_tNotificationGpio.bGPIO1 << FT_GPIO_1) | (UserContext.m_tNotificationGpio.bGPIO0 << FT_GPIO_0);
    switch(ucBatteryChargingType)
    {
        case CONFIGURATION_DEFAULT_BATTERYCHARGING_TYPE_DCP:
        {
            DEBUG(_T("\n\tDCP - Dedicated Charging Port\n"));
            bStatus = FALSE;
            break;
        }
        case CONFIGURATION_DEFAULT_BATTERYCHARGING_TYPE_CDP:
        {
            DEBUG(_T("\n\tCDP - Charging Downstream Port\n"));
            bStatus = FALSE;
            break;
        }
        case CONFIGURATION_DEFAULT_BATTERYCHARGING_TYPE_SDP:
        {
            DEBUG(_T("\n\tSDP - Standard Downstream Port\n"));
            break;
        }
        case CONFIGURATION_DEFAULT_BATTERYCHARGING_TYPE_OFF:
        {
            DEBUG(_T("\n\tUNKNOWN\n"));
            bStatus = FALSE;
            break;
        }
    }


exit:

    //
    // Delete event and critical section
    //
    DeleteCriticalSection(&UserContext.m_hCriticalSection);
    if (UserContext.m_hEventDataAvailable)
    {
        CloseHandle(UserContext.m_hEventDataAvailable);
        UserContext.m_hEventDataAvailable = NULL;
    }

    //
    // Close device handle
    //
    FT_Close(ftHandle);

    return bStatus;
}


// The callback function should be as minimum as possible so as not to block any activity
// In this function, as illustrated, we just copy the callback result/information and then exit the function
static VOID NotificationCallback(PVOID pvCallbackContext, E_FT_NOTIFICATION_CALLBACK_TYPE eCallbackType, PVOID pvCallbackInfo)
{
    switch (eCallbackType)
    {
        case E_FT_NOTIFICATION_CALLBACK_TYPE_GPIO:
        {
            PUSER_CONTEXT pUserContext = (PUSER_CONTEXT)pvCallbackContext;
            FT_NOTIFICATION_CALLBACK_INFO_GPIO* pInfo = (FT_NOTIFICATION_CALLBACK_INFO_GPIO*)pvCallbackInfo;

            EnterCriticalSection(&pUserContext->m_hCriticalSection);
            memcpy(&pUserContext->m_tNotificationGpio, pInfo, sizeof(*pInfo));
            SetEvent(pUserContext->m_hEventDataAvailable);
            LeaveCriticalSection(&pUserContext->m_hCriticalSection);

            break;
        }
        case E_FT_NOTIFICATION_CALLBACK_TYPE_DATA: // fall-through
        default:
        {
            break;
        }
    }
}


static BOOL DisableNotificationMessage()
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
    // Disable notification message for IN pipe for all channels
    //
    oConfigurationData.OptionalFeatureSupport &= 
        ~(CONFIGURATION_OPTIONAL_FEATURE_ENABLENOTIFICATIONMESSAGE_INCHALL);


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