/*
 * FT600 Data Streamer Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#include "stdafx.h"
#include "APP_Logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <share.h>



CLogger *g_pLogger = NULL;



////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CLogger::CLogger
//      This class is for debug messages to the debug console and to file. 
//      The logging to file is disabled by default. 
//      To enable it, enable LOG_TO_FILE macro in APP_Logger.h.
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

CLogger::CLogger(LPCTSTR a_szFileName, LPCTSTR a_szDialogTitle):
    m_bGUI(FALSE),
    m_pLogFile(NULL),
    m_hEditText(NULL),
    m_hEditTextWriteRate(NULL),
    m_hEditTextReadRate(NULL)
{
    __try
    {
        ::InitializeCriticalSection(&m_cs);
    }
    __finally {}

    if (a_szDialogTitle)
    {
        HWND hConsoleDlg = FindWindow(NULL, a_szDialogTitle);
        m_hEditText = GetDlgItem(hConsoleDlg, IDC_RICHEDIT_OUTPUT);
        m_bGUI = TRUE;

        m_hEditTextWriteRate = GetDlgItem(hConsoleDlg, IDC_EDIT_RATE_EP02);
        m_hEditTextReadRate = GetDlgItem(hConsoleDlg, IDC_EDIT_RATE_EP82);
    }

#ifdef LOG_TO_FILE
    if (a_szFileName)
    {
        m_pLogFile = _tfsopen(a_szFileName, _T("a+"), _SH_DENYWR);
        if (m_pLogFile)
        {
            fseek(m_pLogFile, 0, SEEK_END);
            long lSize = ftell(m_pLogFile);
            if (lSize >= MAX_LOG_FILE_SIZE)
            {
                InternalHandleMaxFileSizeReached(a_szFileName, TRUE);
            }
        }

        _tcscpy_s(m_szFileName, a_szFileName);
    }
#endif // LOG_TO_FILE
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CLogger::~CLogger
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

CLogger::~CLogger()
{
    CMD_LOG_NOTIME(_T("\n\n"));

    if (m_pLogFile)
    {
        fseek(m_pLogFile, 0, SEEK_END);
        long lSize = ftell(m_pLogFile);
        if (lSize >= MAX_LOG_FILE_SIZE)
        {
            InternalHandleMaxFileSizeReached(m_szFileName, FALSE);
        }

        fclose(m_pLogFile);
        m_pLogFile = NULL;
    }

    ::DeleteCriticalSection(&m_cs);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CLogger::Log
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

VOID CLogger::Log(BOOL a_bNoTime, BOOL a_bConsole, BOOL a_bUI, LPCTSTR a_format, ...)
{
    ::EnterCriticalSection(&m_cs);


    ZeroMemory(m_aucLineBuffer, sizeof(m_aucLineBuffer));

    va_list argptr;
    va_start(argptr, a_format);
    _vstprintf_s(m_aucLineBuffer, a_format, argptr);
    va_end(argptr);

    _tcscat_s(m_aucLineBuffer, _T("\r\n"));


    // Display to console
    if (a_bConsole)
    {
        _tprintf(_T("%s"), m_aucLineBuffer);
    }

    // Display to UI edit text
    if (a_bUI)
    {
        if (m_bGUI)
        {
            // SendMessage hangs when using 100 as payload length
            // Use SendMessageTimeout to prevent hang
            DWORD dwRet = (DWORD)SendMessageTimeout(m_hEditText, EM_SETSEL, (WPARAM)-1, (LPARAM)-1, 0, 100, NULL);
            if (dwRet)
            {
                SendMessageTimeout(m_hEditText, EM_REPLACESEL, FALSE, (LPARAM)(LPCTSTR)m_aucLineBuffer, 0, 100, NULL);
                SendMessageTimeout(m_hEditText, EM_LINESCROLL, 0, 2, 0, 100, NULL);
            }
        }
    }

    // Everything is logged in the log file
#ifdef LOG_TO_FILE
    if (m_pLogFile)
    {
        if (a_bNoTime)
        {
            _ftprintf_s(m_pLogFile, m_aucLineBuffer);
        }
        else
        {
            SYSTEMTIME st = {0};
            GetLocalTime(&st);
            _ftprintf_s(m_pLogFile, _T("[%d-%02d-%02d %02d:%02d:%02d:%03d][%06u] %s"), 
                st.wYear, st.wMonth, st.wDay, st.wHour,st.wMinute,st.wSecond, st.wMilliseconds, 
                ::GetCurrentThreadId(), m_aucLineBuffer);
        }

        fflush(m_pLogFile);
    }
#endif // LOG_TO_FILE

    ::LeaveCriticalSection(&m_cs);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CLogger::InternalHandleMaxFileSizeReached
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

void CLogger::InternalHandleMaxFileSizeReached(LPCTSTR a_szFileName, BOOL a_bOpenNewFile)
{
    TCHAR szNewFileName[MAX_PATH] = {0};
    SYSTEMTIME st = {0};


    // Close file
    fclose(m_pLogFile);
    m_pLogFile = NULL;

    // Rename file
    GetLocalTime(&st);
    _sntprintf_s(szNewFileName, sizeof(szNewFileName)/sizeof(szNewFileName[0]), _T("%s-%04d%02d%02d-%02d%02d%02d%03d.txt"),
        a_szFileName, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    int ret = _trename(a_szFileName, szNewFileName);
    if (ret != 0)
    {
        _tprintf(_T("CLogger::InternalHandleMaxFileSizeReached _trename failed! %d\n"), errno);
    }
    _tprintf(_T("CLogger::InternalHandleMaxFileSizeReached _trename\n"));

    // Open new file
    if (a_bOpenNewFile)
    {
        m_pLogFile = _tfsopen(a_szFileName, _T("a+"), _SH_DENYWR);
    }
}


void CLogger::LogRate(ULONG a_ulRate, BOOL bWrite)
{
    static TCHAR szRate[32] = {0};
    ULONG ulRateLen = 0;
    DWORD dwRet;


    _stprintf_s(szRate, _T("%d"), a_ulRate);
    ulRateLen = _tcslen(szRate);

    if (bWrite)
    {
        dwRet = (DWORD)SendMessageTimeout(m_hEditTextWriteRate, EM_SETSEL, (WPARAM)-1, (LPARAM)-1, 0, ulRateLen, NULL);
        if (dwRet)
        {
            SendMessageTimeout(m_hEditTextWriteRate, WM_SETTEXT , FALSE, (LPARAM)(LPCTSTR)szRate, 0, ulRateLen, NULL);
        }
    }
    else
    {
        dwRet = (DWORD)SendMessageTimeout(m_hEditTextReadRate, EM_SETSEL, (WPARAM)-1, (LPARAM)-1, 0, ulRateLen, NULL);
        if (dwRet)
        {
            SendMessageTimeout(m_hEditTextReadRate, WM_SETTEXT , FALSE, (LPARAM)(LPCTSTR)szRate, 0, ulRateLen, NULL);
        }
    }
}


