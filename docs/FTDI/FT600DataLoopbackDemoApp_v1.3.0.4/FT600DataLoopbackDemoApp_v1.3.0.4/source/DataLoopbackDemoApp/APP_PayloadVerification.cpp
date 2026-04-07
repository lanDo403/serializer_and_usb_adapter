/*
 * FT600 Data Loopback Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#include "stdafx.h"
#include "APP_PayloadVerification.h"
#include "APP_Utils.h"



#define MAX_FILE_SIZE (100*1024*1024) //100mb



////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadVerifier::CPayloadVerifier
//      This file implements the comparing of the file containing the written data with the file containing the read data. 
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

CPayloadVerifier::CPayloadVerifier():
        m_hFileEPWrite(INVALID_HANDLE_VALUE),
        m_hFileEPRead(INVALID_HANDLE_VALUE)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadVerifier::~CPayloadVerifier
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

CPayloadVerifier::~CPayloadVerifier()
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadVerifier::CompareFiles
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

BOOL CPayloadVerifier::CompareFiles(LPCTSTR a_szFileNameEPWrite, LPCTSTR a_szFileNameEPRead, BOOL *a_pbFilesExist, BOOL a_bCompareMin)
{
    BOOL bResult = FALSE;
    LARGE_INTEGER llFileSize = {0};
    ULONG ulBufferLength = 65536;
    ULONG ulActualBytesTransferred = 0;
    UCHAR ucBufferEPWrite[65536] = {0};
    UCHAR ucBufferEPRead[65536] = {0};


    bResult = OpenFiles(a_szFileNameEPWrite, a_szFileNameEPRead);
    if (!bResult)
    {
        if (a_pbFilesExist)
        {
            *a_pbFilesExist = FALSE;
        }
        goto exit;
    }
    if (a_pbFilesExist)
    {
        *a_pbFilesExist = TRUE;
    }

    bResult = CompareFileSize(&llFileSize, a_bCompareMin);
    if (!bResult)
    {
        goto exit;
    }

    while (llFileSize.QuadPart > 0)
    {
        bResult = ReadFiles(ucBufferEPWrite, ucBufferEPRead, ulBufferLength, &ulActualBytesTransferred);
        if (!bResult)
        {
            goto exit;
        }
        if (ulActualBytesTransferred == 0)
        {
            break;
        }

        if (memcmp(ucBufferEPWrite, ucBufferEPRead, ulActualBytesTransferred)!=0)
        {
            bResult = FALSE;
            goto exit;
        }

        llFileSize.QuadPart -= ulActualBytesTransferred;

        memset(ucBufferEPWrite, 0, sizeof(ucBufferEPWrite));
        memset(ucBufferEPRead, 0, sizeof(ucBufferEPRead));
    }


exit:
    CloseFiles();
    return bResult;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadVerifier::CheckFileSizeTooBig
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

BOOL CPayloadVerifier::CheckFileSizeTooBig(LPCTSTR a_szFileNameEPWrite, LPCTSTR a_szFileNameEPRead)
{
    BOOL bResult = FALSE;
    LARGE_INTEGER llFileSizeRead = {0};
    LARGE_INTEGER llFileSizeWrite = {0};


    bResult = OpenFiles(a_szFileNameEPWrite, a_szFileNameEPRead);
    if (!bResult)
    {
        goto exit;
    }

    bResult = ::GetFileSizeEx(m_hFileEPWrite, &llFileSizeWrite);
    if (!bResult)
    {
        _tprintf(_T("CPayloadVerifier::CompareFileSize GetFileSizeEx READ failed!\n"));
        goto exit;
    }

    bResult = ::GetFileSizeEx(m_hFileEPRead, &llFileSizeRead);
    if (!bResult)
    {
        _tprintf(_T("CPayloadVerifier::CompareFileSize GetFileSizeEx WRITE failed!\n"));
        goto exit;
    }


exit:
    CloseFiles();

    if (llFileSizeWrite.QuadPart >= MAX_FILE_SIZE || llFileSizeRead.QuadPart >= MAX_FILE_SIZE)
    {
        DeleteFiles(a_szFileNameEPWrite, a_szFileNameEPRead);
    }

    return bResult;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadVerifier::OpenFiles
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

BOOL CPayloadVerifier::OpenFiles(LPCTSTR a_szFileNameEPWrite, LPCTSTR a_szFileNameEPRead)
{
    CUtils::CTimeProbe oTimer;
    DWORD dwMsecs = 0;
    BOOL bRet = FALSE;


    if (!a_szFileNameEPWrite || !a_szFileNameEPRead)
    {
        _tprintf(_T("%s Invalid parameters! %s %s\n"), _T(__FUNCTION__), a_szFileNameEPWrite, a_szFileNameEPRead);
        return FALSE;
    }
    
    // Check if the files exists
    {
        bRet = ::PathFileExists(a_szFileNameEPWrite);
        if (!bRet)
        {
            _tprintf(_T("%s PathFileExists failed! [%s] %d\n"), _T(__FUNCTION__), a_szFileNameEPWrite, GetLastError());
        }

        bRet = ::PathFileExists(a_szFileNameEPRead);
        if (!bRet)
        {
            _tprintf(_T("%s PathFileExists failed! [%s] %d\n"), _T(__FUNCTION__), a_szFileNameEPRead, GetLastError());
        }
    }

    // Check if the files can be opened
    {
        m_hFileEPWrite = ::CreateFile(a_szFileNameEPWrite, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (m_hFileEPWrite == INVALID_HANDLE_VALUE)
        {
            _tprintf(_T("%s CreateFile failed! [%s] %d\n"), _T(__FUNCTION__), a_szFileNameEPWrite, GetLastError());
        }

        m_hFileEPRead = ::CreateFile(a_szFileNameEPRead, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (m_hFileEPRead == INVALID_HANDLE_VALUE)
        {
            _tprintf(_T("%s CreateFile failed! [%s] %d\n"), _T(__FUNCTION__), a_szFileNameEPRead, GetLastError());
        }
    }

    // Retry opening files for X minutes
    {
        oTimer.StartTime();
        if (m_hFileEPWrite == INVALID_HANDLE_VALUE)
        {
            do
            {
                m_hFileEPWrite = ::CreateFile(a_szFileNameEPWrite, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (m_hFileEPWrite == INVALID_HANDLE_VALUE)
                {
                    Sleep(1000);
                    continue;
                }

                break;

            } while (oTimer.StopTime() < 60000); // 1 min
        }
        else
        {
            oTimer.StopTime();
        }

        oTimer.StartTime();
        if (m_hFileEPRead == INVALID_HANDLE_VALUE)
        {
            do
            {
                m_hFileEPRead = ::CreateFile(a_szFileNameEPRead, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (m_hFileEPRead == INVALID_HANDLE_VALUE)
                {
                    Sleep(1000);
                    continue;
                }

                break;

            } while (oTimer.StopTime() < 60000); // 1 min
        }
        else
        {
            oTimer.StopTime();
        }
    }

    if (m_hFileEPWrite == INVALID_HANDLE_VALUE)
    {
        _tprintf(_T("%s CreateFile failed even after retrying for 1 minute! [%s] %d\n"), _T(__FUNCTION__), a_szFileNameEPWrite, GetLastError());
    }

    if (m_hFileEPRead == INVALID_HANDLE_VALUE)
    {
        _tprintf(_T("%s CreateFile failed! even after retrying for 1 minute [%s] %d\n"), _T(__FUNCTION__), a_szFileNameEPRead, GetLastError());
    }

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadVerifier::CloseFiles
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

VOID CPayloadVerifier::CloseFiles()
{
    if (m_hFileEPWrite != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_hFileEPWrite);
        m_hFileEPWrite = INVALID_HANDLE_VALUE;
    }

    if (m_hFileEPRead != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_hFileEPRead);
        m_hFileEPRead = INVALID_HANDLE_VALUE;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadVerifier::CompareFileSize
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

BOOL CPayloadVerifier::CompareFileSize(PLARGE_INTEGER a_pllFileSize, BOOL a_bCompareMin)
{
    LARGE_INTEGER llFileSizeRead = {0};
    LARGE_INTEGER llFileSizeWrite = {0};
    BOOL bResult = FALSE;


    bResult = ::GetFileSizeEx(m_hFileEPWrite, &llFileSizeWrite);
    if (!bResult)
    {
        _tprintf(_T("CPayloadVerifier::CompareFileSize GetFileSizeEx READ failed!\n"));
    }

    bResult = ::GetFileSizeEx(m_hFileEPRead, &llFileSizeRead);
    if (!bResult)
    {
        _tprintf(_T("CPayloadVerifier::CompareFileSize GetFileSizeEx WRITE failed!\n"));
    }

    if (!a_bCompareMin)
    {
        if (llFileSizeWrite.QuadPart > llFileSizeRead.QuadPart)
        {
            _tprintf(_T("CPayloadVerifier::CompareFileSize failed! llFileSizeWrite.QuadPart(%I64d) != llFileSizeRead.QuadPart(%I64d)\n"), llFileSizeWrite.QuadPart, llFileSizeRead.QuadPart);
            return FALSE;
        }
        else if (llFileSizeWrite.QuadPart < llFileSizeRead.QuadPart)
        {
            //
            // When stopping WRITE and READ transfer on STREAMING mode, 
            // the data read can sometimes be more than data written 
            // because WinUsb does not return the correct value when WRITE was aborted. 
            // As such, application should handle this error condition 
            // and only compare data using the length of the written data, not the read data. 
            //
            _tprintf(_T("CPayloadVerifier::CompareFileSize warning! llFileSizeWrite.QuadPart(%I64d) != llFileSizeRead.QuadPart(%I64d)\n"), llFileSizeWrite.QuadPart, llFileSizeRead.QuadPart);
        }

        a_pllFileSize->QuadPart = llFileSizeWrite.QuadPart;
    }
    else
    {
        a_pllFileSize->QuadPart = llFileSizeRead.QuadPart;
    }
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadVerifier::ReadFiles
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

BOOL CPayloadVerifier::ReadFiles(PUCHAR a_pucBufferEPWrite, PUCHAR a_pucBufferEPRead, ULONG a_ulBufferLength, PULONG a_pulActualBytesTransferred)
{
    BOOL bResult = FALSE;
    ULONG ulActualBytesWritten = 0;
    ULONG ulActualBytesRead = 0;


    bResult = ::ReadFile(m_hFileEPWrite, a_pucBufferEPWrite, a_ulBufferLength, &ulActualBytesWritten, NULL);
    if (!bResult)
    {
        _tprintf(_T("CPayloadVerifier::ReadFiles ReadFile failed! %d\n"), GetLastError());
        return FALSE;
    }

    bResult = ::ReadFile(m_hFileEPRead, a_pucBufferEPRead, a_ulBufferLength, &ulActualBytesRead, NULL);
    if (!bResult)
    {
        _tprintf(_T("CPayloadVerifier::ReadFiles ReadFile failed! %d\n"), GetLastError());
        return FALSE;
    }

    if (ulActualBytesWritten > ulActualBytesRead)
    {
        _tprintf(_T("CPayloadVerifier::ReadFiles ReadFile failed! ulActualBytesWritten != ulActualBytesRead\n"));
        return FALSE;
    }
    else 
        if (ulActualBytesWritten < ulActualBytesRead)
    {
        //
        // When stopping WRITE and READ transfer on STREAMING mode, 
        // the data read can sometimes be more than data written 
        // because WinUsb does not return the correct value when WRITE was aborted. 
        // As such, application should handle this error condition 
        // and only compare data using the length of the written data, not the read data. 
        //
        _tprintf(_T("CPayloadVerifier::ReadFiles ReadFile warning! ulActualBytesWritten != ulActualBytesRead\n"));
    }

    *a_pulActualBytesTransferred = ulActualBytesRead;
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadVerifier::DeleteFiles
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

BOOL CPayloadVerifier::DeleteFiles(LPCTSTR a_szFileNameEPWrite, LPCTSTR a_szFileNameEPRead)
{
    BOOL bResult = FALSE;


    if (a_szFileNameEPWrite)
    {
        bResult = ::DeleteFile(a_szFileNameEPWrite);
        if (!bResult)
        {
            _tprintf(_T("CPayloadVerifier::DeleteFiles DeleteFile %s failed! %d\n"), a_szFileNameEPWrite, GetLastError());
            return bResult;
        }
    }

    if (a_szFileNameEPRead)
    {
        bResult = ::DeleteFile(a_szFileNameEPRead);
        if (!bResult)
        {
            _tprintf(_T("CPayloadVerifier::DeleteFiles DeleteFile %s failed! %d\n"), a_szFileNameEPRead, GetLastError());
            return bResult;
        }
    }

    return bResult;
}


