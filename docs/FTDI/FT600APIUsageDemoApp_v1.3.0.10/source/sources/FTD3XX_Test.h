/*
 * FT600 API Usage Demo App
 *
 * Copyright (C) 2016 FTDI Chip
 *
 */

#pragma once


///////////////////////////////////////////////////////////////////////////////////
// When user uses 245 FPGA image, device must be configure to 245 mode
// Enable this to use 245 mode.
// If not enabled, 600 mode (multichannel mode) will be used by default.
///////////////////////////////////////////////////////////////////////////////////
#if _245Mode
#define USE_245_MODE    1
#endif


///////////////////////////////////////////////////////////////////////////////////
// API tests
///////////////////////////////////////////////////////////////////////////////////

BOOL OpenCloseTest();
BOOL DescriptorTest();

BOOL ModifyChipConfigurationTest();
BOOL SetChipConfigurationTest();
BOOL ResetChipConfigurationTest();

BOOL LoopbackTest();
BOOL AsyncLoopbackTest();
BOOL MultiAsyncLoopbackTest();
BOOL MultiWriteAsyncLoopbackTest();
BOOL MultiReadAsyncLoopbackTest();
BOOL StreamingLoopbackTest();
BOOL MultiChannelAsyncLoopbackTest();
BOOL MultiThreadedMultiChannelLoopbackTest();

BOOL NotificationDataTest();
BOOL GPIOTest();
BOOL ReadTimeoutTest();
BOOL IdleTimeoutForSuspendTest();
BOOL BatteryChargingDetectionTest();

BOOL HotPlugTest();

BOOL OpenMultipleDevicesTest();
BOOL SimultaneousLoopbackOnMultipleDevicesTest();

BOOL SaveChipConfigurationTest();
BOOL RevertChipConfigurationTest();


///////////////////////////////////////////////////////////////////////////////////
// Helper functions
///////////////////////////////////////////////////////////////////////////////////

VOID  ShowConfiguration(CONST PVOID a_pvConfigurationData, BOOL a_bRead);
DWORD GetNumberOfDevicesConnected();
BOOL  GetDeviceSerialNumber(DWORD dwDeviceIndex, char* SerialNumber);
BOOL  GetDeviceDescription(DWORD dwDeviceIndex, char* ProductDescription);
BOOL  GetDevicesSerialNumbers(char** SerialNumbers);
BOOL  GetDevicesProductDescriptions(char** ProductDescriptions);
DWORD GetDevicesInfoList(PVOID *pptDevicesInfo);
DWORD GetDeviceInfoDetail(DWORD dwDeviceIndex, DWORD* Flags, DWORD* Type, DWORD *ID, char* SerialNumber, char* Description, PVOID Handle);
VOID  DisplayDeviceInfoDetail(DWORD dwNumDevices);
VOID  DisplayDeviceInfoList();
VOID  DisplaySerialNumbersOfDevicesConnected(DWORD dwNumDevices);
VOID  DisplayNumberOfDevicesConnected();
VOID  DisplayProductDescriptionOfDevicesConnected(DWORD dwNumDevices);

