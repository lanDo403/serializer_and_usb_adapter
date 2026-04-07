/*
 * FT600 Data Streamer Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#pragma once
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>
#include <wincrypt.h>
#include "APP_Logger.h"



#define MIN(a, b)    ((a)<(b) ? (a) : (b)) 
#define MAX(a, b)    ((a)<(b) ? (b) : (a)) 



class CUtils
{

public:

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // CCriticalSectionHolder
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    class CCriticalSectionHolder
    {

    public:

        CCriticalSectionHolder(CRITICAL_SECTION *a_pCS) : m_pCS(a_pCS) 
        {
            if (m_pCS) 
            {
                ::EnterCriticalSection(m_pCS);
            }
        }

        ~CCriticalSectionHolder()
        {
            if (m_pCS) 
            {
                ::LeaveCriticalSection(m_pCS);
            }
        }
        

    private:

        LPCRITICAL_SECTION m_pCS;

    }; // class CCriticalSectionHolder

    

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // CMFCConsole
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    class CMFCConsole
    {

    private:

        static const WORD MAX_CONSOLE_LINES = 5000;
        static const WORD MAX_CONSOLE_WIDTH = 100;
        static HWND m_hwndConsole;


    public:

        static void Show(BOOL a_bShow, LPCTSTR a_szTitle = NULL)
        {
            if (m_hwndConsole == NULL)
            {
                Initialize(a_szTitle);
            }

            if (m_hwndConsole != NULL)
            {
                if (a_bShow)
                {
                    ::ShowWindow(m_hwndConsole, SW_SHOW);
                }
                else
                {
                    ::ShowWindow(m_hwndConsole, SW_HIDE);
                }
            }
        }

        static void Initialize(LPCTSTR a_szTitle)
        {
            BOOL bRet = ::AllocConsole();
            if (!bRet)
            {
                return;
            }

            m_hwndConsole = ::GetConsoleWindow();
            if (!m_hwndConsole)
            {
                return;
            }

            bRet = ::ShowWindow(m_hwndConsole, SW_HIDE); // hide by default
            if (!bRet)
            {
                return;
            }

#if 0
            CMenu* pSysMenu = CWnd::FromHandle(m_hwndConsole)->GetSystemMenu(FALSE);
            if (pSysMenu != NULL)
            {
                pSysMenu->DeleteMenu(SC_CLOSE, MF_BYCOMMAND);
            }
#endif
            HANDLE hConsoleOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);
            if (INVALID_HANDLE_VALUE == hConsoleOutput)
            {
                return;
            }

            CONSOLE_SCREEN_BUFFER_INFO coninfo = {0};
            bRet = ::GetConsoleScreenBufferInfo(hConsoleOutput, &coninfo);
            if (!bRet)
            {
                return;
            }

            coninfo.dwSize.Y = MAX_CONSOLE_LINES;
            coninfo.dwSize.X = MAX_CONSOLE_WIDTH;
            coninfo.wAttributes = FOREGROUND_GREEN;
            bRet = ::SetConsoleScreenBufferSize(hConsoleOutput, coninfo.dwSize);
            if (!bRet)
            {
                return;
            }
            bRet = ::SetConsoleTextAttribute(hConsoleOutput, coninfo.wAttributes);
            if (!bRet)
            {
                return;
            }
            bRet = ::SetConsoleTitle(a_szTitle);
            if (!bRet)
            {
                return;
            }

            SMALL_RECT rect = {0};
            rect.Left = 0;
            rect.Top = 0;
            rect.Right = MAX_CONSOLE_WIDTH - 1;
            rect.Bottom = 24;
            ::SetConsoleWindowInfo(hConsoleOutput, TRUE, &rect);
            if (!bRet)
            {
                return;
            }

            // redirect unbuffered STDOUT to the console
            int hConHandle = ::_open_osfhandle((intptr_t )hConsoleOutput, _O_TEXT);
            if (hConHandle == -1)
            {
                return;
            }
            FILE *fp = ::_fdopen( hConHandle, "w" );
            if (!fp)
            {
                return;
            }
            *stdout = *fp;
            ::setvbuf( stdout, NULL, _IONBF, 0 );

            // redirect unbuffered STDERR to the console
            hConHandle = ::_open_osfhandle((intptr_t )::GetStdHandle(STD_ERROR_HANDLE), _O_TEXT );
            if (hConHandle == -1)
            {
                return;
            }
            fp = ::_fdopen( hConHandle, "w" );
            if (!fp)
            {
                return;
            }
            *stderr = *fp;
            ::setvbuf( stderr, NULL, _IONBF, 0 );

            // make cout, wcout, cin, wcin, wcerr, cerr, wclog and clog point to console as well
            std::ios::sync_with_stdio();
        }

    }; // class CMFCConsole



    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // CTimeProbe
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    class CTimeProbe
    {

    public:

        CTimeProbe()
        {
            QueryPerformanceFrequency(&m_llFrequency);
            if (m_llFrequency.QuadPart == 0)
            {
                _tprintf(_T("QueryPerformanceFrequency error: hardware supports no high-resolution counter!\n"));
            }
            
        }

        VOID StartTime()
        {
            QueryPerformanceCounter(&m_llTimeStart);
        }

        DWORD StopTime()
        {
            QueryPerformanceCounter(&m_llTimeEnd);
            return ComputeTimeElapsedInMilliSeconds();
        }

        VOID StopTime(DWORD *a_pdwTime, BOOL a_bAppend)
        {
            QueryPerformanceCounter(&m_llTimeEnd);

            if (a_pdwTime)
            {
                if (a_bAppend)
                {
                    *a_pdwTime += ComputeTimeElapsedInMilliSeconds();
                }
                else
                {
                    *a_pdwTime = ComputeTimeElapsedInMilliSeconds();
                }
            }
        }

        DWORD GetTime()
        {
            QueryPerformanceCounter(&m_llTimeStart);
            return (DWORD)m_llTimeStart.QuadPart;
        }


    private:

        DWORD ComputeTimeElapsedInMilliSeconds()
        {
            LONGLONG llElapsed = m_llTimeEnd.QuadPart - m_llTimeStart.QuadPart;
            LONGLONG dwTemp = (LONGLONG)(llElapsed * (1000000/MICROSECOND_PER_MILLISECOND) / (float)m_llFrequency.QuadPart);
            //m_llElapsedMicroseconds.QuadPart = m_llTimeEnd.QuadPart - m_llTimeStart.QuadPart;
            //m_llElapsedMicroseconds.QuadPart *= 1000;
            //m_llElapsedMicroseconds.QuadPart /= m_llFrequency.QuadPart;
            //printf("%I64d %d\n", m_llElapsedMicroseconds.QuadPart, dwTemp);
            return (DWORD)dwTemp;
        }


    private:

        static CONST DWORD MICROSECOND_PER_MILLISECOND = 1000;
        LARGE_INTEGER m_llTimeStart;
        LARGE_INTEGER m_llTimeEnd;
        LARGE_INTEGER m_llFrequency;

    }; // class CTimeProbe



    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // CGuid
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    class CGuid
    {

    public:

        static BOOL GUIDToString(CONST GUID* a_pGUID, LPTSTR a_szBuffer, ULONG a_ulBufferLen)
        {
            if (!a_pGUID || !a_szBuffer || a_ulBufferLen < 37)
            {
                return FALSE;
            }

            _stprintf_s(a_szBuffer, a_ulBufferLen, _T("{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}"), 
                a_pGUID->Data1, a_pGUID->Data2, a_pGUID->Data3,
                a_pGUID->Data4[0], a_pGUID->Data4[1], a_pGUID->Data4[2], a_pGUID->Data4[3],
                a_pGUID->Data4[4], a_pGUID->Data4[5], a_pGUID->Data4[6], a_pGUID->Data4[7]
                );

            return TRUE;
        }

        static BOOL StringToGUID(LPCTSTR a_szBuffer, ULONG a_ulBufferLen, GUID* a_pGUID)
        {
            if (!a_szBuffer || !a_pGUID)
            {
                return FALSE;
            }

            int iRet = _sntscanf_s(a_szBuffer, a_ulBufferLen, _T("{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}"), 
                &a_pGUID->Data1, &a_pGUID->Data2, &a_pGUID->Data3,
                &a_pGUID->Data4[0], &a_pGUID->Data4[1], &a_pGUID->Data4[2], &a_pGUID->Data4[3],
                &a_pGUID->Data4[4], &a_pGUID->Data4[5], &a_pGUID->Data4[6], &a_pGUID->Data4[7]
                );
            if (iRet != 11)
            {
                return FALSE;
            }

            return TRUE;
        }

    }; // class CGuid



    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // CPseudoRandomNumberGenerator
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    class CPseudoRandomNumberGenerator
    {

    public:

        static BOOL Generate(PVOID a_pvRandomNumber, ULONG a_ulRandomNumberLength)
        {
            BOOL bRet = FALSE;
            HCRYPTPROV hCryptProv = NULL;


            // Acquire a cryptographic provider context handle.
            bRet = CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
            if (!bRet)
            {   
                DWORD dwError = GetLastError();
                return FALSE;
            }

            // Generate random number
            bRet = CryptGenRandom(hCryptProv, a_ulRandomNumberLength, (BYTE*)a_pvRandomNumber);
            if (!bRet)
            {    
                DWORD dwError = GetLastError();
            }

            // Release handle
            CryptReleaseContext(hCryptProv, 0);

            return bRet;
        }


    private:


    }; // class CPseudoRandomNumberGenerator

};


