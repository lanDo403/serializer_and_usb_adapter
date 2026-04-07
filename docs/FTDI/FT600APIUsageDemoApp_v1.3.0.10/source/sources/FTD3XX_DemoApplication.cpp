/*
 * FT600 API Usage Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#include "stdafx.h"
#include "FTD3XX_Test.h"
#include <iostream>
#include <string>
#include <vector>
#include <VersionHelpers.h>
#include "FTD3XX_Logger.h"
using namespace std;



///////////////////////////////////////////////////////////////////////////////////
// To test support for multiple devices,
// Set SINGLE_DEVICE_TEST to 0 and connected the devices.
// Make sure both devices are connected to an FPGA
///////////////////////////////////////////////////////////////////////////////////
#define SINGLE_DEVICE_TEST 1


///////////////////////////////////////////////////////////////////////////////////
// Test cases
///////////////////////////////////////////////////////////////////////////////////

typedef BOOL (*FT60X_TEST_FUNCTION)();

typedef struct _FT60X_TEST_CASE
{
    FT60X_TEST_FUNCTION     m_fxn;
    string                  m_dsc;

} FT60X_TEST_CASE;


///////////////////////////////////////////////////////////////////////////////////
// Demonstrations of API usages and functionality
///////////////////////////////////////////////////////////////////////////////////
static std::vector<FT60X_TEST_CASE> SAMPLE_FT60X_USAGE =
{


#if SINGLE_DEVICE_TEST

    // demonstrates opening of a device handle
    { OpenCloseTest,                                "OpenCloseTest"                             },

    // demonstrates modifying current setting of chip configuration
	{ ModifyChipConfigurationTest,					"ModifyChipConfigurationTest"				},

    // demonstrates querying of USB descriptors
    { DescriptorTest,                               "DescriptorTest"                            },


    // demonstrates single channel data transfer loopback function using synchronous write and read
    { LoopbackTest,                                 "LoopbackTest"                              },

    // demonstrates single channel loopback function using asynchronous write and read
    { AsyncLoopbackTest,                            "AsyncLoopbackTest"                         },

    // demonstrates single channel loopback function using multiple asynchronous write and read
    { MultiAsyncLoopbackTest,                       "MultiAsyncLoopbackTest"                    },

    // demonstrates single channel loopback function using multiple asynchronous write
    { MultiWriteAsyncLoopbackTest,                  "MultiAsyncLoopbackTest2"                   },

    // demonstrates single channel loopback function using multiple asynchronous read
    { MultiReadAsyncLoopbackTest,                   "MultiAsyncLoopbackTest3"                   },

    // demonstrates single channel loopback function using synchronous write and read using streaming mode
    { StreamingLoopbackTest,                        "StreamingLoopbackTest"                     },

    // demonstrates multi channel loopback function using asynchronous write and read
    { MultiChannelAsyncLoopbackTest,                "MultiChannelAsyncLoopbackTest"             },

    // demonstrates multi threaded multi channel loopback function using synchronous write and read
    { MultiThreadedMultiChannelLoopbackTest,        "MultiThreadedMultiChannelLoopbackTest"     },

    // demonstrates reading from IN pipes using notification messaging
    { NotificationDataTest,                         "NotificationDataTest"                      },

	// demonstrates reading and setting GPIO pins
	{ GPIOTest,	  								    "GPIOTest"								    },

	// demonstrates reading battery charging detection status
	{ BatteryChargingDetectionTest,					"BatteryChargingDetectionTest"				},

	// demonstrates the usage of get and set timeout APIs.
	{ ReadTimeoutTest,                               "ReadTimeoutTest"                          },

	// demonstrates the usage of get and set Suspend timeout APIs.
	{ IdleTimeoutForSuspendTest,                     "IdleTimeoutForSuspendTest"                },
	
	// demonstrates setting of chip configuration from scratch
	{ SetChipConfigurationTest,						"SetChipConfigurationTest"					},

	// demonstrates resetting chip configuration
    { ResetChipConfigurationTest,                   "ResetChipConfigurationTest"                },

    // demonstrates USB hot plug, needs user interaction - user to plug and unplug device
    { HotPlugTest,                                  "HotPlugTest"                               },

#else // SINGLE_DEVICE_TEST

    // demonstrates opening of multiple FT60X devices
    { OpenMultipleDevicesTest,                      "OpenMultipleDevicesTest"                   },

    // demonstrates simultaenous loopback on multiple FT60X devices
    { SimultaneousLoopbackOnMultipleDevicesTest,    "SimultaneousLoopbackOnMultipleDevicesTest" },

#endif // SINGLE_DEVICE_TEST

};


VOID DisplayTestHostInfo()
{
#define BUF_SIZE 64
	TCHAR szComputerName[BUF_SIZE] = { 0 };
	TCHAR szUserName[BUF_SIZE] = { 0 };
	TCHAR szOSVersion[BUF_SIZE] = { 0 };
	DWORD dwSize = 0;
	BOOL bRet = FALSE;
	SYSTEMTIME stLocalTime = { 0 };
	OSVERSIONINFO osVersion = { 0 };


	// Get computer name
	dwSize = BUF_SIZE;
	bRet = ::GetComputerName(szComputerName, &dwSize);
	if (!bRet)
	{
		return;
	}
	if (dwSize >= BUF_SIZE)
	{
		return;
	}

	// Get user name
	dwSize = BUF_SIZE;
	bRet = ::GetUserName(szUserName, &dwSize);
	if (!bRet)
	{
		return;
	}
	if (dwSize >= BUF_SIZE)
	{
		return;
	}

	// Get local time
	::GetLocalTime(&stLocalTime);

	// Get OS version
	//if (IsWindows10OrGreater())
	//{
	//    _tcscpy_s(szOSVersion, _T("Windows 10 or Server"));
	//}
	//else 
	if (IsWindows8Point1OrGreater())
	{
		_tcscpy_s(szOSVersion, _T("Windows 8.1"));
	}
	else if (IsWindows8OrGreater())
	{
		_tcscpy_s(szOSVersion, _T("Windows 8"));
	}
	else if (IsWindows7SP1OrGreater())
	{
		_tcscpy_s(szOSVersion, _T("Windows 7 SP1"));
	}
	else if (IsWindows7OrGreater())
	{
		_tcscpy_s(szOSVersion, _T("Windows 7"));
	}
	else //if (IsWindows10OrGreater())
	{
		_tcscpy_s(szOSVersion, _T("Windows 10"));
	}

	CMD_LOG(_T("**************************************************************\n"));
	CMD_LOG(_T("TEST SUITE,%s\n"), _T("FT60X API USAGE DEMO"));
	CMD_LOG(_T("REVISION DATE,%s %s\n"), _T(__DATE__), _T(__TIME__));
	CMD_LOG(_T("WORKSTATION,%s\n"), szComputerName);
	CMD_LOG(_T("OSVERSION,%s\n"), szOSVersion);
	CMD_LOG(_T("OPERATOR,%s\n"), szUserName);
	CMD_LOG(_T("DATE,%d-%02d-%02d\n"), stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay);
	CMD_LOG(_T("TIME,%02d:%02d:%02d\n"), stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond);
	CMD_LOG(_T("**************************************************************\n"));

	CMD_LOG(_T("\n"));
	CMD_LOG(_T("\n"));
	CMD_LOG(_T("\n"));
}

static ULONGLONG g_ullStartTime = 0;

VOID DisplayTestStart(LPCTSTR a_szDescription)
{
	SYSTEMTIME stStartTime = { 0 };
	GetLocalTime(&stStartTime);
	CMD_LOG(_T("--------------------------------------------------------------\n"));
	CMD_LOG(_T("%s\n"), a_szDescription);
	CMD_LOG(_T("--------------------------------------------------------------\n"));
	CMD_LOG(_T("Start time,%d-%02d-%02d %02d:%02d:%02d\n"), stStartTime.wYear, stStartTime.wMonth, stStartTime.wDay, stStartTime.wHour, stStartTime.wMinute, stStartTime.wSecond);
	CMD_LOG(_T("\n"));

	FILETIME ftStartTime = { 0 };
	::SystemTimeToFileTime(&stStartTime, &ftStartTime);
	ULARGE_INTEGER uliStartTime = { 0 };
	uliStartTime.LowPart = ftStartTime.dwLowDateTime;
	uliStartTime.HighPart = ftStartTime.dwHighDateTime;
	g_ullStartTime = uliStartTime.QuadPart;
}

VOID DisplayTestStop(BOOL a_bStatus)
{
	SYSTEMTIME stStopTime = { 0 };
	GetLocalTime(&stStopTime);
	CMD_LOG(_T("\n"));
	CMD_LOG(_T("Stop time,%d-%02d-%02d %02d:%02d:%02d\n"), stStopTime.wYear, stStopTime.wMonth, stStopTime.wDay, stStopTime.wHour, stStopTime.wMinute, stStopTime.wSecond);

	FILETIME ftStopTime = { 0 };
	::SystemTimeToFileTime(&stStopTime, &ftStopTime);
	ULARGE_INTEGER uliStopTime = { 0 };
	uliStopTime.LowPart = ftStopTime.dwLowDateTime;
	uliStopTime.HighPart = ftStopTime.dwHighDateTime;
	ULONGLONG ullStopTime = uliStopTime.QuadPart;

	ULONGLONG ullDiffTimeSeconds = (ullStopTime - g_ullStartTime) / 10000000;

	if (ullDiffTimeSeconds < 60)
	{
		CMD_LOG(_T("Duration,%I64d seconds\n"), ullDiffTimeSeconds);
	}
	else
	{
		CMD_LOG(_T("Duration,%I64d minutes\n"), ullDiffTimeSeconds / 60);
	}

	CMD_LOG(_T("Status,%s\n"), a_bStatus ? _T("SUCCESS") : _T("FAILED"));

	CMD_LOG(_T("\n"));
	CMD_LOG(_T("\n"));
}

VOID DisplayTestSummary(DWORD a_dwNumFails, DWORD a_dwNumSuccess)
{
	CMD_LOG(_T("--------------------------------------------------------------\n"));
	CMD_LOG(_T("Summary\n"));
	CMD_LOG(_T("--------------------------------------------------------------\n"));
	CMD_LOG(_T("Summary Log Counts,[ Fails (%d); Success (%d) ]\n"), a_dwNumFails, a_dwNumSuccess);

	CMD_LOG(_T("\n"));
	CMD_LOG(_T("\n"));
}


///////////////////////////////////////////////////////////////////////////////////
// Main function
//
// NOTES:
//
// 1. Please enable USE_245_MODE in FTD3XX_Test.h if using 245 FPGA
// 2. Please set SINGLE_DEVICE_TEST to 0 to test multiple devices support.
//    Make sure 2 EVBs connected to FPGAs are connected when enabling this test
// 3. Please use Visual Studio 2013 for development machine.
//    To run test application in test machine, please install 
//    Visual C++ Redistributable Packages for Visual Studio 2013.
//    http://www.microsoft.com/en-US/download/details.aspx?id=40784
//
///////////////////////////////////////////////////////////////////////////////////
int _tmain(int argc, _TCHAR* argv[])
{
    BOOL bResult = FALSE;
    BOOL bStressTest = FALSE;
	DWORD dwNumFails = 0;
	DWORD dwNumSuccess = 0;


	INIT_LOGGER(L"FT600APIUsageDemoApp.txt", NULL, FALSE);
	DisplayTestHostInfo();
	SaveChipConfigurationTest();

    do
    {
		for (auto & i : SAMPLE_FT60X_USAGE)
        {
			DisplayTestStart(wstring(i.m_dsc.begin(), i.m_dsc.end()).c_str());
            bResult = i.m_fxn();
			DisplayTestStop(bResult);

            if (!bResult)
            {
				dwNumFails++;
				goto exit;
            }
			
			dwNumSuccess++;
            Sleep(1000);
		}
    }
    while (bStressTest);


exit:

	RevertChipConfigurationTest();
	DisplayTestSummary(dwNumFails, dwNumSuccess);
	
	FREE_LOGGER();

	system("pause");
    return 0;
}

