/*
 * FT600 Data Streamer Demo App
 *
 * Copyright (C) 2016 FTDI Chip
 *
 */

#pragma once



#define NUM_EP_PAIRS     1
#define BASE_WRITE_EP    0X02
#define BASE_READ_EP     0X82

#if USE_ASYNCHRONOUS
#define MAX_QUEUE_SIZE_CH1                  128
#define MAX_PAYLOAD_SIZE_CH1_USB3           1024*1024 // 256K
#define MAX_PAYLOAD_SIZE_CH1_USB2           1024*1024 // 256K
#else
#define MAX_QUEUE_SIZE_CH1                  16
#define MAX_PAYLOAD_SIZE_CH1_USB3           16*1024*1024 // 256K
#define MAX_PAYLOAD_SIZE_CH1_USB2           4*1024*1024 // 256K

#endif

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

} ETASK_TYPE;

typedef enum _ETASK_STATUS
{
    ETASK_STATUS_FAILED,
    ETASK_STATUS_COMPLETED,
    ETASK_STATUS_STOPPED,
    ETASK_STATUS_UNPLUGGED,

} ETASK_STATUS;



typedef struct _TTASK_COMPLETION_PARAM
{
    LARGE_INTEGER       m_llTaskNumber;         // task id number
    LARGE_INTEGER       m_llPayloadTransferred; // payload transferred
    DWORD               m_dwTimeMs;
    ETASK_STATUS        m_eStatus;
    UCHAR               m_ucEP;                 // endpoint number

} TTASK_COMPLETION_PARAM, *PTTASK_COMPLETION_PARAM;

typedef struct _TTASK_PAYLOAD_PARAM
{
	ETASK_PAYLOAD_TYPE  m_eType;                // payload type
	ULONG               m_ulFixedValue;         // fixed value for payload if payload is fixed value
	ULONG               m_ulFixedValue2;        // fixed value for payload if payload is fixed value
	BOOL				m_bIntfType;

} TTASK_PAYLOAD_PARAM, *PTTASK_PAYLOAD_PARAM;

typedef struct _TTASK_TEST_PARAM
{
    ULONG               m_ulQueueSize;
    ULONG               m_ulPacketSize;

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

} TTASK_MANAGER_PARAM, *PTTASK_MANAGER_PARAM;

typedef struct _DeviceDetectionRoutineParams
{
    PUCHAR  m_pucWriteEP;
    PUCHAR  m_pucReadEP;
    ULONG   m_ulFirmwareVersion;
	ULONG   m_ulDriverVersion;
	ULONG   m_ulLibraryVersion;
	USHORT  m_uwVID;
    USHORT  m_uwPID;
    UCHAR   m_ucNumWriteEP;
    UCHAR   m_ucNumReadEP; 
    BOOL    m_bIsUsb3;
	FT_DEVICE m_eIntfType;
} DeviceDetectionRoutineParams, *PDeviceDetectionRoutineParams;


