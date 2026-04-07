/*
 * FT600 Data Loopback Demo App
 *
 * Copyright (C) 2016 FTDI Chip
 *
 */

#pragma once
#include "APP_Task.h"
#include "DRV_DriverInterface.h"
#include "APP_LoopbackMonitoring.h"
#include <list>
#include <iterator>



class CUIManager;
class CWriterThread;
class CReaderThread;
class CPayloadRecorder;
class CLoopbackMonitor;
class CDriverInterface;

typedef struct _UICompletionParam
{
    CTaskManager* pThis;
    PTTASK_MANAGER_PARAM pTask;

} UICompletionParam;



class CTaskManager : public CWinThread
{
    DECLARE_DYNCREATE(CTaskManager)

protected:
    CTaskManager() {}

public:

    CTaskManager(CUIManager* a_pUIManager, EOPEN_BY a_eTypeOpenBy, PVOID a_pvOpenByParam); 
    ~CTaskManager();
    virtual BOOL InitInstance();
    VOID OnStartWork(WPARAM a_wParam, LPARAM a_lParam);


protected:
    DECLARE_MESSAGE_MAP()


//////////////////////////////////////////////////////////////////////////


public:

    BOOL ExitThread();
    BOOL AddTask(CONST PTTASK_MANAGER_PARAM a_pTask);
    BOOL CompletionRoutine(PTTASK_COMPLETION_PARAM a_pCompletionParam);
    BOOL GetVIDPID(PUSHORT a_puwVID, PUSHORT a_puwPID);
	BOOL IsDevicePath(CONST CHAR* pucDevicePath);
    BOOL IsLoopbackReset(UCHAR a_ucEP);
    BOOL SetLoopbackData(BOOL a_bWrite, UCHAR a_ucEP, PLARGE_INTEGER a_pllPayloadTransferred, ETASK_PAYLOAD_TYPE a_eType);
    BOOL IsLoopbackDataMisMatch(UCHAR a_ucEP);
    CDriverInterface* GetDriver() 
    {
        return m_pDriver;
    }

    CLoopbackMonitor* GetLoopbackMonitor(UCHAR a_ucEP) 
    { 
        if (a_ucEP & 0x82) return m_pLoopbackMonitor[a_ucEP - BASE_READ_EP]; 
        else return m_pLoopbackMonitor[a_ucEP - BASE_WRITE_EP];
    }


private:

    BOOL Initialize();
    VOID Cleanup();
    VOID ProcessTasks();

    // Task-related
    BOOL ProcessATask(CONST PTTASK_MANAGER_PARAM a_pTask);
    BOOL ProcessAWriteTask(CONST PTTASK_MANAGER_PARAM a_pTask);
        BOOL ProcessAWriteStartTask(CONST PTTASK_MANAGER_PARAM a_pTask);
        BOOL ProcessAWriteStopTask(CONST PTTASK_MANAGER_PARAM a_pTask);
    BOOL ProcessAReadTask(CONST PTTASK_MANAGER_PARAM a_pTask);
        BOOL ProcessAReadStartTask(CONST PTTASK_MANAGER_PARAM a_pTask);
        BOOL ProcessAReadStopTask(CONST PTTASK_MANAGER_PARAM a_pTask);
        BOOL ProcessAReadFlushTask(CONST PTTASK_MANAGER_PARAM a_pTask);

    // Completion routine-related
    BOOL CompletionRoutine_VerifyResult(PTTASK_MANAGER_PARAM a_pTask);
    VOID EnqueueOngoingTask(PTTASK_MANAGER_PARAM a_pTask);
    VOID DequeueOngoingTask(CONST PTTASK_COMPLETION_PARAM a_pCompletionParam, PTTASK_MANAGER_PARAM *a_pTask);
    VOID DequeueOngoingTask(UCHAR a_ucEP, PTTASK_MANAGER_PARAM *a_pTask);
    BOOL IsExistOngoingTask(UCHAR a_ucEP);

    VOID LaunchUICompletionThread(PTTASK_MANAGER_PARAM pTask);
    static DWORD UICompletionThread(PVOID a_pvParam);

    VOID LaunchUIDeviceDetectionThread();
    static DWORD UIDeviceDetectionThread(PVOID a_pvParam);


private:

    CUIManager                         *m_pUIManager;
    BOOL                                m_bInitializeDone;
    BOOL                                m_bCleanupDone;
    BOOL                                m_bCleanupOngoing;
    HANDLE                              m_hEvent;

    // Task-related
    CRITICAL_SECTION                    m_csNewTasks;
    CRITICAL_SECTION                    m_csOngoingTasks;
    std::list<PTTASK_MANAGER_PARAM>     m_listNewTasks;
    std::list<PTTASK_MANAGER_PARAM>     m_listOngoingTasks;
    LARGE_INTEGER                       m_llTaskNumber;
    CRITICAL_SECTION                    m_csCompletionRoutine;
    CRITICAL_SECTION                    m_csCleanup;
    CRITICAL_SECTION                    m_csUICompletionRoutine;

    // Read and write operations
    CWriterThread                      *m_pWriterThreads[NUM_EP_PAIRS];
    CReaderThread                      *m_pReaderThreads[NUM_EP_PAIRS];
    CPayloadRecorder                   *m_pWritePayloadRecorder[NUM_EP_PAIRS];
    CPayloadRecorder                   *m_pReadPayloadRecorder[NUM_EP_PAIRS];
    CLoopbackMonitor                   *m_pLoopbackMonitor[NUM_EP_PAIRS];
    BOOL                                m_bLoopbackPending[NUM_EP_PAIRS];

    // Endpoint-related
    UCHAR                               m_ucWriteEP[NUM_EP_PAIRS];
    UCHAR                               m_ucReadEP[NUM_EP_PAIRS];
    UCHAR                               m_ucNumWriteEP;
    UCHAR                               m_ucNumReadEP;
	USHORT                              m_uwWriteMPS[NUM_EP_PAIRS];
	USHORT                              m_uwReadMPS[NUM_EP_PAIRS];
    // Open-device related
    EOPEN_BY                            m_eTypeOpenBy;
    PVOID                               m_pvOpenByParam;
    USHORT                              m_uwVID;
    USHORT                              m_uwPID;
    ULONG                               m_ulFirmwareVersion;
	ULONG                               m_ulDriverVersion;
	ULONG                               m_ulLibraryVersion;
    CDriverInterface                   *m_pDriver;
};


