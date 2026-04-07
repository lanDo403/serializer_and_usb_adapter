/*
 * FT600 Data Loopback Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#pragma once
#include "APP_Task.h"
#include "APP_Utils.h"



class CPayloadRecorder;
class CTaskManager;
class CUtils::CTimeProbe;

class CReaderThread : public CWinThread
{
    DECLARE_DYNCREATE(CReaderThread)

protected:
    CReaderThread() {} 

public:
    CReaderThread(CTaskManager* a_pTaskManager, UCHAR a_ucEP, CPayloadRecorder *a_pPayloadRecorder, USHORT a_uwMPS); 
    ~CReaderThread();
    virtual BOOL InitInstance();

    VOID OnStartWork(WPARAM a_wParam, LPARAM a_lParam);

protected:
    DECLARE_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////


public:

    // Set parameters
    BOOL SetTaskCompletionIdentifier(CONST PLARGE_INTEGER a_pllID);
    BOOL SetTaskPayloadType(CONST PTTASK_PAYLOAD_PARAM a_pPayload);
    BOOL SetTaskTestParam(CONST PTTASK_TEST_PARAM a_pTestParam);
    BOOL SetPerformanceTest(BOOL a_bTrue);

    // Commands
    BOOL StartTask();
    BOOL StopTask(BOOL a_bFlush);

    BOOL ExitThread();


private:

    BOOL Initialize();
    VOID Cleanup();
    VOID ProcessTasks();

    ETASK_STATUS ProcessAStartTask(PLARGE_INTEGER a_pllBytesTransferred, DWORD *a_pdwTimeMs);
    BOOL         ProcessAStopTask(BOOL a_bFlush = FALSE);

    BOOL IsStopTask();
    VOID SetStopTask(BOOL a_bSet);
    BOOL IsOngoingTask();
    VOID SetOngoingTask(BOOL a_bSet);

    VOID ShowErrorMessage();


private:

    CTaskManager                       *m_pTaskManager;
    UCHAR                               m_ucEP;
    CPayloadRecorder                   *m_pPayloadRecorder;

    BOOL                                m_bCleanupDone;
    HANDLE                              m_hEvent;
    CRITICAL_SECTION                    m_csEvent;
    CRITICAL_SECTION                    m_csCleanup;

    // StopTask and ongoing task flags
    BOOL                                m_bStopTask;
    BOOL                                m_bOngoingTask;
    CRITICAL_SECTION                    m_csStopTask;
    CRITICAL_SECTION                    m_csOngoingTask;

    // Test parameters
    TTASK_PAYLOAD_PARAM                 m_Payload;          // Payload parameters
    TTASK_TEST_PARAM                    m_TestParam;        // Test parameters
    TTASK_COMPLETION_PARAM              m_CompletionParam;  // Completion status for completion callback

	USHORT								m_uwMPS;
};


