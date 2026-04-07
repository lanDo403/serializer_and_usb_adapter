/*
 * FT600 Data Loopback Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#pragma once



class CPayloadRecorder
{

public:

    CPayloadRecorder(UCHAR a_ucEP);
    ~CPayloadRecorder();

    BOOL Write(PUCHAR a_pucBuffer, ULONG a_ulBufferLength, PULONG a_pulActualBytesWritten);
    BOOL UpdateFileName(SYSTEMTIME *a_pTime);
    VOID Close();
    BOOL GetFileName(LPTSTR a_szFileName, ULONG a_ulFileNameSize);
    BOOL GetFileSize(ULARGE_INTEGER *a_pullFileSize);
    LPCTSTR GetFileName();
    BOOL PathFileExist();


private:

    BOOL Create();


private:

    UCHAR               m_ucEP;
    TCHAR               m_szFileName[MAX_PATH];
    HANDLE              m_hFile;
    CRITICAL_SECTION    m_cs;

    LARGE_INTEGER       m_llFileSize;
};


