/*
 * FT600 API Usage Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#include "stdafx.h"



#define LOOPBACK_DATA 4096



typedef struct _USER_CONTEXT
{
    CRITICAL_SECTION                    m_hCriticalSection;
    HANDLE                              m_hEventDataAvailable;
    FT_NOTIFICATION_CALLBACK_INFO_DATA  m_tNotificationData;

} USER_CONTEXT, *PUSER_CONTEXT;



static VOID NotificationCallback(PVOID pvCallbackContext, E_FT_NOTIFICATION_CALLBACK_TYPE eCallbackType, PVOID pvCallbackInfo);
static BOOL EnableNotificationMessage();
static BOOL DisableNotificationMessage();

///////////////////////////////////////////////////////////////////////////////////
// Demonstrates reading from IN pipes using notification messaging
// FT_Create                        Open a device
// FT_SetNotificationCallback		Register the callback function for DATA availability
// FT_ClearNotificationCallback		Unregister the callback function
// FT_Close                         Close device handle
///////////////////////////////////////////////////////////////////////////////////
BOOL NotificationDataTest()
{
    FT_STATUS ftStatus = FT_OK;
    FT_HANDLE ftHandle;
    UCHAR acWriteBuf[LOOPBACK_DATA] = { 0 };
    UCHAR acReadBuf[LOOPBACK_DATA] = { 0 };
    BOOL bResult = TRUE;
    USER_CONTEXT UserContext = {0};


    //
    // Enable notification messasge feature
    //
    if (!EnableNotificationMessage())
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
    UserContext.m_hEventDataAvailable = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!UserContext.m_hEventDataAvailable)
    {
        bResult = FALSE;
        goto exit;
    }
    InitializeCriticalSection(&UserContext.m_hCriticalSection);


    //
    // Set/register the callback function
    //
    ftStatus = FT_SetNotificationCallback(ftHandle, NotificationCallback, &UserContext);
    if (FT_FAILED(ftStatus))
    {
        bResult = FALSE;
        goto exit;
    }

    
    //
    // Loopback data using notification message
    //
    {
        // Write data sychronously
        ULONG ulBytesTransferred = 0;
        CMD_LOG(_T("\tWriting %d bytes\n"), sizeof(acWriteBuf));
        ftStatus = FT_WritePipe(ftHandle, 0x02, acWriteBuf, sizeof(acWriteBuf), &ulBytesTransferred, NULL);
        if (FT_FAILED(ftStatus))
        {
            bResult = FALSE;
            goto exit;
        }
        CMD_LOG(_T("\tWriting %d bytes DONE!\n"), ulBytesTransferred);


        // Read data using the notification callback
        ULONG ulCurrentRecvData = 0;
        do
        {
            DWORD dwRet = WaitForSingleObject(UserContext.m_hEventDataAvailable, INFINITE);
            EnterCriticalSection(&UserContext.m_hCriticalSection);
            switch (dwRet)
            {
                case WAIT_OBJECT_0: // Event was set by the notification callback
                {
                    CMD_LOG(_T("\tReading %d bytes!\n"), UserContext.m_tNotificationData.ulRecvNotificationLength);

                    if (UserContext.m_tNotificationData.ulRecvNotificationLength == 0 ||
                        UserContext.m_tNotificationData.ucEndpointNo == 0)
                    {
                        LeaveCriticalSection(&UserContext.m_hCriticalSection);
                        bResult = FALSE;
                        goto exit;
                    }

                    ULONG ulBytesTransferred = 0;
                    ftStatus = FT_ReadPipe( ftHandle, 
                                            UserContext.m_tNotificationData.ucEndpointNo, 
                                            &acReadBuf[ulCurrentRecvData],
                                            UserContext.m_tNotificationData.ulRecvNotificationLength, 
                                            &ulBytesTransferred, 
                                            NULL
                                            );
                    if (FT_FAILED(ftStatus))
                    {
                        CMD_LOG(_T("\tReading %d bytes FAILED! %x\n"), ulBytesTransferred, ftStatus);
                    }
                    else
                    {
                        ulCurrentRecvData += ulBytesTransferred;
                        CMD_LOG(_T("\tReading %d bytes DONE!\n"), ulBytesTransferred);
                    }

                    UserContext.m_tNotificationData.ulRecvNotificationLength = 0;
                    UserContext.m_tNotificationData.ucEndpointNo = 0;
                    LeaveCriticalSection(&UserContext.m_hCriticalSection);
                    break;
                }
                default:
                {
                    LeaveCriticalSection(&UserContext.m_hCriticalSection);
                    break;
                }
            }
        } 
        while (ulCurrentRecvData < LOOPBACK_DATA);

        //
        // Verify if data written is same as data read
        //
        if (memcmp(acWriteBuf, acReadBuf, LOOPBACK_DATA))
        {
            bResult = FALSE;
            goto exit;
        }
    }


exit:

    //
    // Clear/unregister the callback function
    //
    FT_ClearNotificationCallback(ftHandle);


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
    ftHandle = NULL;

	//revert back notification setting in config.
	DisableNotificationMessage();

    return bResult;
}


// The callback function should be as minimum as possible so as not to block any activity
// In this function, as illustrated, we just copy the callback result/information and then exit the function
static VOID NotificationCallback(PVOID pvCallbackContext, E_FT_NOTIFICATION_CALLBACK_TYPE eCallbackType, PVOID pvCallbackInfo)
{
    switch (eCallbackType)
    {
        case E_FT_NOTIFICATION_CALLBACK_TYPE_DATA:
        {
            PUSER_CONTEXT pUserContext = (PUSER_CONTEXT)pvCallbackContext;
            FT_NOTIFICATION_CALLBACK_INFO_DATA* pInfo = (FT_NOTIFICATION_CALLBACK_INFO_DATA*)pvCallbackInfo;

            EnterCriticalSection(&pUserContext->m_hCriticalSection);
            memcpy(&pUserContext->m_tNotificationData, pInfo, sizeof(*pInfo));
            SetEvent(pUserContext->m_hEventDataAvailable);
            LeaveCriticalSection(&pUserContext->m_hCriticalSection);

            break;
        }
        case E_FT_NOTIFICATION_CALLBACK_TYPE_GPIO: // fall-through
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
	FT_60XCONFIGURATION oConfigurationData = { 0 };


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

static BOOL EnableNotificationMessage()
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


    // Enable notification message for IN pipe for all channels
    oConfigurationData.OptionalFeatureSupport |= 
        CONFIGURATION_OPTIONAL_FEATURE_ENABLENOTIFICATIONMESSAGE_INCHALL;


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
