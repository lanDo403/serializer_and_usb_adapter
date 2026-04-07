/*
 * FT600 Data Streamer Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#pragma once
#include "APP_Task.h"
#include "APP_Utils.h"


class CTaskManager;
class CUtils::CTimeProbe;
class CPayloadGenerator;

class CWriterThread : public CWinThread
{
    DECLARE_DYNCREATE(CWriterThread)

protected:
    CWriterThread() {}  

public:
    CWriterThread(CTaskManager* a_pTaskManager, UCHAR a_ucEP); 
    ~CWriterThread();
    virtual BOOL InitInstance();

    VOID OnStartWork(WPARAM a_wParam, LPARAM a_lParam);

protected:
    DECLARE_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////


public:

    // Set parameters
    BOOL SetTaskCompletionIdentifier(CONST PLARGE_INTEGER a_pllID);
    BOOL SetTaskTestParam(CONST PTTASK_TEST_PARAM a_pTestParam);
	BOOL SetTaskPayloadType(CONST PTTASK_PAYLOAD_PARAM a_pPayload);

    // Commands
    BOOL StartTask();
    BOOL StopTask();

    BOOL ExitThread();


private:

    BOOL Initialize();
    VOID Cleanup();
    VOID ProcessTasks();

    ETASK_STATUS ProcessAStartTaskSynch();
    ETASK_STATUS ProcessAStartTaskAsynch();
    BOOL         ProcessAStopTask();
    VOID         ShowStatus(ULONG ulTotalBytesTransferred, ULONG ulTimeMs);
	VOID         ShowStats();
    BOOL IsStopTask();
    VOID SetStopTask(BOOL a_bSet);
    BOOL IsOngoingTask();
    VOID SetOngoingTask(BOOL a_bSet);


private:

    CTaskManager                       *m_pTaskManager;
    UCHAR                               m_ucEP;

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
    TTASK_TEST_PARAM                    m_TestParam;        // Test parameters
    TTASK_COMPLETION_PARAM              m_CompletionParam;  // Completion status for completion callback
	TTASK_PAYLOAD_PARAM					m_Payload;
    CUtils::CTimeProbe                  m_oTimer;

	CPayloadGenerator                  *m_pPayload;
	ULONG								m_ulTotalBytesTransferred;

};


