/*
 * FT600 Data Streamer Demo App
 *
 * Copyright (C) 2016 FTDI Chip
 *
 */

#include "stdafx.h"
#include "UI_Manager.h"
#include "APP_TaskManager.h"
#include "APP_ReaderThread.h"
#include "APP_WriterThread.h"
#include "APP_Device.h"
#include "APP_Utils.h"



IMPLEMENT_DYNCREATE(CTaskManager, CWinThread)

BOOL CTaskManager::m_bCyclePort = FALSE;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::CTaskManager
//      This contains the Task Manager thread which processes tasks received from UI Manager thread 
//      and sends it to the corresponding Writer threads and/or Reader threads. 
//      Each endpoint (EP02, EP03, EP04, EP05, EP82, EP83, EP84, and EP85) 
//      has its own writer/reader thread. So if there are 8 endpoints, 
//      then there are 8 writer/reader threads in total. 
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

CTaskManager::CTaskManager(CUIManager* a_pUIManager, EOPEN_BY a_eTypeOpenBy, PVOID a_pvOpenByParam): 
    m_pUIManager(a_pUIManager),
    m_bInitializeDone(FALSE),
    m_bCleanupDone(FALSE),
    m_bCleanupOngoing(FALSE),
    m_hEvent(NULL),
    m_eTypeOpenBy(a_eTypeOpenBy),
    m_pvOpenByParam(a_pvOpenByParam),
    m_pDriver(NULL)
{
    CMD_LOG(_T("INITIALIZATION: %s"), _T(__FUNCTION__));

    m_pWriterThreads = NULL;
    m_pReaderThreads = NULL;
    m_llTaskNumber.QuadPart = 0;

    memset(&m_ucWriteEP, 0, sizeof(m_ucWriteEP));
    memset(&m_ucReadEP, 0, sizeof(m_ucReadEP));
    m_ucNumWriteEP = 0;
    m_ucNumReadEP = 0;
    m_uwVID = 0;
    m_uwPID = 0;

    try
    {
        ::InitializeCriticalSection(&m_csNewTasks);
        ::InitializeCriticalSection(&m_csOngoingTasks);
        ::InitializeCriticalSection(&m_csCompletionRoutine);
        ::InitializeCriticalSection(&m_csCleanup);
        ::InitializeCriticalSection(&m_csUICompletionRoutine);
    }
    catch (int)
    {
        CMD_LOG(_T("INITIALIZATION: %s failed!"), _T(__FUNCTION__));
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::~CTaskManager
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

CTaskManager::~CTaskManager()
{
    CMD_LOG(_T("TERMINATION: %s"), _T(__FUNCTION__));

    Cleanup();
    //::DeleteCriticalSection(&m_csNewTasks);
    //::DeleteCriticalSection(&m_csOngoingTasks);
    //::DeleteCriticalSection(&m_csCompletionRoutine);
    //::DeleteCriticalSection(&m_csCleanup);
    //::DeleteCriticalSection(&m_csUICompletionRoutine);

    CMD_LOG(_T("TERMINATION: %s DONE!"), _T(__FUNCTION__));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::InitInstance
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

BOOL CTaskManager::InitInstance()
{
    PostThreadMessage(WM_STARTWORK, 0, 0);
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::OnStartWork
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

VOID CTaskManager::OnStartWork(WPARAM wParam, LPARAM lParam)
{
    //_ASSERTE(_CrtCheckMemory());

    BOOL bRet = Initialize();
    if (bRet)
    {
        ProcessTasks();
        Cleanup();
    }

    //_ASSERTE(_CrtCheckMemory());

    //PostQuitMessage(0);
}

BEGIN_MESSAGE_MAP(CTaskManager, CWinThread)
    ON_THREAD_MESSAGE(WM_STARTWORK, OnStartWork)
END_MESSAGE_MAP()


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::Initialize
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

BOOL CTaskManager::Initialize()
{
    CMD_LOG(_T("INITIALIZATION: %s"), _T(__FUNCTION__));

    m_listNewTasks.clear();

    m_hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    if (m_hEvent == NULL)
    {
        return FALSE;
    }    

    m_pDriver = new CDriverInterface();
    BOOL bResult = m_pDriver->Initialize(m_eTypeOpenBy, m_pvOpenByParam);
    if (!bResult) 
    {
        if (m_pUIManager)
        {
            LaunchUIDeviceDetectionThread();
        }
        Cleanup();
        return FALSE;
    }

    //if (m_bCyclePort)
    //{
    //    m_bCyclePort = FALSE;
    //    m_pDriver->CyclePort();
    //    Cleanup();
    //    return FALSE;
    //}

    bResult = m_pDriver->GetEP(&m_ucNumReadEP, &m_ucNumWriteEP, m_ucReadEP, m_ucWriteEP);
    if (!bResult) 
    {
        if (m_pUIManager)
        {
            LaunchUIDeviceDetectionThread();
        }
        Cleanup();
        return FALSE;
    }

    m_pDriver->GetFirmwareVersion(&m_ulFirmwareVersion);
	m_pDriver->GetDriverVersion(&m_ulDriverVersion);
	m_pDriver->GetLibraryVersion(&m_ulLibraryVersion);
	m_pDriver->GetVIDPID(&m_uwVID, &m_uwPID);
    m_bIsUsb3 = m_pDriver->IsUsb3();
	m_pDriver->GetInterfaceType(m_pDriver->m_dwDeviceIndex, (DWORD *) &m_eIntfType);
    UIDeviceDetectionThread(this);

    if (m_ucNumWriteEP)
    {
        m_pWriterThreads = new CWriterThread(this, 0x02);
        m_pWriterThreads->CreateThread();
    }
    if (m_ucNumReadEP)
    {
        m_pReaderThreads = new CReaderThread(this, 0x82);
        m_pReaderThreads->CreateThread();
    }

    m_bInitializeDone = TRUE;
    m_bCleanupDone = FALSE;
    m_bCleanupOngoing = FALSE;

    return TRUE;
}




////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::LaunchUICompletionThread
//
// Summary
//      Launches the UI device detection thread
//
// Parameters
//
// Return Value
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

VOID CTaskManager::LaunchUIDeviceDetectionThread()
{
    HANDLE hdl = (HANDLE)::_beginthreadex(NULL, 0, (PTHREAD_START)UIDeviceDetectionThread, this, 0, NULL);
    //CloseHandle(hdl);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::UIDeviceDetectionThread
//
// Summary
//      Performs the UI device detection routine
//
// Parameters
//
// Return Value
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

DWORD CTaskManager::UIDeviceDetectionThread(PVOID a_pvParam)
{
    CTaskManager* pObj = (CTaskManager*)a_pvParam;
    if (pObj)
    {
        DeviceDetectionRoutineParams param;
        param.m_pucReadEP = pObj->m_ucReadEP;
        param.m_pucWriteEP = pObj->m_ucWriteEP;
        param.m_ucNumReadEP = pObj->m_ucNumReadEP;
        param.m_ucNumWriteEP = pObj->m_ucNumWriteEP;
        param.m_uwPID = pObj->m_uwPID;
        param.m_uwVID = pObj->m_uwVID;
        param.m_ulFirmwareVersion = pObj->m_ulFirmwareVersion;
		param.m_ulDriverVersion = pObj->m_ulDriverVersion;
		param.m_ulLibraryVersion = pObj->m_ulLibraryVersion;
		param.m_bIsUsb3 = pObj->m_bIsUsb3;
		param.m_eIntfType = pObj->m_eIntfType;
        pObj->m_pUIManager->DeviceDetectionRoutine(&param);
    }

    return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::Cleanup
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

VOID CTaskManager::Cleanup()
{
    if (m_bCleanupDone)
    {
        return;
    }
    CUtils::CCriticalSectionHolder cs(&m_csCleanup);

    CMD_LOG(_T("TERMINATION: %s"), _T(__FUNCTION__));

    // Terminate writer threads
    if (m_pWriterThreads)
    {
        m_pWriterThreads->ExitThread();
        m_pWriterThreads = NULL;
    }

    // Terminate reader threads
    if (m_pReaderThreads)
    {
        m_pReaderThreads->ExitThread();
        m_pReaderThreads = NULL;
    }

    CMD_LOG(_T("TERMINATION: %s 2"), _T(__FUNCTION__));

    try
    {
        if (m_hEvent)
        {
            ::CloseHandle(m_hEvent);
            m_hEvent = NULL;
        }
    }
    catch(int)
    {
    }

    // Cleanup list for new tasks
    {
        CUtils::CCriticalSectionHolder cs(&m_csNewTasks);
        while (!m_listNewTasks.empty())
        {
            PTTASK_MANAGER_PARAM pTask = m_listNewTasks.front();
            m_listNewTasks.pop_front();
            if (pTask)
            {
                delete pTask;
                pTask = NULL;
            }
        }
        m_listNewTasks.clear();
    }

    // Cleanup list for ongoing tasks
    {
        CUtils::CCriticalSectionHolder cs(&m_csOngoingTasks);
        while (!m_listOngoingTasks.empty())
        {
            PTTASK_MANAGER_PARAM pTask = m_listOngoingTasks.front();
            m_listOngoingTasks.pop_front();
            if (pTask)
            {
                delete pTask;
                pTask = NULL;
            }
        }
        m_listOngoingTasks.clear();
    }

    CMD_LOG(_T("TERMINATION: %s 3"), _T(__FUNCTION__));

    if (m_pDriver)
    {
        m_pDriver->Cleanup();
        delete m_pDriver;
        m_pDriver = NULL;
    }

    m_bInitializeDone = FALSE;

    m_bCleanupDone = TRUE;

    CMD_LOG(_T("TERMINATION: %s 4"), _T(__FUNCTION__));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::ExitThread
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

BOOL CTaskManager::ExitThread()
{
    CMD_LOG(_T("TERMINATION: %s"), _T(__FUNCTION__));

    if (m_hEvent)
    {
        ::CloseHandle(m_hEvent);
        m_hEvent = NULL;
    }

    // Wait for main thread to exit loop and to perform its cleanup
    while (!m_bCleanupDone)
    {
        Sleep(1);
    }

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::AddTask
//
// Summary
//      Called by the UI Manager thread.
//      Saves the task to a linked list.
//
// Parameters
//      a_pTask     - Task to be processed
//
// Return Value
//      TRUE if successful, otherwise FALSE
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CTaskManager::AddTask(CONST PTTASK_MANAGER_PARAM a_pTask)
{
#if READ_OR_WRITE_ONLY
    // Add restriction that no new transfer should be added if an existing transfer is ongoing
    if (a_pTask->m_eAction == ETASK_TYPE_IO_START)
    {
        CUtils::CCriticalSectionHolder cs(&m_csOngoingTasks);
        if (!m_listOngoingTasks.empty())
        {
            return FALSE;
        }
    }
#endif // READ_OR_WRITE_ONLY

    CUtils::CCriticalSectionHolder cs(&m_csNewTasks);

    if (!a_pTask || 
        !m_bInitializeDone || m_bCleanupDone)
    {
        return FALSE;
    }

    // Add task to the task list
    PTTASK_MANAGER_PARAM pTask = new TTASK_MANAGER_PARAM;
    memcpy(pTask, a_pTask, sizeof(TTASK_MANAGER_PARAM));
    pTask->m_llTaskNumber.QuadPart = ++m_llTaskNumber.QuadPart;
    m_listNewTasks.push_back(pTask);

    // Send a signal that task is available
    BOOL bResult = ::SetEvent(m_hEvent);
    if (!bResult)
    {
        return FALSE;
    }

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::ProcessTasks
//
// Summary
//      Main thread of the Task Manager
//      Waits for tasks from UI Manager to process 
//
// Parameters
//      None
//
// Return Value
//      None
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

VOID CTaskManager::ProcessTasks()
{
    DWORD dwResult = 0;


    while (!m_bCleanupOngoing && m_hEvent)
    {
        // Wait for signal that task is available
        dwResult = ::WaitForSingleObject(m_hEvent, 100);
        if (dwResult == WAIT_TIMEOUT || dwResult == WAIT_OBJECT_0)
        {
            PTTASK_MANAGER_PARAM pTask = NULL;

            // Get a task from list of new tasks
            {
                CUtils::CCriticalSectionHolder csNew(&m_csNewTasks);
                if (m_listNewTasks.empty())
                {
                    continue;
                }

                pTask = m_listNewTasks.front();
                if (pTask)
                {
                    m_listNewTasks.pop_front();
                }
            }

            if (!pTask)
            {
                CMD_LOG(_T("ProcessTasks(): No task available!\n"));
                continue;
            }

            if (m_bCleanupOngoing)
            {
                delete pTask;
                break;
            }

            // Process the task
            ProcessATask(pTask);

            if (pTask->m_eAction == ETASK_TYPE_IO_START)
            {
                CUtils::CCriticalSectionHolder csNew(&m_csOngoingTasks);
                m_listOngoingTasks.push_back(pTask);
            }
            else
            {
                delete pTask;
            }
            continue;
        }
        else
        {
            break;
        }
    }

    CMD_LOG(_T("TERMINATION: %s"), _T(__FUNCTION__));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::ProcessATask
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

BOOL CTaskManager::ProcessATask(CONST PTTASK_MANAGER_PARAM a_pTask)
{
    BOOL bResult = FALSE;


    if (!a_pTask)
    {
        return FALSE;
    }

    switch(a_pTask->m_eAction)
    {
        case ETASK_TYPE_IO_START: // fall through
        case ETASK_TYPE_IO_STOP:  // fall through
        {
            if (a_pTask->m_bIsWrite)
            {
                bResult = ProcessAWriteTask(a_pTask);
            }
            else
            {
                bResult = ProcessAReadTask(a_pTask);
            }
            break;
        }
        default:
        {
            break;
        }
    }

    return bResult;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::ProcessAWriteTask
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

BOOL CTaskManager::ProcessAWriteTask(CONST PTTASK_MANAGER_PARAM a_pTask)
{
    if (!a_pTask)
    {
        return FALSE;
    }


    BOOL bResult = FALSE;

    if (ETASK_TYPE_IO_START == a_pTask->m_eAction)
    {
        bResult = ProcessAWriteStartTask(a_pTask);
    }
    else //if (ETASK_TYPE_IO_STOP == a_pTask->m_eAction)
    {
        bResult = ProcessAWriteStopTask(a_pTask);
    }

    return bResult;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::ProcessAReadTask
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

BOOL CTaskManager::ProcessAReadTask(CONST PTTASK_MANAGER_PARAM a_pTask)
{
    if (!a_pTask)
    {
        return FALSE;
    }


    BOOL bResult = FALSE;

    if (ETASK_TYPE_IO_START == a_pTask->m_eAction)
    {
        bResult = ProcessAReadStartTask(a_pTask);
    }
    else //if (ETASK_TYPE_IO_STOP == a_pTask->m_eAction)
    {
        bResult = ProcessAReadStopTask(a_pTask);
    }

    return bResult;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::ProcessAWriteStartTask
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

BOOL CTaskManager::ProcessAWriteStartTask(CONST PTTASK_MANAGER_PARAM a_pTask)
{
    if (!a_pTask)
    {
        return FALSE;
    }


    BOOL bResult = FALSE;

    if (!m_pWriterThreads)
    {
        return FALSE;
    }

    bResult = m_pWriterThreads->SetTaskCompletionIdentifier(&a_pTask->m_llTaskNumber);
    bResult = m_pWriterThreads->SetTaskTestParam(&a_pTask->m_TestParam);
	bResult = m_pWriterThreads->SetTaskPayloadType(&a_pTask->m_PayloadParam);
    bResult = m_pWriterThreads->StartTask();
    return bResult;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::ProcessAWriteStopTask
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

BOOL CTaskManager::ProcessAWriteStopTask(CONST PTTASK_MANAGER_PARAM a_pTask)
{
    if (!a_pTask)
    {
        return FALSE;
    }


    BOOL bResult = FALSE;

    if (!m_pWriterThreads)
    {
        return FALSE;
    }

    bResult = m_pWriterThreads->StopTask();
    if (!bResult)
    {
        if (m_pDriver)
        {
            CMD_LOG(_T("FORCE CLOSE: %s"), _T(__FUNCTION__));
            m_pDriver->Cleanup();
            delete m_pDriver;
            m_pDriver = NULL;
        }
    }

    return bResult;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::ProcessAReadStartTask
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

BOOL CTaskManager::ProcessAReadStartTask(CONST PTTASK_MANAGER_PARAM a_pTask)
{
    if (!a_pTask)
    {
        return FALSE;
    }


    BOOL bResult = FALSE;

    if (!m_pReaderThreads)
    {
        return FALSE;
    }

    bResult = m_pReaderThreads->SetTaskCompletionIdentifier(&a_pTask->m_llTaskNumber);
    bResult = m_pReaderThreads->SetTaskTestParam(&a_pTask->m_TestParam);
    bResult = m_pReaderThreads->StartTask();

    return bResult;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::ProcessAReadStopTask
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

BOOL CTaskManager::ProcessAReadStopTask(CONST PTTASK_MANAGER_PARAM a_pTask)
{
    if (!a_pTask)
    {
        return FALSE;
    }


    BOOL bResult = FALSE;

    if (!m_pReaderThreads)
    {
        return FALSE;
    }

    bResult = m_pReaderThreads->StopTask();
    if (!bResult)
    {
        if (m_pDriver)
        {
            CMD_LOG(_T("FORCE CLOSE: %s"), _T(__FUNCTION__));
            m_pDriver->Cleanup();
            delete m_pDriver;
            m_pDriver = NULL;
        }
    }

    return bResult;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::GetVIDPID
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

BOOL CTaskManager::GetVIDPID(PUSHORT a_puwVID, PUSHORT a_puwPID)
{
    *a_puwVID = m_uwVID;
    *a_puwPID = m_uwPID;
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::IsDevicePath
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

BOOL CTaskManager::IsDevicePath(CONST CHAR* pucDevicePath)
{
	if (m_pDriver)
	{
		return m_pDriver->IsDevicePath(pucDevicePath);
	}

	return FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::CompletionRoutine
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

BOOL CTaskManager::CompletionRoutine(CONST PTTASK_COMPLETION_PARAM a_pCompletionParam)
{
    if (m_bCleanupOngoing)
    {
        return FALSE;
    }


    EnterCriticalSection(&m_csCompletionRoutine);
    LaunchUICompletionThread(a_pCompletionParam->m_ucEP);
    m_bCyclePort = a_pCompletionParam->m_eStatus == ETASK_STATUS_UNPLUGGED;
    LeaveCriticalSection(&m_csCompletionRoutine);


    EnterCriticalSection(&m_csOngoingTasks);
    if (!m_listOngoingTasks.empty())
    {
        PTTASK_MANAGER_PARAM pTask = m_listOngoingTasks.front();
        if (pTask)
        {
            m_listOngoingTasks.pop_front();
        }
        delete pTask;
    }
    LeaveCriticalSection(&m_csOngoingTasks);

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::LaunchUICompletionThread
//
// Summary
//      Launches the UI completion thread
//      UI completion thread will delete UICompletionParam and the PTTASK_MANAGER_PARAM task
//
// Parameters
//
// Return Value
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

VOID CTaskManager::LaunchUICompletionThread(UCHAR a_ucEP)
{
    UICompletionParam *pParam = new UICompletionParam;
    pParam->pThis = this;
    pParam->ucEP = a_ucEP;

    // UI thread will delete both UICompletionParam and PTTASK_MANAGER_PARAM
    HANDLE hdl = (HANDLE)::_beginthreadex(NULL, 0, (PTHREAD_START)UICompletionThread, pParam, 0, NULL);
    //CloseHandle(hdl);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::UICompletionThread
//
// Summary
//      Performs the UI completion routine
//      Deletes the task input and the completion param input
//
// Parameters
//
// Return Value
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

DWORD CTaskManager::UICompletionThread(PVOID a_pvParam)
{
    UICompletionParam* pObj = (UICompletionParam*)a_pvParam;
    if (pObj)
    {
        CUtils::CCriticalSectionHolder csOngoing(&pObj->pThis->m_csUICompletionRoutine);
        pObj->pThis->m_pUIManager->CompletionRoutine(pObj->ucEP);
        delete pObj;
        pObj = NULL;
    }

    return 0;
}



