/*
 * FT600 Data Loopback Demo App
 *
 * Copyright (C) 2016 FTDI Chip
 *
 */

#pragma once



#define NUM_EP_PAIRS     4
#define BASE_WRITE_EP    0X02
#define BASE_READ_EP     0X82



typedef enum _ETASK_PAYLOAD_TYPE
{
    ETASK_PAYLOAD_TYPE_INCREMENTAL,
    ETASK_PAYLOAD_TYPE_RANDOM,
    ETASK_PAYLOAD_TYPE_FIXEDVALUE

} ETASK_PAYLOAD_TYPE;

typedef enum _ETASK_TYPE
{
    ETASK_TYPE_IO_START,
    ETASK_TYPE_IO_STOP,
    ETASK_TYPE_IO_FLUSH,

} ETASK_TYPE;

typedef enum _ETASK_STATUS
{
    ETASK_STATUS_FAILED,
    ETASK_STATUS_COMPLETED,
    ETASK_STATUS_STOPPED,

} ETASK_STATUS;



typedef struct _TTASK_COMPLETION_PARAM
{
    LARGE_INTEGER       m_llTaskNumber;         // task id number
    LARGE_INTEGER       m_llPayloadTransferred; // payload transferred
    ETASK_STATUS        m_eStatus;
    DWORD               m_dwTimeMs;         

} TTASK_COMPLETION_PARAM, *PTTASK_COMPLETION_PARAM;

typedef struct _TTASK_PAYLOAD_PARAM
{
    ETASK_PAYLOAD_TYPE  m_eType;                // payload type

} TTASK_PAYLOAD_PARAM, *PTTASK_PAYLOAD_PARAM;

typedef struct _TTASK_TEST_PARAM
{
    LARGE_INTEGER       m_llSessionLength;      // session length
//    BOOL                m_bStream;              // streaming mode?

} TTASK_TEST_PARAM, *PTTASK_TEST_PARAM;

typedef struct _TTASK_MANAGER_PARAM
{
    LARGE_INTEGER       m_llTaskNumber;         // task id number

    UCHAR               m_ucEP;                 // endpoint number
    BOOL                m_bIsWrite;             // for direction

    TTASK_PAYLOAD_PARAM m_PayloadParam;         // payload parameters
    TTASK_TEST_PARAM    m_TestParam;            // test parameters

    ETASK_TYPE          m_eAction;              // start, stop, flush
    ETASK_STATUS        m_eStatus;              // completion status
    LARGE_INTEGER       m_llPayloadTransferred; // payload transferred

    LARGE_INTEGER       m_llPendingData;        // pending data
    BOOL                m_bFilesMatch;          // if pending data is 0

    BOOL                m_bStressTest;          // stress testing; repeat task

} TTASK_MANAGER_PARAM, *PTTASK_MANAGER_PARAM;

typedef struct _TTASK_LOOPBACK_PARAM
{
    TTASK_PAYLOAD_PARAM m_PayloadParam;         // payload parameters
    TTASK_TEST_PARAM    m_TestParam;            // test parameters

} TTASK_LOOPBACK_PARAM, *PTTASK_LOOPBACK_PARAM;

typedef struct _DeviceDetectionRoutineParams
{
    PUCHAR m_pucWriteEP;
    PUCHAR m_pucReadEP;
    UCHAR m_ucNumWriteEP;
    UCHAR m_ucNumReadEP; 
    USHORT m_uwVID; 
    USHORT m_uwPID;
    ULONG m_ulFirmwareVersion;
	ULONG m_ulDriverVersion;
	ULONG m_ulLibraryVersion;

} DeviceDetectionRoutineParams, *PDeviceDetectionRoutineParams;


