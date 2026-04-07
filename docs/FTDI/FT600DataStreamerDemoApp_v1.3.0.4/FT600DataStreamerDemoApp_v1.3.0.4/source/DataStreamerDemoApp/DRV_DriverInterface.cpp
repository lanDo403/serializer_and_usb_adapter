/*
 * FT600 Data Streamer Demo App
 *
 * Copyright (C) 2016 FTDI Chip
 *
 */

#include "stdafx.h"
#include "DRV_DriverInterface.h"
#include "APP_Device.h"
#include "APP_Logger.h"



////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::CDriverInterface
//      This is the wrapper class to the driver. 
//      Currently this application uses the test driver - FT600 API over WinUSB. 
//      Once D2XX supports FT600, this file will be modified to use the D2XX APIs. 
//      D2XX driver is the USB driver for all FTDI USB products.
//
// Summary
//
// Parameters
//
// Return Value
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

CDriverInterface::CDriverInterface()
    : m_FTHandle(NULL),
      m_bIsUsb3(TRUE)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::~CDriverInterface
//
// Summary
//
// Parameters
//
// Return Value
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

CDriverInterface::~CDriverInterface()
{
    Cleanup();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::GetNumberOfDevicesConnected
//
// Summary
//
// Parameters
//
// Return Value
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

DWORD CDriverInterface::GetNumberOfDevicesConnected()
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


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::GetDevicesInfoList
//
// Summary
//
// Parameters
//
// Return Value
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

DWORD CDriverInterface::GetDevicesInfoList(FT_DEVICE_LIST_INFO_NODE **pptDevicesInfo)
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


DWORD CDriverInterface::GetDeviceIndex(
	FT_DEVICE_LIST_INFO_NODE* ptDevicesInfo, 
	DWORD dwNumDevices, 
	EOPEN_BY a_eTypeOpenBy,
	PVOID a_pvOpenBy
	)
{
	FT_STATUS ftStatus = FT_OK;

	for (DWORD i = 0; i < dwNumDevices; i++)
	{
		switch (a_eTypeOpenBy)
		{
			case EOPEN_BY_DESC:
			{
				if (strcmp(ptDevicesInfo[i].Description, (CHAR*)a_pvOpenBy) == 0)
				{
					return i;
				}
				break;
			}
			case EOPEN_BY_SERIAL:
			{
				if (strcmp(ptDevicesInfo[i].SerialNumber, (CHAR*)a_pvOpenBy) == 0)
				{
					return i;
				}
				break;
			}
			case EOPEN_BY_INDEX:
			{
				if (i == atoi((CHAR*)a_pvOpenBy))
				{
					return i;
				}
				break;
			}
			default:
			{
				return 0xFFFFFFFF;
			}

		} // end switch

	} // end for

	return 0xFFFFFFFF;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::ReleaseDevicesInfoList
//
// Summary
//
// Parameters
//
// Return Value
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

VOID CDriverInterface::ReleaseDevicesInfoList(FT_DEVICE_LIST_INFO_NODE *ptDevicesInfo)
{
	if (ptDevicesInfo)
	{
		free(ptDevicesInfo);
		ptDevicesInfo = NULL;
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::CDriverInterface
//
// Summary
//      Initializes the driver
//
// Parameters
//      a_eTypeOpenBy   - Indicates how device will be opened
//      a_pvOpenBy      - Indicates the parameter to be used in opening the device
//
// Return Value
//      TRUE if device was initialized properly; otherwise returns FALSE
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CDriverInterface::Initialize(
    EOPEN_BY a_eTypeOpenBy, 
    PVOID a_pvOpenBy
    )
{
    BOOL bResult = FALSE;
    FT_STATUS ftStatus = FT_OK;
    int iDeviceNumber = 0;
    int iNumTries = 0;
	DWORD dwNumDevices = 0;
	DWORD dwDeviceIndex = 0;
	FT_DEVICE_LIST_INFO_NODE *ptDevicesInfo = NULL;
	FT_60XCONFIGURATION oConfig = { 0 };


	std::string strOpenBy((CHAR*)a_pvOpenBy);
	std::wstring wstrOpenBy(strOpenBy.begin(), strOpenBy.end());

	dwNumDevices = GetNumberOfDevicesConnected();
	if (dwNumDevices == 0)
	{
		GUI_LOG(_T("No device is connected!"));
		return FALSE;
	}

	if (GetDevicesInfoList(&ptDevicesInfo) == 0)
	{
		GUI_LOG(_T("No device is connected!"));
		return FALSE;
	}

	GUI_LOG(_T("\nList of Connected Devices!\n"));
	for (DWORD i = 0; i < dwNumDevices; i++)
	{
		std::string strSerialNumber(ptDevicesInfo[i].SerialNumber);
		std::wstring wstrSerialNumber(strSerialNumber.begin(), strSerialNumber.end());
		std::string strDescription(ptDevicesInfo[i].Description);
		std::wstring wstrDescription(strDescription.begin(), strDescription.end());

		GUI_LOG(_T("Device[%d]"),			i);
		GUI_LOG(_T("\tFlags: 0x%x %s  |  Type: %d %s  |  ID: 0x%08X  |  ftHandle: 0x%x %s"),	
											ptDevicesInfo[i].Flags,
											ptDevicesInfo[i].Flags & FT_FLAGS_SUPERSPEED ? _T("[USB 3]") :
											ptDevicesInfo[i].Flags & FT_FLAGS_HISPEED ? _T("[USB 2]") : 
											ptDevicesInfo[i].Flags & FT_FLAGS_OPENED ? _T("[OPENED]") : _T(""),
											ptDevicesInfo[i].Type,
											ptDevicesInfo[i].Type == FT_DEVICE_600 || ptDevicesInfo[i].Type == FT_DEVICE_601 ? _T("[FT60X]") : _T(""),
											ptDevicesInfo[i].ID,
											ptDevicesInfo[i].ftHandle,
											ptDevicesInfo[i].ftHandle ? _T("[Already open]") : _T("")
											);
		GUI_LOG(_T("\tDescription: %s"),	wstrDescription.c_str());
		GUI_LOG(_T("\tSerialNumber: %s"),	wstrSerialNumber.c_str());
	}
	GUI_LOG(_T(""));

	dwDeviceIndex = GetDeviceIndex(ptDevicesInfo, dwNumDevices, a_eTypeOpenBy, a_pvOpenBy);
	if (dwDeviceIndex == 0xFFFFFFFF)
	{
		GUI_LOG(_T("Device with %s [%s] not found!"),
			a_eTypeOpenBy == EOPEN_BY_DESC ? _T("Description") : 
			a_eTypeOpenBy == EOPEN_BY_SERIAL ? _T("Serial Number"):
			a_eTypeOpenBy == EOPEN_BY_INDEX ? _T("Index") : _T(""),
			wstrOpenBy.c_str());

		ReleaseDevicesInfoList(ptDevicesInfo);
		return FALSE;
	}

	ReleaseDevicesInfoList(ptDevicesInfo);

    CMD_LOG(_T("FT_Open"));
    m_FTHandle = NULL;

    do
    {
        switch (a_eTypeOpenBy)
        {
            case EOPEN_BY_GUID:
            {
                ftStatus = FT_Create(a_pvOpenBy, FT_OPEN_BY_GUID, &m_FTHandle);
                break;
            }
            case EOPEN_BY_DESC:
            {
                ftStatus = FT_Create(a_pvOpenBy, FT_OPEN_BY_DESCRIPTION, &m_FTHandle);
                break;
            }
			case EOPEN_BY_SERIAL:
			{
				ftStatus = FT_Create(a_pvOpenBy, FT_OPEN_BY_SERIAL_NUMBER, &m_FTHandle);
				break;
			}
			case EOPEN_BY_INDEX:
			{
				ULONG ulIndex = atoi((CHAR*)a_pvOpenBy);
				ftStatus = FT_Create((PVOID)ulIndex, FT_OPEN_BY_INDEX, &m_FTHandle);
				break;
			}
		}

        if (FT_FAILED(ftStatus))
        { 
            CMD_LOG(_T("\t%s FAILED! FT_Open"), _T(__FUNCTION__));
            Sleep(1000);
            continue;
        }

        // Get device VID and PID 
        // to verify if this is FT60X
        USHORT uwVID = 0;
        USHORT uwPID = 0;
        ftStatus = FT_GetVIDPID(m_FTHandle, &uwVID, &uwPID);
        if (FT_FAILED(ftStatus))
        { 
            CMD_LOG(_T("\t%s FAILED! FT_GetVIDPID"), _T(__FUNCTION__));
            break;
        }
        if ((uwVID != FT600_VID) || 
            (uwPID != FT600_PID && uwPID != FT601_PID) )
        {
            CMD_LOG(_T("\t%s FAILED! FT_GetVIDPID VID=0x%04x PID=0x%04x"), _T(__FUNCTION__), uwVID, uwPID);
            Cleanup();
            break;
        }

		ftStatus = FT_GetChipConfiguration(m_FTHandle, &oConfig);
		if (FT_FAILED(ftStatus))
		{
			CMD_LOG_NOTIME(_T("\t%s FAILED! FT_GetChipConfiguration [0x%x]"), _T(__FUNCTION__), ftStatus);
			break;
		}
		m_FIFOMode = oConfig.FIFOMode;

		if (m_FIFOMode == CONFIGURATION_FIFO_MODE_245)
		{
			/* Configure the GPIOs to OUTPUT mode.*/
			ftStatus = FT_EnableGPIO(m_FTHandle,
				(FT_GPIO_DIRECTION_OUT << FT_GPIO_0) | (FT_GPIO_DIRECTION_OUT << FT_GPIO_1),
				(FT_GPIO_DIRECTION_OUT << FT_GPIO_0) | (FT_GPIO_DIRECTION_OUT << FT_GPIO_1));
			if (FT_FAILED(ftStatus))
			{
				CMD_LOG(_T("\t%s FAILED! FT_EnableGPIO"), _T(__FUNCTION__));
				break;
			}

			/* Set the pins to LOW */
			ftStatus = FT_WriteGPIO(m_FTHandle,
				(FT_GPIO_VALUE_HIGH << FT_GPIO_0) | (FT_GPIO_VALUE_HIGH << FT_GPIO_1), /* mask - GPIO_0 and GPIO_1 are being written*/
				(FT_GPIO_VALUE_LOW << FT_GPIO_0) | (FT_GPIO_VALUE_LOW << FT_GPIO_1)); /* value for GPIO_0 and GPIO_1 */
			if (FT_FAILED(ftStatus))
			{
				CMD_LOG(_T("\t%s FAILED! FT_WriteGPIO"), _T(__FUNCTION__));
				break;
			}
		}
        break;
    }
    while (iNumTries++ < 1); //while (iNumTries++ < 3);


    if (ftStatus != FT_OK || m_FTHandle == NULL) 
    {
        if (ftStatus == FT_DEVICE_NOT_FOUND) 
        {
            GUI_LOG(_T("Device not connected or driver not installed!"));
        } 
        else 
        {
            GUI_LOG(_T("Failed looking for device, FT_STATUS 0x%x!"), ftStatus);
        }

        Cleanup();
        return FALSE;
    }
	m_dwDeviceIndex = dwDeviceIndex;
	GUI_LOG(_T("Device with %s [%s] opened! Device[%d]!"), 
		a_eTypeOpenBy == EOPEN_BY_DESC ? _T("Description") : 
		a_eTypeOpenBy == EOPEN_BY_SERIAL ? _T("Serial Number") :
		a_eTypeOpenBy == EOPEN_BY_INDEX ? _T("Index") : _T(""),
		wstrOpenBy.c_str(), dwDeviceIndex);

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::Cleanup
//
// Summary
//      Uninitializes the driver
//
// Parameters
//      None
//
// Return Value
//      None
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

VOID CDriverInterface::Cleanup()
{
    if (m_FTHandle)
    {
        FT_Close(m_FTHandle);
        m_FTHandle = NULL;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::GetEP
//
// Summary
//      Get number of endpoints and endpoint IDs
//
// Parameters
//      a_pucNumReadEP  - Pointer to contain the number of READ endpoints
//      a_pucNumWriteEP - Pointer to contain the number of WRITE endpoints
//      a_pucReadEP     - Pointer to an array containing the IDs of the READ endpoints
//      a_pucWriteEP    - Pointer to an array containing the IDs of the WRITE endpoints
//
// Return Value
//      TRUE if successful, FALSE otherwise
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CDriverInterface::GetEP(
    PUCHAR a_pucNumReadEP, 
    PUCHAR a_pucNumWriteEP, 
    PUCHAR a_pucReadEP, 
    PUCHAR a_pucWriteEP
    )
{
    FT_DEVICE_DESCRIPTOR DeviceDescriptor;
    FT_CONFIGURATION_DESCRIPTOR ConfigurationDescriptor;
    FT_INTERFACE_DESCRIPTOR InterfaceDescriptor;
    FT_PIPE_INFORMATION Pipe;
    BOOL bResult = FALSE;
    FT_STATUS ftResult = FT_OK;


    ZeroMemory(&DeviceDescriptor, sizeof(FT_DEVICE_DESCRIPTOR));
    ZeroMemory(&ConfigurationDescriptor, sizeof(FT_CONFIGURATION_DESCRIPTOR));
    ZeroMemory(&InterfaceDescriptor, sizeof(FT_INTERFACE_DESCRIPTOR));
    ZeroMemory(&Pipe, sizeof(FT_PIPE_INFORMATION));

    // Get configuration descriptor to determine the number of interfaces in the configuration
    ftResult = FT_GetDeviceDescriptor(m_FTHandle, &DeviceDescriptor);
    if (FT_FAILED(ftResult))
    {
        return FALSE;
    }
    if (DeviceDescriptor.bcdUSB < 0x0300)
    {
        m_bIsUsb3 = FALSE;
    }

    // Get configuration descriptor to determine the number of interfaces in the configuration
    ftResult = FT_GetConfigurationDescriptor(m_FTHandle, &ConfigurationDescriptor);
    if (FT_FAILED(ftResult))
    {
        return FALSE;
    }

    // Get interface descriptor to determine the number of endpoints in the interface
    UCHAR ucEPOffset = 0;
    UCHAR ucInterfaceIndex = 1;

    if (ConfigurationDescriptor.bNumInterfaces == 1)
    {
        ftResult = FT_GetInterfaceDescriptor(m_FTHandle, 0, &InterfaceDescriptor);
        ucEPOffset = 2;
        ucInterfaceIndex = 0;
    }
    else //if (ConfigurationDescriptor.bNumInterfaces == 2)
    {
        ftResult = FT_GetInterfaceDescriptor(m_FTHandle, 1, &InterfaceDescriptor);
    }
    if (FT_FAILED(ftResult))
    {
        return FALSE;
    }

    // Get a pair of endpoints for bulk transfer
    *a_pucNumReadEP = 0;
    *a_pucNumWriteEP = 0;
    for (UCHAR i=ucEPOffset; i<InterfaceDescriptor.bNumEndpoints; i++)
    {
        ftResult = FT_GetPipeInformation(m_FTHandle, ucInterfaceIndex, i, &Pipe);
        if (FT_FAILED(ftResult))
        {
            return FALSE;
        }

        if (FT_IS_READ_PIPE(Pipe.PipeId))
        {
            a_pucReadEP[*a_pucNumReadEP] = Pipe.PipeId;
            (*a_pucNumReadEP)++;
        }
        else
        {
            a_pucWriteEP[*a_pucNumWriteEP] = Pipe.PipeId;
            (*a_pucNumWriteEP)++;
        }
    }

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::GetVIDPID
//
// Summary
//      Gets the device vendor ID and product ID
//
// Parameters
//      a_puwVID    - Pointer to contain the VID
//      a_puwPID    - Pointer to contain the PID
//
// Return Value
//      TRUE if successful, FALSE otherwise
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CDriverInterface::GetVIDPID(
    PUSHORT a_puwVID, 
    PUSHORT a_puwPID
    )
{
    return FT_GetVIDPID(
        m_FTHandle, 
        a_puwVID, 
        a_puwPID
        ) == FT_OK ? TRUE : FALSE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::GetFirmwareVersion
//
// Summary
//      Gets the device vendor ID and product ID
//
// Parameters
//      pulFirmwareVersion    - Pointer to contain the firmware version
//
// Return Value
//      TRUE if successful, FALSE otherwise
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CDriverInterface::GetFirmwareVersion(
    PULONG pulFirmwareVersion
    )
{
    return FT_GetFirmwareVersion(
        m_FTHandle, 
        pulFirmwareVersion
        ) == FT_OK ? TRUE : FALSE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::GetInterfaceType
//
// Summary
//      Gets the device vendor ID and product ID
//
// Parameters
//      pulFirmwareVersion    - Pointer to contain the firmware version
//
// Return Value
//      TRUE if successful, FALSE otherwise
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CDriverInterface::GetInterfaceType(
	DWORD dwIndex,
	LPDWORD lpdwType
	)
{
	return FT_GetDeviceInfoDetail(
		dwIndex,
		NULL,
		lpdwType,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
		) == FT_OK ? TRUE : FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::WritePipe
//
// Summary
//      Writes specified data to a specified endpoint
//
// Parameters
//      a_ucPipeID              - Indicates the endpoint where data will be sent
//      a_pucBuffer             - Contains the data that will be sent
//      a_ulBufferLength        - Specifies the number of bytes of data that will be sent
//      a_pulLengthTransferred  - Pointer to contain the number of bytes sent
//
// Return Value
//      TRUE if successful, FALSE otherwise
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

FT_STATUS CDriverInterface::WritePipe(
    UCHAR a_ucPipeID,
    PUCHAR a_pucBuffer,
    ULONG a_ulBufferLength,
    PULONG a_pulLengthTransferred,
    LPOVERLAPPED a_pOverlapped
    )
{
    return FT_WritePipe(
        m_FTHandle,
        a_ucPipeID,
        a_pucBuffer,
        a_ulBufferLength,
        a_pulLengthTransferred,
        a_pOverlapped
        );
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::ReadPipe
//
// Summary
//      Read data from a specified endpoint
//
// Parameters
//      a_ucPipeID              - Indicates the endpoint where data will be received
//      a_pucBuffer             - Contains the pointer that will contain the data received
//      a_ulBufferLength        - Specifies the number of bytes of data that will be received
//      a_pulLengthTransferred  - Pointer to contain the number of bytes received
//
// Return Value
//      TRUE if successful, FALSE otherwise
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

FT_STATUS CDriverInterface::ReadPipe(
    UCHAR a_ucPipeID,
    PUCHAR a_pucBuffer,
    ULONG a_ulBufferLength,
    PULONG a_pulLengthTransferred,
    LPOVERLAPPED a_pOverlapped
    )
{
    return FT_ReadPipe(
        m_FTHandle,
        a_ucPipeID,
        a_pucBuffer,
        a_ulBufferLength,
        a_pulLengthTransferred,
        a_pOverlapped
        );
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::GetOverlappedResult
//
// Summary
//
// Parameters
//      a_pOverlapped
//      a_pulBytesTransferred
//
// Return Value
//      FT_STATUS
//
////////////////////////////////////////////////////////////////////////////////////////////////////
FT_STATUS CDriverInterface::GetOverlappedResult(
    LPOVERLAPPED a_pOverlapped,
    PULONG a_pulBytesTransferred,
    BOOL a_bWait
    )
{
    return FT_GetOverlappedResult(
        m_FTHandle,
        a_pOverlapped,
        a_pulBytesTransferred,
        a_bWait
        );
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::InitializeOverlapped
//
// Summary
//
// Parameters
//      a_pOverlapped
//
// Return Value
//      FT_STATUS
//
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CDriverInterface::InitializeOverlapped(
    LPOVERLAPPED a_pOverlapped
    )
{
    return (FT_InitializeOverlapped(
        m_FTHandle,
        a_pOverlapped
        ) == FT_OK ? TRUE : FALSE);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::ReleaseOverlapped
//
// Summary
//
// Parameters
//      a_pOverlapped
//
// Return Value
//      FT_STATUS
//
////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CDriverInterface::ReleaseOverlapped(
    LPOVERLAPPED a_pOverlapped
    )
{
    FT_ReleaseOverlapped(
        m_FTHandle,
        a_pOverlapped
        );
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::FlushPipe
//
// Summary
//      Flushes data from the EPC
//
// Parameters
//      a_ucPipeID              - Indicates the endpoint that will be flushed
//
// Return Value
//      TRUE if successful, FALSE otherwise
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CDriverInterface::FlushPipe(
    _In_  UCHAR a_ucPipeID
    )
{
    return FT_FlushPipe(
        m_FTHandle,
        a_ucPipeID
        ) == FT_OK ? TRUE : FALSE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::AbortPipe
//
// Summary
//      Aborts any pending write or read requests
//
// Parameters
//      a_ucPipeID              - Indicates the endpoint that will be aborted
//
// Return Value
//      TRUE if successful, FALSE otherwise
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CDriverInterface::AbortPipe(
    _In_  UCHAR a_ucPipeID
    )
{
	FT_STATUS ftStatus;
	UINT32 u32Gpio = (FT_IS_READ_PIPE(a_ucPipeID)) ? FT_GPIO_0 : FT_GPIO_1;
	
	if (m_FIFOMode == CONFIGURATION_FIFO_MODE_600)
	{
		ftStatus = FT_AbortPipe(
			m_FTHandle,
			a_ucPipeID
			);
		return TRUE;
	}

	ftStatus = FT_WriteGPIO(m_FTHandle,
		FT_GPIO_VALUE_HIGH << u32Gpio,
		FT_GPIO_VALUE_HIGH << u32Gpio);

	if (FT_FAILED(ftStatus))
	{
		return FALSE;
	}

	ftStatus = FT_AbortPipe(
		m_FTHandle,
		a_ucPipeID
		);

	if (FT_FAILED(ftStatus))
	{
		return FALSE;
	}

	ftStatus = FT_WriteGPIO(m_FTHandle,
		FT_GPIO_VALUE_HIGH << u32Gpio,
		FT_GPIO_VALUE_LOW << u32Gpio);

	if (FT_FAILED(ftStatus))
	{
		return FALSE;
	}

	return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::StartStreamPipe
//
// Summary
//      Starts streaming for the specified pipe
//
// Parameters
//      a_ucPipeID              - Indicates the endpoint
//      a_ulStreamSize          - Indicates the size to be sent
//
// Return Value
//      TRUE if successful, FALSE otherwise
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CDriverInterface::StartStreamPipe(
    _In_  UCHAR a_ucPipeID,
    _In_  ULONG a_ulStreamSize
    )
{
    return FT_SetStreamPipe(
        m_FTHandle,
        FALSE,
        FALSE,
        a_ucPipeID,
        a_ulStreamSize
    ) == FT_OK ? TRUE : FALSE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::StopStreamPipe
//
// Summary
//      Stops streaming for the specified pipe
//
// Parameters
//      a_ucPipeID              - Indicates the endpoint
//
// Return Value
//      TRUE if successful, FALSE otherwise
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CDriverInterface::StopStreamPipe(
    _In_  UCHAR a_ucPipeID
    )
{
    return FT_ClearStreamPipe(
        m_FTHandle,
        FALSE,
        FALSE,
        a_ucPipeID
    ) == FT_OK ? TRUE : FALSE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CDriverInterface::IsDevicePath
//
// Summary
//      Uninitializes the driver
//
// Parameters
//      None
//
// Return Value
//      None
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CDriverInterface::IsDevicePath(CONST CHAR* pucDevicePath)
{
	FT_STATUS ftResult;

	ftResult = FT_IsDevicePath(m_FTHandle, pucDevicePath);
	if (FT_FAILED(ftResult))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL CDriverInterface::IsUsb3()
{
    if (!m_bIsUsb3)
    {
        APP_LOG(_T("Warning: Device is connected using USB 2 cable and/or through a USB 2 host controller!"));
    }

    return m_bIsUsb3;
}

VOID CDriverInterface::CyclePort()
{
    FT_CycleDevicePort(m_FTHandle);
}

BOOL CDriverInterface::GetDriverVersion(LPDWORD pDriverVersion)
{
	FT_STATUS ftResult;

	ftResult = FT_GetDriverVersion(m_FTHandle, pDriverVersion);
	if (FT_FAILED(ftResult))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL CDriverInterface::GetLibraryVersion(LPDWORD pLibraryVersion)
{
	FT_STATUS ftResult;

	ftResult = FT_GetLibraryVersion(pLibraryVersion);
	if (FT_FAILED(ftResult))
	{
		return FALSE;
	}

	return TRUE;
}

