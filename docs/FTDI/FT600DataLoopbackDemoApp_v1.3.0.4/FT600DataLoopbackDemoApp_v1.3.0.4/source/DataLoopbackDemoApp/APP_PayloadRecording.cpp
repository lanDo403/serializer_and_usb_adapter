/*
 * FT600 Data Loopback Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#include "stdafx.h"
#include "APP_PayloadRecording.h"
#include "APP_Utils.h"



////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadRecorder::CPayloadRecorder
//      This file implements the saving of written data to a file. It also applies to read data. 
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

CPayloadRecorder::CPayloadRecorder(UCHAR a_ucEP):
    m_ucEP(a_ucEP),
    m_hFile(INVALID_HANDLE_VALUE)
{
    ZeroMemory(m_szFileName, sizeof(m_szFileName));
    ::InitializeCriticalSection(&m_cs);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadRecorder::~CPayloadRecorder
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

CPayloadRecorder::~CPayloadRecorder()
{
    Close();
    ::DeleteCriticalSection(&m_cs);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadRecorder::Write
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

BOOL CPayloadRecorder::Write(PUCHAR a_pucBuffer, ULONG a_ulBufferLength, PULONG a_pulActualBytesWritten)
{
    CUtils::CCriticalSectionHolder cs(&m_cs);
    BOOL bResult = FALSE;


    if (INVALID_HANDLE_VALUE == m_hFile)
    {
        Create();
    }

    bResult = ::WriteFile(m_hFile, a_pucBuffer, a_ulBufferLength, a_pulActualBytesWritten, NULL);
    if (!bResult)
    {
        DWORD dwError = ::GetLastError();
        if (ERROR_DISK_FULL == dwError)
        {
            _tprintf(_T("[EP%02x] CPayloadRecorder::Write(): WriteFile failed! ERROR_DISK_FULL\n"), m_ucEP);
        }
        else
        {
            _tprintf(_T("[EP%02x] CPayloadRecorder::Write(): WriteFile failed! %d\n"), m_ucEP, GetLastError());
        }
        return FALSE;
    }

    m_llFileSize.QuadPart += *a_pulActualBytesWritten;
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadRecorder::UpdateFileName
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

BOOL CPayloadRecorder::UpdateFileName(SYSTEMTIME *a_pTime)
{
    CUtils::CCriticalSectionHolder cs(&m_cs);
    if (!a_pTime)
    {
        return FALSE;
    }

    ZeroMemory(m_szFileName, sizeof(m_szFileName));
    _sntprintf_s(m_szFileName, sizeof(m_szFileName)/sizeof(m_szFileName[0]), 
        _T("%s\\%04d%02d%02d_%02d%02d%02d_PayloadEP%02x.txt"),
        PAYLOAD_RECORDER_DIRECTORY_NAME,
        a_pTime->wYear, a_pTime->wMonth, a_pTime->wDay, a_pTime->wHour, a_pTime->wMinute, a_pTime->wSecond,
        m_ucEP
        );

    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        Close();
    }

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadRecorder::Create
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

BOOL CPayloadRecorder::Create()
{
    CUtils::CCriticalSectionHolder cs(&m_cs);
    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        return TRUE;
    }

    BOOL bResult = CreateDirectory(PAYLOAD_RECORDER_DIRECTORY_NAME, NULL);
    if (!bResult)
    {
        DWORD dwError = GetLastError();
        if (dwError != ERROR_ALREADY_EXISTS)
        {
            _tprintf(_T("[0x%02x] CPayloadRecorder::Create(): CreateDirectory failed! %d\n"), m_ucEP, dwError);
            return FALSE;
        }
    }
 
    APP_LOG(_T("[EP%02x] %s"), m_ucEP, m_szFileName);
    m_hFile = ::CreateFile(m_szFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == m_hFile)
    {
        _tprintf(_T("[0x%02x] CPayloadRecorder::Create(): CreateFile failed! %d\n"), m_ucEP, GetLastError());
        return FALSE;
    }

    m_llFileSize.QuadPart = 0;
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadRecorder::Close
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

VOID CPayloadRecorder::Close()
{
    CUtils::CCriticalSectionHolder cs(&m_cs);
    if (INVALID_HANDLE_VALUE == m_hFile)
    {
        return;
    }

    ::CloseHandle(m_hFile);
    m_hFile = INVALID_HANDLE_VALUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadRecorder::GetFileName
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

BOOL CPayloadRecorder::GetFileName(LPTSTR a_szFileName, ULONG a_ulFileNameSize)
{
    CUtils::CCriticalSectionHolder cs(&m_cs);
    _tcscpy_s(a_szFileName, a_ulFileNameSize, m_szFileName);
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadRecorder::GetFileName
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

LPCTSTR CPayloadRecorder::GetFileName()
{
    return m_szFileName;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadRecorder::GetFileSize
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

BOOL CPayloadRecorder::GetFileSize(ULARGE_INTEGER *a_pullFileSize)
{
    a_pullFileSize->LowPart = ::GetFileSize(m_hFile, &a_pullFileSize->HighPart);
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadRecorder::PathFileExist
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

BOOL CPayloadRecorder::PathFileExist()
{
    return PathFileExists(m_szFileName);
}


