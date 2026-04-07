/*
 * FT600 Data Loopback Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#pragma once



class CPayloadVerifier
{

public:

    CPayloadVerifier();
    ~CPayloadVerifier();
    BOOL CompareFiles(LPCTSTR a_szFileNameEPWrite, LPCTSTR a_szFileNameEPRead, BOOL *a_pbFilesExist, BOOL a_bCompareMin);
    BOOL DeleteFiles(LPCTSTR a_szFileNameEPWrite, LPCTSTR a_szFileNameEPRead);
    BOOL CheckFileSizeTooBig(LPCTSTR a_szFileNameEPWrite, LPCTSTR a_szFileNameEPRead); 

private:

    BOOL OpenFiles(LPCTSTR a_szFileNameEPWrite, LPCTSTR a_szFileNameEPRead);
    VOID CloseFiles();
    BOOL CompareFileSize(PLARGE_INTEGER a_pllFileSize, BOOL a_bCompareMin);
    BOOL ReadFiles(PUCHAR a_pucBufferEPWrite, PUCHAR a_pucBufferEPRead, ULONG a_ulBufferLength, PULONG a_pulActualBytesTransferred);


private:

    HANDLE m_hFileEPWrite;
    HANDLE m_hFileEPRead;

};


