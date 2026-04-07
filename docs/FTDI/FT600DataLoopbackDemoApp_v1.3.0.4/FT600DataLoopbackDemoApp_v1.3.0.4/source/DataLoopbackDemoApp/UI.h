/*
 * FT600 Data Loopback Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#pragma once
#include "afxwin.h"
#include "Resource.h"
#include "afxcmn.h"



#define MAX_PAYLOAD_SIZE_CH1                104857600 // 100 MB
#define MAX_PAYLOAD_SIZE                    104857600 // 100 MB

#define STATUS_MSG_INVALID_SESSION_LENGTH   _T("Invalid Session Length! (Min: 1, Max: ")
#define STATUS_MSG_INVALID_FLUSH_VALUE      _T("No Outstanding Bytes!")
#define STATUS_MSG_STOP_SUCCEEDED           _T("Stopped successfully!")
#define STATUS_MSG_WRITE_SUCCEEDED          _T("Payload data written!")
#define STATUS_MSG_READ_SUCCEEDED           _T("Payload data read!")
#define STATUS_MSG_WRITE_FAILED             _T("Failed to read from device!")
#define STATUS_MSG_READ_FAILED              _T("Failed to read from device!")
#define STATUS_MSG_ONGOING                  _T("Ongoing...")

#define DEFAULT_VALUE_SESSION_LENGTH_EP0282 _T("4096")
#define DEFAULT_VALUE_SESSION_LENGTH_EP0383 _T("4096")
#define DEFAULT_VALUE_SESSION_LENGTH_EP0484 _T("4096")
#define DEFAULT_VALUE_SESSION_LENGTH_EP0585 _T("4096")

#define COMPLETION_STATUS_STOPPED           _T("Stopped  ")
#define COMPLETION_STATUS_COMPLETED         _T("Completed")
#define COMPLETION_STATUS_FAILED            _T("Failed   ")



typedef enum _EENABLE_STATE
{
    ENABLE_STATE_DISABLED,
    ENABLE_STATE_ENABLED,
    ENABLE_STATE_STOPPED,
    ENABLE_STATE_STARTED,

} EENABLE_STATE;



typedef struct _EP_WritePipe_Components
{
    UCHAR            m_ucEP;
    CButton         *m_pCheck;
    CButton         *m_pButton_Start;
    CButton         *m_pButton_Stop;
    CEdit           *m_pEdit_SL;
    BOOL             m_bEdit_SL;

    BOOL             m_bUsePreviousValue;
    BOOL             m_bDisableUsePreviousValue;
    BOOL             m_bTaskOngoing;
    EENABLE_STATE    m_eState;

} EP_WritePipe_Components, *PEP_WritePipe_Components;

typedef struct _EP_ReadPipe_Components
{
    UCHAR            m_ucEP;
    CButton         *m_pCheck;
    CButton         *m_pButton_Start;
    CButton         *m_pButton_Stop;
    CEdit           *m_pEdit_SL;
    BOOL             m_bEdit_SL;

    BOOL             m_bUsePreviousValue;
    BOOL             m_bDisableUsePreviousValue;
    BOOL             m_bTaskOngoing;
    EENABLE_STATE    m_eState;

} EP_ReadPipe_Components, *PEP_ReadPipe_Components;


