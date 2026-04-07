/*
 * FT600 Data Streamer Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#pragma once
#include "APP_Utils.h"
#include "stdafx.h"
#include "resource.h"



#define LOG_TO_FILE
#define GUI_LOG(format, ...)        do{ if(g_pLogger) g_pLogger->Log(FALSE, FALSE, TRUE, format, ## __VA_ARGS__); }while(0)
#define CMD_LOG(format, ...)        do{ if(g_pLogger) g_pLogger->Log(FALSE, TRUE, FALSE, format, ## __VA_ARGS__); }while(0)
#define APP_LOG(format, ...)        do{ if(g_pLogger) g_pLogger->Log(FALSE, TRUE, TRUE, format, ## __VA_ARGS__); }while(0)
#define CMD_LOG_NOTIME(format, ...) do{ if(g_pLogger) g_pLogger->Log(TRUE, TRUE, FALSE, format, ## __VA_ARGS__); }while(0)
#define STAT_LOG(rate, write)       do{ if(g_pLogger) g_pLogger->LogRate(rate, write); }while(0)



class CLogger;
extern CLogger *g_pLogger;



class CLogger
{

public:

    CLogger(LPCTSTR a_szFileName, LPCTSTR a_szDialogTitle);
    ~CLogger();

    void Log(BOOL a_bNoTime, BOOL a_bConsole, BOOL a_bUI, LPCTSTR a_format, ...);


public:
    void LogRate(ULONG a_ulRate, BOOL bWrite);


private:

    void InternalHandleMaxFileSizeReached(LPCTSTR a_szFileName, BOOL a_bOpenNewFile);


private:

    static const ULONG MAX_LINE_BUFFER_SIZE = 1024;
    static const ULONG MAX_LOG_FILE_SIZE = 10485760; // 10MB 1024*1024*10

    CRITICAL_SECTION    m_cs;
    TCHAR               m_aucLineBuffer[MAX_LINE_BUFFER_SIZE];

    // GUI related
    BOOL                m_bGUI;
    HWND                m_hEditText;
    HWND                m_hEditTextWriteRate;
    HWND                m_hEditTextReadRate;

    // FILE related
    FILE               *m_pLogFile;
    TCHAR               m_szFileName[MAX_PATH];

};


