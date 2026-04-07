/*
 * FT600 API Usage Demo App
 *
 * Copyright (C) 2016 FTDI Chip
 *
 */

#include "stdafx.h"
#include <dbt.h>
#include <process.h> // for _beginthreadex()



typedef unsigned (__stdcall *PTHREAD_START) (void *);

static  HDEVNOTIFY  RegisterHotPlug(HWND hWnd);
static  VOID        UnregisterHotPlug(HDEVNOTIFY hDeviceNotify);
static  HWND        CreateInvinsibleWindow();
static  VOID        DestroyInvinsibleWindow(HWND hWnd);
static  VOID        RunInvinsibleWindowLoop();
LRESULT CALLBACK    HandleInvinsibleWindowMessaging(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);



///////////////////////////////////////////////////////////////////////////////////
// Demonstrates hot plugging using Windows APIs
// RegisterDeviceNotification
// UnregisterDeviceNotification
///////////////////////////////////////////////////////////////////////////////////
BOOL HotPlugTest()
{
    //
    // Create a window since Hot Plugging does not work with console applications
    // Windows does not allow us to register for Hot Plug events if we don't have a window
    //
    HWND hWnd = CreateInvinsibleWindow();
    if (!hWnd)
    {
        return FALSE;
    }


    //
    // Register notification of plugin/plugout for FTDI devices
    //
    HDEVNOTIFY hDeviceNotify = RegisterHotPlug(hWnd);
    if (!hDeviceNotify)
    {
        DestroyInvinsibleWindow(hWnd);
        return FALSE;
    }

    CMD_LOG(_T("\tThis test needs user interaction.\n"));
    CMD_LOG(_T("\tPlease unplug device!\n"));
    CMD_LOG(_T("\tWaiting for user to unplug device...\n"));

    RunInvinsibleWindowLoop();
    UnregisterHotPlug(hDeviceNotify);
    DestroyInvinsibleWindow(hWnd);
    return TRUE;
}



static HDEVNOTIFY RegisterHotPlug(HWND hWnd)
{
    HDEVNOTIFY hDeviceNotify = NULL;
    GUID DeviceGUID[2] = {0};


    // Register notification for all FTDI devices
	memcpy(&DeviceGUID[0], &GUID_DEVINTERFACE_FOR_D3XX, sizeof(GUID));

    DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
    ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
    NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    memcpy((PUCHAR)&NotificationFilter.dbcc_classguid, (PUCHAR)&DeviceGUID[0], sizeof(DeviceGUID[0]));

    hDeviceNotify = RegisterDeviceNotification(hWnd, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (NULL == hDeviceNotify) 
    {
        return NULL;
    }

    return hDeviceNotify;
}

static VOID UnregisterHotPlug(HDEVNOTIFY hDeviceNotify)
{
    if (hDeviceNotify)
    {
        UnregisterDeviceNotification(hDeviceNotify);
        hDeviceNotify = NULL;
    }
}

static VOID ProcessHotPlugPlugin()
{
    PostQuitMessage(0);
}

static VOID ProcessHotPlugPlugout()
{
    CMD_LOG(_T("\tPlease plug device!\n"));
    CMD_LOG(_T("\tWaiting for user to plug device...\n"));
}



static HWND CreateInvinsibleWindow()
{
    const wchar_t winClass[] = L"SampleHotPlugWindow";
    const wchar_t winTitle[] = L"SampleHotPlug";


    WNDCLASS wc = {0};
    wc.lpfnWndProc   = HandleInvinsibleWindowMessaging;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = winClass;
    wc.lpszMenuName  = NULL;

    if (!RegisterClass(&wc))
    {
        DWORD dwError = GetLastError();
        if (dwError != ERROR_CLASS_ALREADY_EXISTS)
        {
            return FALSE;
        }
    }

    HWND hWnd = CreateWindow(
        winClass,
        winTitle,
        WS_OVERLAPPED | WS_CAPTION,
        0,
        0,
        100,
        100,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL
        );
    if (hWnd == NULL)
    {
        return NULL;
    }

    ShowWindow(hWnd, SW_HIDE);
    UpdateWindow(hWnd);

    return hWnd;
}

static VOID DestroyInvinsibleWindow(HWND hWnd)
{
    if (hWnd)
    {
        DestroyWindow(hWnd);
    }
}

static VOID RunInvinsibleWindowLoop()
{
    MSG msg; 
    int lRet;

    while((lRet = GetMessage(&msg, NULL, 0, 0)) != 0)
    {
        if (lRet == -1)
        {
            break;
        }
        TranslateMessage(&msg); 
        DispatchMessage(&msg);
    }
}

LRESULT CALLBACK HandleInvinsibleWindowMessaging(
    HWND hwnd, 
    UINT uMsg, 
    WPARAM wParam, 
    LPARAM lParam
    )
{
    switch (uMsg)
    {
        case WM_DEVICECHANGE:
        {
            PDEV_BROADCAST_DEVICEINTERFACE pHandle = (PDEV_BROADCAST_DEVICEINTERFACE) lParam;

            switch (wParam)
            {
                case DBT_DEVICEARRIVAL:
                {
					CMD_LOG(_T("\tAn FTDI device was plugged!\n"));
					CMD_LOG(_T("\t%s\n"), pHandle->dbcc_name);
					
                    //
                    // Check if the FTDI device that was plugged is FT60X device
                    //
                    if ((wcsstr(pHandle->dbcc_name, L"0403")) && 
                        (wcsstr(pHandle->dbcc_name, L"601F") || 
                         wcsstr(pHandle->dbcc_name, L"601E"))
                        )
                    {
                        ProcessHotPlugPlugin();
                    }

                    break;
                }
                case DBT_DEVICEREMOVECOMPLETE:
                {
                    CMD_LOG(_T("\tAn FTDI device was unplugged!\n"));
					CMD_LOG(_T("\t%s\n"), pHandle->dbcc_name);

                    //
                    // Check if the FTDI device that was unplugged is FT60X device
                    //
#if 1
                    if ((wcsstr(pHandle->dbcc_name, L"0403")) && 
                        (wcsstr(pHandle->dbcc_name, L"601F") || 
                         wcsstr(pHandle->dbcc_name, L"601E"))
                        )
#else
					std::wstring wstr(pHandle->dbcc_name);
					std::string str(wstr.begin(), wstr.end());
					if (FT_IsDevicePath(str.c_str()))
#endif
					{
                        ProcessHotPlugPlugout();
                    }

                    break;
                }
                default:
                {
                    break;
                }
            }
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


