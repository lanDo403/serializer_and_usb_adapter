/*
 * FT600 Data Loopback Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#include "stdafx.h"
#include "APP_LoopbackMonitoring.h"
#include "APP_Task.h"
#include "APP_Utils.h"



////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CLoopbackMonitor::CLoopbackMonitor
//      This file implements the monitoring of the loopback status completion 
//      of both the Write threads and Read threads of a channel. 
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

CLoopbackMonitor::CLoopbackMonitor(UCHAR a_ucWriteEP, UCHAR a_ucReadEP):
    m_ucWriteEP(a_ucWriteEP),
    m_ucReadEP(a_ucReadEP)
{
    Reset();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CLoopbackMonitor::~CLoopbackMonitor
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

CLoopbackMonitor::~CLoopbackMonitor()
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CLoopbackMonitor::AddWrittenPayload
//
// Summary
//
// Parameters
//
// Return Value
//      TRUE if successful, FALSE if otherwise
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CLoopbackMonitor::AddWrittenPayload(PLARGE_INTEGER a_pllLength, ETASK_PAYLOAD_TYPE a_eType)
{
    if (a_pllLength->QuadPart == 0)
    {
        return TRUE;
    }


    if (m_llWriteLength.QuadPart == 0 && m_llReadLength.QuadPart == 0)
    {
        m_eType = a_eType;
    }
    else
    {
        if (m_eType != a_eType)
        {
            CMD_LOG(_T("[0x%02x] AddWrittenPayload failed! m_eType != a_eType\n"), m_ucWriteEP);
        }
    }

    m_llWriteLength.QuadPart += a_pllLength->QuadPart;
    CMD_LOG(_T("[EP%02x] Loopback status (W:%I64d R:%I64d)!"), m_ucWriteEP, m_llWriteLength.QuadPart, m_llReadLength.QuadPart);
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CLoopbackMonitor::AddReadPayload
//
// Summary
//
// Parameters
//
// Return Value
//      TRUE if successful, FALSE if otherwise
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CLoopbackMonitor::AddReadPayload(PLARGE_INTEGER a_pllLength)
{
    if (a_pllLength->QuadPart == 0)
    {
        return TRUE;
    }


    m_llReadLength.QuadPart += a_pllLength->QuadPart;

    if (m_llWriteLength.QuadPart == m_llReadLength.QuadPart)
    {
        CMD_LOG(_T("[EP%02x] Loopback status (W:%I64d R:%I64d) completed!"), m_ucReadEP, m_llWriteLength.QuadPart, m_llReadLength.QuadPart);
        Reset();
    }
    else
    {
        CMD_LOG(_T("[EP%02x] Loopback status (W:%I64d R:%I64d)!"), m_ucReadEP, m_llWriteLength.QuadPart, m_llReadLength.QuadPart);
    }

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CLoopbackMonitor::IsLoopbackReset
//
// Summary
//
// Parameters
//
// Return Value
//      TRUE if successful, FALSE if otherwise
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CLoopbackMonitor::IsLoopbackReset()
{
    if (m_bWriteOngoing)
    {
        return FALSE;
    }

    if (m_llWriteLength.QuadPart == 0 && m_llReadLength.QuadPart == 0)
    {
        return TRUE;
    }

    return FALSE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CLoopbackMonitor::GetPendingData
//
// Summary
//
// Parameters
//
// Return Value
//      TRUE if successful, FALSE if otherwise
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CLoopbackMonitor::GetPendingData(PLARGE_INTEGER a_pllLength)
{
    a_pllLength->QuadPart = m_llWriteLength.QuadPart - m_llReadLength.QuadPart;
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CLoopbackMonitor::HasPendingData
//
// Summary
//      Checks if there is a pending write
//
// Parameters
//      None
//
// Return Value
//      TRUE if has pending data, FALSE if otherwise
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CLoopbackMonitor::HasPendingData()
{
    return m_llWriteLength.QuadPart - m_llReadLength.QuadPart ? TRUE : FALSE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CLoopbackMonitor::SetFileMismatch
//
// Summary
//      Sets the status of file comparison between the write file and read file
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

VOID CLoopbackMonitor::SetFileMismatch()
{
    m_bFileMismatch = TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CLoopbackMonitor::IsFileMismatch
//
// Summary
//      Checks the status of file comparison
//
// Parameters
//      None
//
// Return Value
//      TRUE if mismatch, FALSE if otherwise
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CLoopbackMonitor::IsFileMismatch()
{
    return m_bFileMismatch;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CLoopbackMonitor::Reset
//
// Summary
//      Resets the loopback state machine - that is no write and read ongoing
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

VOID CLoopbackMonitor::Reset()
{
    m_eType = (ETASK_PAYLOAD_TYPE)0;
    m_llWriteLength.QuadPart = 0;
    m_llReadLength.QuadPart = 0;
    m_bWriteOngoing = FALSE;
    m_bSetWaitForWriteCompletion = FALSE;
    m_bFileMismatch = FALSE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CLoopbackMonitor::SetWriteOngoing
//
// Summary
//      Sets status to indicate write thread is ongoing
//
// Parameters
//      a_bOngoing  - Indicates write thread is ongoing of finished
//
// Return Value
//      None
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

VOID CLoopbackMonitor::SetWriteOngoing(BOOL a_bOngoing)
{
    m_bWriteOngoing = a_bOngoing;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CLoopbackMonitor::IsWaitingForWriteCompletion
//
// Summary
//      Checks if write thread is waiting for completion of the loopback
//
// Parameters
//      None
//
// Return Value
//      Pointer to the completion parameter
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

PTTASK_COMPLETION_PARAM CLoopbackMonitor::IsWaitingForWriteCompletion()
{
    if (m_bSetWaitForWriteCompletion)
    {
        return &m_CompletionParam;
    }
    else
    {
        return NULL;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CLoopbackMonitor::SetWaitingForWriteCompletion
//
// Summary
//      Set flag to indicate that write thread is waiting for completion iof the loopback
//
// Parameters
//      a_pCompletionParam  - Completion parameter
//
// Return Value
//      None
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

VOID CLoopbackMonitor::SetWaitingForWriteCompletion(PTTASK_COMPLETION_PARAM a_pCompletionParam)
{
    memcpy(&m_CompletionParam, a_pCompletionParam, sizeof(*a_pCompletionParam));
    m_bSetWaitForWriteCompletion = TRUE;
}


