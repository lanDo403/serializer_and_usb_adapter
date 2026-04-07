/*
* FT600 Function Tester
*
* Copyright (C) 2014 Richmond Umagat <richmond.umagat@ftdichip.com>
*
*/

#include "stdafx.h"
#include "FTD3XX_Logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <share.h>



CLogger *g_pLogger = NULL;

VOID InitLogger(LPCTSTR a_szFileName, LPCTSTR a_szDialogTitle, BOOL a_bAppend)
{
	if (!g_pLogger)
	{
		g_pLogger = new CLogger(a_szFileName, a_szDialogTitle, a_bAppend);
	}
}

VOID FreeLogger()
{
	if (g_pLogger)
	{
		delete g_pLogger;
		g_pLogger = NULL;
	}
}

CLogger::CLogger(LPCTSTR a_szFileName, LPCTSTR a_szDialogTitle, BOOL a_bAppend) :
m_bGUI(FALSE),
m_pLogFile(NULL)
{
	::InitializeCriticalSection(&m_cs);

#if 0
	if (a_szDialogTitle)
	{
		HWND hConsoleDlg = FindWindow(NULL, a_szDialogTitle);
		m_hEditText = GetDlgItem(hConsoleDlg, IDC_RICHEDIT_OUTPUT);
		m_bGUI = TRUE;
	}
#endif

#ifdef LOG_TO_FILE
	if (a_szFileName)
	{
		if (a_bAppend)
			m_pLogFile = _tfsopen(a_szFileName, _T("a+"), _SH_DENYWR);
		else
			m_pLogFile = _tfsopen(a_szFileName, _T("w+"), _SH_DENYWR);

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

VOID CLogger::Log(BOOL a_bNoTime, BOOL a_bConsole, BOOL a_bUI, LPCTSTR a_format, ...)
{
	::EnterCriticalSection(&m_cs);


	ZeroMemory(m_aucLineBuffer, sizeof(m_aucLineBuffer));

	va_list argptr;
	va_start(argptr, a_format);
	_vstprintf_s(m_aucLineBuffer, a_format, argptr);
	va_end(argptr);

#if 0
	_tcscat_s(m_aucLineBuffer, _T("\r\n"));
#endif

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
			SYSTEMTIME st = { 0 };
			GetLocalTime(&st);
			_ftprintf_s(m_pLogFile, _T("[%d-%02d-%02d %02d:%02d:%02d:%03d][%06u] %s"),
				st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
				::GetCurrentThreadId(), m_aucLineBuffer);
		}

		fflush(m_pLogFile);
	}
#endif // LOG_TO_FILE

	::LeaveCriticalSection(&m_cs);
}

void CLogger::InternalHandleMaxFileSizeReached(LPCTSTR a_szFileName, BOOL a_bOpenNewFile)
{
	TCHAR szNewFileName[MAX_PATH] = { 0 };
	SYSTEMTIME st = { 0 };


	// Close file
	fclose(m_pLogFile);
	m_pLogFile = NULL;

	// Rename file
	GetLocalTime(&st);
	_sntprintf_s(szNewFileName, sizeof(szNewFileName) / sizeof(szNewFileName[0]), _T("%s-%04d%02d%02d-%02d%02d%02d%03d.txt"),
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


