/*
 * FT600 Data Streamer Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#include "stdafx.h"
#include "UI_DataStreamer.h"
#include "UI_Manager.h"
#include "APP_Utils.h"



#ifdef _DEBUG
#define new DEBUG_NEW
#endif

HWND CUtils::CMFCConsole::m_hwndConsole = NULL; 

CFunctionTesterApp theApp;

BEGIN_MESSAGE_MAP(CFunctionTesterApp, CWinApp)
    ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()



////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CFunctionTesterApp::CFunctionTesterApp
//
// Summary
//      This is the main entry point of this Windows MFC-based application.
//
// Parameters
//
// Return Value
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

CFunctionTesterApp::CFunctionTesterApp()
{
}


static BOOL IsDemoApplicationRunning(LPCTSTR szEventInstanceName, BOOL bClose)
{
    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, szEventInstanceName);
    if (!hEvent)
    {
        return TRUE;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(hEvent);
        return TRUE;
    }

    if (bClose)
    {
        CloseHandle(hEvent);
    }

    return FALSE;
}

static BOOL IsDemoApplicationsRunning()
{
#if 0
#if 1
	//
	// Make sure only 1 instance of this application is running
	//
	if (IsDemoApplicationRunning(L"FT600DataStreamerDemoApp", FALSE))
	{
		return TRUE;
	}
#else
    //
    // Make sure other demo applications are not running
    //
    if (IsDemoApplicationRunning(L"FT600Demo", FALSE))
    {
        return TRUE;
    }

    //
    // D3XX 1.0.0.6 currently has issue when machine has jtagserver.exe from Altera running.
    // Make sure jtagserver.exe from Altera installation is not running.
    //
    {
        _tsystem( _T("taskkill /F /T /IM jtagserver.exe") );
    }
#endif
#endif

    return FALSE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CFunctionTesterApp::InitInstance
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

BOOL CFunctionTesterApp::InitInstance()
{
    CWinApp::InitInstance();


    //
    // Ensure other demo applications are not running
    // Ensure only 1 instance of the application is running
    //
    if (IsDemoApplicationsRunning())
    {
        return FALSE;
    }


    //
    // Initialize console debugging
    //
    CUtils::CMFCConsole::Initialize(APPLICATION_TITLE);
    CUtils::CMFCConsole::Show(FALSE);


    //
    // Initialize rich edit
    // Crash will occur if rich edit text is added but not initialized
    //
    AfxInitRichEdit2();

    // Create the shell manager, in case the dialog contains
    // any shell tree view or shell list view controls.
    CShellManager *pShellManager = new CShellManager;

    // Activate "Windows Native" visual manager for enabling themes in MFC controls
    CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

    // Standard initialization
    // If you are not using these features and wish to reduce the size
    // of your final executable, you should remove from the following
    // the specific initialization routines you do not need
    // Change the registry key under which our settings are stored
    // TODO: You should modify this string to be something appropriate
    // such as the name of your company or organization
    SetRegistryKey(_T("Local AppWizard-Generated Applications"));

    CUIManager dlg;
    m_pMainWnd = &dlg;
    INT_PTR nResponse = dlg.DoModal();
    if (nResponse == IDOK)
    {
        // TODO: Place code here to handle when the dialog is
        //  dismissed with OK
    }
    else if (nResponse == IDCANCEL)
    {
        // TODO: Place code here to handle when the dialog is
        //  dismissed with Cancel
    }
    else if (nResponse == -1)
    {
        TRACE(traceAppMsg, 0, "Warning: dialog creation failed, so application is terminating unexpectedly.\n");
        TRACE(traceAppMsg, 0, "Warning: if you are using MFC controls on the dialog, you cannot #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS.\n");
    }

    // Delete the shell manager created above.
    if (pShellManager != NULL)
    {
        delete pShellManager;
    }

    // Since the dialog has been closed, return FALSE so that we exit the
    //  application, rather than start the application's message pump.
    return FALSE;
}

