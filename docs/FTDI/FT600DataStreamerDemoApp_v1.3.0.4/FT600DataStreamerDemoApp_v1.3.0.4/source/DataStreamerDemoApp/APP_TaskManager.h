/*
 * FT600 Data Streamer Demo App
 *
 * Copyright (C) 2016 FTDI Chip
 *
 */

#pragma once
#include "APP_Task.h"
#include "DRV_DriverInterface.h"
#include <list>
#include <iterator>



class CUIManager;
class CWriterThread;
class CReaderThread;
class CDriverInterface;

typedef struct _UICompletionParam
{
    CTaskManager* pThis;
    UCHAR ucEP;

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
    CDriverInterface* GetDriver() {return m_pDriver;}

	// Read and write operations
	CWriterThread                      *m_pWriterThreads;
	CReaderThread                      *m_pReaderThreads;

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

    // Completion routine-related
    VOID EnqueueOngoingTask(PTTASK_MANAGER_PARAM a_pTask);
    VOID DequeueOngoingTask(CONST PTTASK_COMPLETION_PARAM a_pCompletionParam, PTTASK_MANAGER_PARAM *a_pTask);
    VOID DequeueOngoingTask(UCHAR a_ucEP, PTTASK_MANAGER_PARAM *a_pTask);
    BOOL IsExistOngoingTask(UCHAR a_ucEP);

    VOID LaunchUICompletionThread(UCHAR a_ucEP);
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
    std::list<PTTASK_MANAGER_PARAM>     m_listNewTasks;
    CRITICAL_SECTION                    m_csOngoingTasks;
    std::list<PTTASK_MANAGER_PARAM>     m_listOngoingTasks;
    LARGE_INTEGER                       m_llTaskNumber;
    CRITICAL_SECTION                    m_csCompletionRoutine;
    CRITICAL_SECTION                    m_csCleanup;
    CRITICAL_SECTION                    m_csUICompletionRoutine;



    // Endpoint-related
    UCHAR                               m_ucWriteEP[NUM_EP_PAIRS];
    UCHAR                               m_ucReadEP[NUM_EP_PAIRS];
    UCHAR                               m_ucNumWriteEP;
    UCHAR                               m_ucNumReadEP;

    // Open-device related
    EOPEN_BY                            m_eTypeOpenBy;
    PVOID                               m_pvOpenByParam;
    USHORT                              m_uwVID;
    USHORT                              m_uwPID;
    ULONG                               m_ulFirmwareVersion;
	ULONG                               m_ulDriverVersion;
	ULONG                               m_ulLibraryVersion;
	BOOL                                m_bIsUsb3;
    CDriverInterface                   *m_pDriver;

    static BOOL                         m_bCyclePort;
	FT_DEVICE							m_eIntfType;

};


