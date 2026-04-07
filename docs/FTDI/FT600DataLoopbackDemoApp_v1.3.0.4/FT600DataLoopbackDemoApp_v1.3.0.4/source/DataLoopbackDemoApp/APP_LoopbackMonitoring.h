/*
 * FT600 Data Loopback Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#pragma once
#include "APP_Task.h"



class CLoopbackMonitor
{

public:

    CLoopbackMonitor(UCHAR a_ucWriteEP, UCHAR a_ucReadEP);
    ~CLoopbackMonitor();

    BOOL AddWrittenPayload(PLARGE_INTEGER a_pllLength, ETASK_PAYLOAD_TYPE a_eType);   // to be called by WriterThread completion
    BOOL AddReadPayload(PLARGE_INTEGER a_pllLength);                                  // to be called by ReaderThread completion

    BOOL IsLoopbackReset();
    BOOL GetPendingData(PLARGE_INTEGER a_pllLength);
    BOOL HasPendingData();
    VOID SetWriteOngoing(BOOL a_bOngoing);
    VOID Reset();

    PTTASK_COMPLETION_PARAM IsWaitingForWriteCompletion();
    VOID SetWaitingForWriteCompletion(PTTASK_COMPLETION_PARAM a_pCompletionParam);

    VOID SetFileMismatch();
    BOOL IsFileMismatch();


private:

    UCHAR                   m_ucWriteEP;
    UCHAR                   m_ucReadEP;
    ETASK_PAYLOAD_TYPE      m_eType;                // payload type
    LARGE_INTEGER           m_llWriteLength;        // payload length already written
    LARGE_INTEGER           m_llReadLength;         // payload length already read
    BOOL                    m_bWriteOngoing;

    BOOL                    m_bSetWaitForWriteCompletion;
    TTASK_COMPLETION_PARAM  m_CompletionParam;

    BOOL                    m_bFileMismatch;
};


