/*
 * FT600 Data Streamer Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#pragma once
#include "afxwin.h"
#include "Resource.h"
#include "afxcmn.h"



#define STATUS_MSG_INVALID_PACKET_SIZE      _T("Invalid Session Size! (Min: 1, Max: ")
#define STATUS_MSG_INVALID_QUEUE_SIZE       _T("Invalid Queue Size! (Min: 2, Max: ")
#define STATUS_MSG_INVALID_FLUSH_VALUE      _T("No Outstanding Bytes!")
#define STATUS_MSG_STOP_SUCCEEDED           _T("Stopped successfully!")
#define STATUS_MSG_WRITE_SUCCEEDED          _T("Payload data written!")
#define STATUS_MSG_READ_SUCCEEDED           _T("Payload data read!")
#define STATUS_MSG_WRITE_FAILED             _T("Failed to read from device!")
#define STATUS_MSG_READ_FAILED              _T("Failed to read from device!")
#define STATUS_MSG_ONGOING                  _T("Ongoing...")

#if USE_ASYNCHRONOUS
#define DEFAULT_VALUE_PACKET_SIZE_USB3      _T("262144") //256K
#define DEFAULT_VALUE_PACKET_SIZE_USB2      _T("262144") //256K
#define DEFAULT_VALUE_QUEUE_SIZE_EP0282     _T("16")
#else
#define DEFAULT_VALUE_PACKET_SIZE_USB3      _T("16777216") //
#define DEFAULT_VALUE_PACKET_SIZE_USB2      _T("4194304") //
#define DEFAULT_VALUE_QUEUE_SIZE_EP0282     _T("16")
#endif
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

typedef struct _EP_Components
{
    UCHAR            m_ucEP;
    CButton         *m_pCheck;
    CButton         *m_pButton_Start;
    CButton         *m_pButton_Stop;
    CEdit           *m_pEdit_SL;
    CEdit           *m_pEdit_QS;
    CRichEditCtrl   *m_pEdit_RATE;

    BOOL             m_bTaskOngoing;
    EENABLE_STATE    m_eState;
	int				m_radioPattern;

} 
EP_WritePipe_Components, *PEP_WritePipe_Components,
EP_ReadPipe_Components, *PEP_ReadPipe_Components,
EP_Pipe_Components, *PEP_Pipe_Components;



