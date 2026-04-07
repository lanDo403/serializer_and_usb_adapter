/*
 * FT600 Data Loopback Demo App
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
#include "APP_PayloadRecording.h"
#include "APP_PayloadVerification.h"
#include "APP_LoopbackMonitoring.h"
#include "APP_Utils.h"



IMPLEMENT_DYNCREATE(CTaskManager, CWinThread)



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

    memset(m_pWriterThreads, 0, sizeof(m_pWriterThreads));
    memset(m_pReaderThreads, 0, sizeof(m_pReaderThreads));
    m_llTaskNumber.QuadPart = 0;

    memset(&m_ucWriteEP, 0, sizeof(m_ucWriteEP));
    memset(&m_ucReadEP, 0, sizeof(m_ucReadEP));
    m_ucNumWriteEP = 0;
    m_ucNumReadEP = 0;
    m_uwVID = 0;
    m_uwPID = 0;

    memset(m_pWritePayloadRecorder, 0, sizeof(m_pWritePayloadRecorder));
    memset(m_pReadPayloadRecorder, 0, sizeof(m_pReadPayloadRecorder));

    memset(m_pLoopbackMonitor, 0, sizeof(m_pLoopbackMonitor));

    ::InitializeCriticalSection(&m_csNewTasks);
    ::InitializeCriticalSection(&m_csOngoingTasks);
    ::InitializeCriticalSection(&m_csCompletionRoutine);
    ::InitializeCriticalSection(&m_csCleanup);
    ::InitializeCriticalSection(&m_csUICompletionRoutine);
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
    //::DeleteCriticalSection(&m_csOngoingTasks);
    //::DeleteCriticalSection(&m_csNewTasks);
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
    m_listOngoingTasks.clear();

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

    bResult = m_pDriver->GetEP(&m_ucNumReadEP, &m_ucNumWriteEP, m_ucReadEP, m_ucWriteEP, m_uwReadMPS, m_uwWriteMPS);
    if (!bResult) 
    {
        if (m_pUIManager)
        {
            LaunchUIDeviceDetectionThread();
        }
        Cleanup();
        return FALSE;
    }

    m_pDriver->GetVIDPID(&m_uwVID, &m_uwPID);
    m_pDriver->GetFirmwareVersion(&m_ulFirmwareVersion);
	m_pDriver->GetDriverVersion(&m_ulDriverVersion);
	m_pDriver->GetLibraryVersion(&m_ulLibraryVersion);

    LaunchUIDeviceDetectionThread();

    for (int i=0; i<m_ucNumWriteEP; i++)
    {
        m_pWritePayloadRecorder[i] = new CPayloadRecorder(m_ucWriteEP[i]);
        m_pWriterThreads[i] = new CWriterThread(this, m_ucWriteEP[i], m_pWritePayloadRecorder[i]);
        m_pWriterThreads[i]->CreateThread();
    }

    for (int i=0; i<m_ucNumReadEP; i++)
    {
        m_pReadPayloadRecorder[i] = new CPayloadRecorder(m_ucReadEP[i]);
        m_pReaderThreads[i] = new CReaderThread(this, m_ucReadEP[i], m_pReadPayloadRecorder[i], m_uwReadMPS[i]);
        m_pReaderThreads[i]->CreateThread();
    }

    for (int i=0; i<MAX(m_ucNumWriteEP, m_ucNumReadEP); i++)
    {
        m_pLoopbackMonitor[i] = new CLoopbackMonitor(m_ucWriteEP[i], m_ucReadEP[i]);
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
    CloseHandle(hdl);
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

    // Close driver handle
    if (m_pDriver)
    {
        m_pDriver->Cleanup();
    }

    // Terminate writer threads
    for (UCHAR i=0; i<m_ucNumWriteEP; i++)
    {
        if (m_pWriterThreads[i])
        {
            m_pWriterThreads[i]->ExitThread();
            // no longer need to delete delete 
            // since calling ExitThread will trigger destructor
            //delete m_pWriterThreads[i]; 
            m_pWriterThreads[i] = NULL;
        }

        if (m_pWritePayloadRecorder[i])
        {
            delete m_pWritePayloadRecorder[i];
            m_pWritePayloadRecorder[i] = NULL;
        }
    }

    // Terminate reader threads
    for (UCHAR i=0; i<m_ucNumReadEP; i++)
    {
        if (m_pReaderThreads[i])
        {
            m_pReaderThreads[i]->ExitThread();
            // no longer need to delete delete 
            // since calling ExitThread will trigger destructor
            //delete m_pReaderThreads[i];
            m_pReaderThreads[i] = NULL;
        }

        if (m_pReadPayloadRecorder[i])
        {
            delete m_pReadPayloadRecorder[i];
            m_pReadPayloadRecorder[i] = NULL;
        }
    }


    for (UCHAR i=0; i<MAX(m_ucNumWriteEP, m_ucNumReadEP); i++)
    {
        if (m_pLoopbackMonitor[i])
        {
            delete m_pLoopbackMonitor[i];
            m_pLoopbackMonitor[i] = NULL;
        }
    }

    if (m_hEvent)
    {
        ::CloseHandle(m_hEvent);
        m_hEvent = NULL;
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

    if (m_pDriver)
    {
        delete m_pDriver;
        m_pDriver = NULL;
    }

    m_bInitializeDone = FALSE;

    m_bCleanupDone = TRUE;
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
    //CMD_LOG(_T("TERMINATION: %s"), _T(__FUNCTION__));

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
        dwResult = ::WaitForSingleObject(m_hEvent, 50);
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

            // Handle case when read takes place first before write
            if (pTask->m_bStressTest)
            {
                if (pTask->m_bIsWrite && pTask->m_eAction == ETASK_TYPE_IO_START)
                {
                    m_bLoopbackPending[pTask->m_ucEP - BASE_WRITE_EP] = TRUE;
                }
                else if (!pTask->m_bIsWrite && pTask->m_eAction == ETASK_TYPE_IO_START)
                {
                    if (m_bLoopbackPending[pTask->m_ucEP - BASE_READ_EP])
                    {
                        m_bLoopbackPending[pTask->m_ucEP - BASE_READ_EP] = FALSE;
                    }
                    else 
                    {
                        // has no pending data for reading
                        if (!m_pLoopbackMonitor[pTask->m_ucEP - BASE_READ_EP]->HasPendingData())
                        {
                            // requeue the task at end of queue
                            {
                                CUtils::CCriticalSectionHolder csNew(&m_csNewTasks);
                                m_listNewTasks.push_back(pTask);
                            }

                            continue;
                        }
                    }
                }
            }

            if (m_bCleanupOngoing)
            {
                delete pTask;
                break;
            }

            // Process the task
            BOOL bResult = ProcessATask(pTask);
            if (!bResult)
            {
                //CMD_LOG(_T("[EP%02X] ProcessTasks(): ProcessATask failed!\n"), pTask->m_ucEP);
                delete pTask;
                continue;
            }

            // Enqueue task to ongoing tasks if necessary
            switch(pTask->m_eAction)
            {
                case ETASK_TYPE_IO_START:
                {
                    UCHAR ucEP = pTask->m_ucEP;
                    BOOL bStress = pTask->m_bStressTest;
                    BOOL bWrite = pTask->m_bIsWrite;

                    EnqueueOngoingTask(pTask);

                    // Wait for write thread to finish if stress test mode
                    if (bStress)
                    {
                        if (bWrite && pTask->m_TestParam.m_llSessionLength.QuadPart <= 32768)
                        {
                            BOOL bWriteOngoing = TRUE;
                            do
                            {
                                bWriteOngoing = IsExistOngoingTask(ucEP);
                                Sleep(16);
                            } while (bWriteOngoing && !m_bCleanupOngoing && m_hEvent);
                        }
                    }

                    break;
                }
                case ETASK_TYPE_IO_STOP:        // fallthrough
                case ETASK_TYPE_IO_FLUSH:       // fallthrough
                default:
                {
                    delete pTask;
                    break;
                }
            }
        }
        else
        {
            break;
        }
    }

    //CMD_LOG(_T("TERMINATION: %s"), _T(__FUNCTION__));
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
        case ETASK_TYPE_IO_FLUSH:
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
    else if (ETASK_TYPE_IO_STOP == a_pTask->m_eAction)
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
    else if (ETASK_TYPE_IO_STOP == a_pTask->m_eAction)
    {
        bResult = ProcessAReadStopTask(a_pTask);
    }
    else if (ETASK_TYPE_IO_FLUSH == a_pTask->m_eAction)    
    {
        bResult = ProcessAReadFlushTask(a_pTask);
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


    UCHAR ucWriteEndpointIndex = a_pTask->m_ucEP-BASE_WRITE_EP;
    BOOL bResult = FALSE;

    if (!m_pWriterThreads[ucWriteEndpointIndex])
    {
        return FALSE;
    }

    if (m_pLoopbackMonitor[ucWriteEndpointIndex]->IsLoopbackReset())
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        m_pWritePayloadRecorder[ucWriteEndpointIndex]->UpdateFileName(&st);
        if (m_pReadPayloadRecorder[ucWriteEndpointIndex])
        {
            m_pReadPayloadRecorder[ucWriteEndpointIndex]->UpdateFileName(&st);
        }
    }

    m_pLoopbackMonitor[ucWriteEndpointIndex]->SetWriteOngoing(TRUE);
    bResult = m_pWriterThreads[ucWriteEndpointIndex]->SetTaskCompletionIdentifier(&a_pTask->m_llTaskNumber);
    bResult = m_pWriterThreads[ucWriteEndpointIndex]->SetTaskTestParam(&a_pTask->m_TestParam);
    bResult = m_pWriterThreads[ucWriteEndpointIndex]->SetTaskPayloadType(&a_pTask->m_PayloadParam);
    bResult = m_pWriterThreads[ucWriteEndpointIndex]->StartTask();
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


    UCHAR ucWriteEndpointIndex = a_pTask->m_ucEP-BASE_WRITE_EP;
    BOOL bResult = FALSE;

    if (!m_pWriterThreads[ucWriteEndpointIndex])
    {
        return FALSE;
    }

    bResult = m_pWriterThreads[ucWriteEndpointIndex]->StopTask(FALSE);

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


    UCHAR ucReadEndpointIndex = a_pTask->m_ucEP-BASE_READ_EP;
    BOOL bResult = FALSE;

    if (!m_pReaderThreads[ucReadEndpointIndex])
    {
        return FALSE;
    }

    if (m_pLoopbackMonitor[ucReadEndpointIndex]->IsLoopbackReset())
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        if (m_pWritePayloadRecorder[ucReadEndpointIndex])
        {
            m_pWritePayloadRecorder[ucReadEndpointIndex]->UpdateFileName(&st);
        }
        m_pReadPayloadRecorder[ucReadEndpointIndex]->UpdateFileName(&st);
    }

    bResult = m_pReaderThreads[ucReadEndpointIndex]->SetTaskCompletionIdentifier(&a_pTask->m_llTaskNumber);
    bResult = m_pReaderThreads[ucReadEndpointIndex]->SetTaskTestParam(&a_pTask->m_TestParam);
    bResult = m_pReaderThreads[ucReadEndpointIndex]->StartTask();

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


    UCHAR ucReadEndpointIndex = a_pTask->m_ucEP-BASE_READ_EP;
    BOOL bResult = FALSE;

    if (!m_pReaderThreads[ucReadEndpointIndex])
    {
        return FALSE;
    }

    bResult = m_pReaderThreads[ucReadEndpointIndex]->StopTask(FALSE);

    return bResult;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::ProcessAReadFlushTask
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

BOOL CTaskManager::ProcessAReadFlushTask(CONST PTTASK_MANAGER_PARAM a_pTask)
{
    if (!a_pTask)
    {
        return FALSE;
    }


    UCHAR ucReadEndpointIndex = a_pTask->m_ucEP-BASE_READ_EP;
    BOOL bResult = FALSE;

    if (!m_pReaderThreads[ucReadEndpointIndex])
    {
        return FALSE;
    }

    bResult = m_pReaderThreads[ucReadEndpointIndex]->StopTask(TRUE);
    m_pLoopbackMonitor[ucReadEndpointIndex]->Reset();

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
// CTaskManager::CompletionRoutine_VerifyResult
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

BOOL CTaskManager::CompletionRoutine_VerifyResult(PTTASK_MANAGER_PARAM a_pTask)
{
    BOOL bFilesSame = FALSE;
    ULARGE_INTEGER ullWriteFileSize = {0};
    ULARGE_INTEGER ullReadFileSize = {0};


    UCHAR ucReadEndpointIndex = a_pTask->m_ucEP-BASE_READ_EP;
    if (!m_pWritePayloadRecorder[ucReadEndpointIndex]->PathFileExist() || 
        !m_pReadPayloadRecorder[ucReadEndpointIndex]->PathFileExist())
    {
        return TRUE;
    }

    m_pWritePayloadRecorder[ucReadEndpointIndex]->GetFileSize(&ullWriteFileSize);
    m_pReadPayloadRecorder[ucReadEndpointIndex]->GetFileSize(&ullReadFileSize);
    m_pWritePayloadRecorder[ucReadEndpointIndex]->Close();
    m_pReadPayloadRecorder[ucReadEndpointIndex]->Close();

    CPayloadVerifier cVerifier;
    BOOL bFilesExist = FALSE;
    BOOL bResult = cVerifier.CompareFiles(m_pWritePayloadRecorder[ucReadEndpointIndex]->GetFileName(), 
                                          m_pReadPayloadRecorder[ucReadEndpointIndex]->GetFileName(), 
                                          &bFilesExist, FALSE);//a_pTask->m_TestParam.m_bStream);
    a_pTask->m_bFilesMatch = bResult;
    if (bFilesExist)
    {
        if (bResult)
        {
            APP_LOG(_T("[%02X%02X] RESULT: Verified files do match! PASSED!"), 
                ucReadEndpointIndex+BASE_WRITE_EP, a_pTask->m_ucEP);
            bFilesSame = TRUE;
        }
        else
        {
            APP_LOG(_T("[%02X%02X] RESULT: Verified files do not match! FAILED!"), 
                ucReadEndpointIndex+BASE_WRITE_EP, a_pTask->m_ucEP);
        }
    }
    else
    {
        APP_LOG(_T("[%02X%02X] RESULT: Files do not exist! FAILED!"), 
            ucReadEndpointIndex+BASE_WRITE_EP, a_pTask->m_ucEP);
    }
    CMD_LOG(_T(""));

    // If stress test mode, delete files if files are similar
    if (a_pTask->m_bStressTest) // stresstest mode
    {
        if (bFilesSame) // successful
        {
            // delete files
            cVerifier.DeleteFiles(m_pWritePayloadRecorder[ucReadEndpointIndex]->GetFileName(), 
                                  m_pReadPayloadRecorder[ucReadEndpointIndex]->GetFileName());
        }
        else
        {
            if (!bFilesExist)
            {
                APP_LOG(_T("[%02X%02X] NOTE: Ignoring comparison because file could not be located!\n"), 
                    ucReadEndpointIndex+BASE_WRITE_EP, a_pTask->m_ucEP);
                a_pTask->m_bFilesMatch = TRUE;
            }
        }
    }
    else
    {
        DWORD dwMaxSize = (ucReadEndpointIndex == 0) ? MAX_PAYLOAD_SIZE_CH1 : MAX_PAYLOAD_SIZE;
        if (ullWriteFileSize.QuadPart > dwMaxSize || 
            ullReadFileSize.QuadPart > dwMaxSize )
        {
            // delete files
            cVerifier.DeleteFiles(m_pWritePayloadRecorder[ucReadEndpointIndex]->GetFileName(), 
                                  m_pReadPayloadRecorder[ucReadEndpointIndex]->GetFileName());
        }
    }

    return bFilesSame;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::EnqueueOngoingTask
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

VOID CTaskManager::EnqueueOngoingTask(PTTASK_MANAGER_PARAM a_pTask)
{
    CUtils::CCriticalSectionHolder csOngoing(&m_csOngoingTasks);
    m_listOngoingTasks.push_back(a_pTask);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::DequeueOngoingTask
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

VOID CTaskManager::DequeueOngoingTask(CONST PTTASK_COMPLETION_PARAM a_pCompletionParam, PTTASK_MANAGER_PARAM *a_pTask)
{
    CUtils::CCriticalSectionHolder csOngoing(&m_csOngoingTasks);

    if (m_listOngoingTasks.empty())
    {
        return;
    }

    std::list<PTTASK_MANAGER_PARAM>::iterator iter = m_listOngoingTasks.begin();
    while (iter != m_listOngoingTasks.end())
    {
        *a_pTask = *iter;
        if ((*a_pTask)->m_llTaskNumber.QuadPart == a_pCompletionParam->m_llTaskNumber.QuadPart)
        {
            m_listOngoingTasks.erase(iter++);
            break;
        }

        iter++;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::DequeueOngoingTask
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

VOID CTaskManager::DequeueOngoingTask(UCHAR a_ucEP, PTTASK_MANAGER_PARAM *a_pTask)
{
    CUtils::CCriticalSectionHolder csOngoing(&m_csOngoingTasks);

    if (m_listOngoingTasks.empty())
    {
        return;
    }

    std::list<PTTASK_MANAGER_PARAM>::iterator iter = m_listOngoingTasks.begin();
    while (iter != m_listOngoingTasks.end())
    {
        *a_pTask = *iter;
        if ((*a_pTask)->m_ucEP == a_ucEP)
        {
            m_listOngoingTasks.erase(iter++);
            break;
        }

        iter++;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::IsExistOngoingTask
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

BOOL CTaskManager::IsExistOngoingTask(UCHAR a_ucEP)
{
    CUtils::CCriticalSectionHolder csOngoing(&m_csOngoingTasks);

    if (m_listOngoingTasks.empty())
    {
        return FALSE;
    }

    std::list<PTTASK_MANAGER_PARAM>::iterator iter = m_listOngoingTasks.begin();
    while (iter != m_listOngoingTasks.end())
    {
        PTTASK_MANAGER_PARAM a_pTask = *iter;
        if (a_pTask->m_ucEP == a_ucEP)
        {
            return TRUE;
        }

        iter++;
    }

    return FALSE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::IsLoopbackDataMisMatch
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

BOOL CTaskManager::IsLoopbackDataMisMatch(UCHAR a_ucEP)
{
    UCHAR ucEPIndex = a_ucEP - BASE_WRITE_EP;
    return m_pLoopbackMonitor[ucEPIndex]->IsFileMismatch();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskManager::SetLoopbackData
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

BOOL CTaskManager::SetLoopbackData(BOOL a_bWrite, UCHAR a_ucEP, PLARGE_INTEGER a_pllPayloadTransferred, ETASK_PAYLOAD_TYPE a_eType)
{
    BOOL bRet = TRUE;


    if (a_bWrite)
    {
        UCHAR ucEPIndex = a_ucEP - BASE_WRITE_EP;

        // Record the payload written 
        m_pLoopbackMonitor[ucEPIndex]->AddWrittenPayload(a_pllPayloadTransferred, a_eType);
        m_pLoopbackMonitor[ucEPIndex]->SetWriteOngoing(FALSE);
    }
    else
    {
        UCHAR ucEPIndex = a_ucEP - BASE_READ_EP;

        // Record the payload read
        m_pLoopbackMonitor[ucEPIndex]->AddReadPayload(a_pllPayloadTransferred);

        if (m_pLoopbackMonitor[ucEPIndex]->IsLoopbackReset())
        {
            TTASK_MANAGER_PARAM oTask = {0};
            oTask.m_ucEP = a_ucEP;
            oTask.m_bStressTest = TRUE;
            bRet = CompletionRoutine_VerifyResult(&oTask);
            if (!bRet)
            {
                m_pLoopbackMonitor[ucEPIndex]->SetFileMismatch();
            }
        }
    }

    return bRet;
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
    PTTASK_MANAGER_PARAM pTask = NULL;


    //
    // Dequeue corresponding element from m_listOngoingTasks
    //
    DequeueOngoingTask(a_pCompletionParam, &pTask);
    if (!pTask)
    {
        CMD_LOG(_T("CompletionRoutine(): Not found!\n"));
        LeaveCriticalSection(&m_csCompletionRoutine);
        return FALSE;
    }
    if (pTask->m_ucEP > 0x81 + NUM_EP_PAIRS)
    {
        LeaveCriticalSection(&m_csCompletionRoutine);
        return FALSE;
    }

    //
    // Record completed data to the LoopbackMonitor
    //
    if (a_pCompletionParam->m_dwTimeMs == 0)
    {
        // prevent division by zero
        a_pCompletionParam->m_dwTimeMs = 1;
    }

    // Process write task
    if (pTask->m_bIsWrite)
    {
        UCHAR ucEPIndex = pTask->m_ucEP - BASE_WRITE_EP;

        APP_LOG(_T("[EP%02X] Written %I64d bytes to device!"), 
            pTask->m_ucEP, a_pCompletionParam->m_llPayloadTransferred.QuadPart);

        if (m_ucNumReadEP)
        {
            // Record the payload written 
            {
                m_pLoopbackMonitor[ucEPIndex]->AddWrittenPayload(&a_pCompletionParam->m_llPayloadTransferred, pTask->m_PayloadParam.m_eType);
                m_pLoopbackMonitor[ucEPIndex]->GetPendingData(&pTask->m_llPendingData);
                m_pLoopbackMonitor[ucEPIndex]->SetWriteOngoing(FALSE);

                // Check if there is a pended read task completion
                PTTASK_COMPLETION_PARAM pNewCompletionParam = m_pLoopbackMonitor[ucEPIndex]->IsWaitingForWriteCompletion();
                if (pNewCompletionParam)
                {
                    //
                    // Call the completion routine of the UI Manager
                    //
                    pTask->m_llPayloadTransferred.QuadPart = a_pCompletionParam->m_llPayloadTransferred.QuadPart;
                    pTask->m_eStatus = a_pCompletionParam->m_eStatus;

                    LaunchUICompletionThread(pTask);

                    pTask = NULL;
                    LeaveCriticalSection(&m_csCompletionRoutine);

                    //
                    // Process the completion of the pended read task completion
                    //
                    memcpy(a_pCompletionParam, pNewCompletionParam, sizeof(*pNewCompletionParam));
                    EnterCriticalSection(&m_csCompletionRoutine);
                    do
                    {
                        DequeueOngoingTask(ucEPIndex+BASE_READ_EP, &pTask);
                    } while (!pTask);

                    if (a_pCompletionParam->m_dwTimeMs == 0)
                    {
                        a_pCompletionParam->m_dwTimeMs = 1;
                    }
                }
            }
        }
        else
        {
            m_pWritePayloadRecorder[ucEPIndex]->Close();
            CPayloadVerifier cVerifier;
            cVerifier.DeleteFiles(m_pWritePayloadRecorder[ucEPIndex]->GetFileName(), NULL);

            pTask->m_llPayloadTransferred.QuadPart = 0;
            pTask->m_bFilesMatch = TRUE;

            m_pLoopbackMonitor[ucEPIndex]->Reset();
        }
    }

    // Process read task
    if (!pTask->m_bIsWrite)
    {
        UCHAR ucEPIndex = pTask->m_ucEP - BASE_READ_EP;
        LARGE_INTEGER llTemp = {0};


        if (m_ucNumWriteEP)
        {
            m_pLoopbackMonitor[ucEPIndex]->GetPendingData(&llTemp);

            if (llTemp.QuadPart == 0 && a_pCompletionParam->m_llPayloadTransferred.QuadPart != 0)
            {
                //
                // Pend the completion of the read thread later since the write thread has not officially been completed
                //

                if (a_pCompletionParam->m_eStatus == ETASK_STATUS_COMPLETED)
                {
                    m_pLoopbackMonitor[ucEPIndex]->SetWaitingForWriteCompletion(a_pCompletionParam);

                    CUtils::CCriticalSectionHolder csOngoing(&m_csOngoingTasks);
                    m_listOngoingTasks.push_back(pTask);

                    LeaveCriticalSection(&m_csCompletionRoutine);
                    return TRUE;
                }
            }
            else
            {
                APP_LOG(_T("[EP%02X] Read %I64d bytes from device!"),
                    pTask->m_ucEP, a_pCompletionParam->m_llPayloadTransferred.QuadPart);

                // Record the payload read
                m_pLoopbackMonitor[ucEPIndex]->AddReadPayload(&a_pCompletionParam->m_llPayloadTransferred);
                m_pLoopbackMonitor[ucEPIndex]->GetPendingData(&pTask->m_llPendingData);

                if (a_pCompletionParam->m_llPayloadTransferred.QuadPart != 0)
                {
                    if (m_pLoopbackMonitor[ucEPIndex]->IsLoopbackReset())
                    {
                        CompletionRoutine_VerifyResult(pTask);
                    }
                    else
                    {
                        // Prevent creating files more than 100MB

                        ULARGE_INTEGER ullWriteFileSize = {0};
                        ULARGE_INTEGER ullReadFileSize = {0};
                        m_pWritePayloadRecorder[ucEPIndex]->GetFileSize(&ullWriteFileSize);
                        m_pReadPayloadRecorder[ucEPIndex]->GetFileSize(&ullReadFileSize);

                        DWORD dwMaxSize = (ucEPIndex == 0)?MAX_PAYLOAD_SIZE_CH1:MAX_PAYLOAD_SIZE;
                        if (ullWriteFileSize.QuadPart > dwMaxSize || ullReadFileSize.QuadPart > dwMaxSize)
                        {
                            m_pWritePayloadRecorder[ucEPIndex]->Close();
                            m_pReadPayloadRecorder[ucEPIndex]->Close();
                            CPayloadVerifier cVerifier;
                            cVerifier.DeleteFiles(m_pWritePayloadRecorder[ucEPIndex]->GetFileName(), m_pReadPayloadRecorder[ucEPIndex]->GetFileName());

                            pTask->m_llPayloadTransferred.QuadPart = 0;
                            pTask->m_bFilesMatch = FALSE;

                            m_pLoopbackMonitor[ucEPIndex]->Reset();
                        }
                    }
                }
            }
        }
        else
        {
            APP_LOG(_T("[EP%02X] Read %I64d bytes from device!"),
                pTask->m_ucEP, a_pCompletionParam->m_llPayloadTransferred.QuadPart);

            m_pLoopbackMonitor[ucEPIndex]->Reset();

            m_pReadPayloadRecorder[ucEPIndex]->Close();
            CPayloadVerifier cVerifier;
            cVerifier.DeleteFiles(NULL, m_pReadPayloadRecorder[ucEPIndex]->GetFileName());

            pTask->m_llPayloadTransferred.QuadPart = 0;
            pTask->m_bFilesMatch = TRUE;

            m_pLoopbackMonitor[ucEPIndex]->Reset();
        }
    }

    //
    // Call the completion routine of the UI Manager
    //
    if (!m_bCleanupOngoing)
    {
        pTask->m_llPayloadTransferred.QuadPart = a_pCompletionParam->m_llPayloadTransferred.QuadPart;
        pTask->m_eStatus = a_pCompletionParam->m_eStatus;

        LaunchUICompletionThread(pTask);
    }

    LeaveCriticalSection(&m_csCompletionRoutine);
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

VOID CTaskManager::LaunchUICompletionThread(PTTASK_MANAGER_PARAM pTask)
{
    UICompletionParam *pParam = new UICompletionParam;
    pParam->pThis = this;
    pParam->pTask = pTask;

    // UI thread will delete both UICompletionParam and PTTASK_MANAGER_PARAM
    HANDLE hdl = (HANDLE)::_beginthreadex(NULL, 0, (PTHREAD_START)UICompletionThread, pParam, 0, NULL);
    CloseHandle(hdl);
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
        {
            CUtils::CCriticalSectionHolder csOngoing(&pObj->pThis->m_csUICompletionRoutine);
            pObj->pThis->m_pUIManager->CompletionRoutine(pObj->pTask);
        }

        delete pObj->pTask;
        pObj->pTask = NULL;

        delete pObj;
        pObj = NULL;
    }

    return 0;
}



