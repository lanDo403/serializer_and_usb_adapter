/*
 * FT600 Data Streamer Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#pragma once
#include "UI.h"
#include "APP_Task.h"
#include "afxwin.h"



class CTaskManager;

class CUIManager : public CDialogEx
{

public:

    CUIManager(CWnd* pParent = NULL);
    ~CUIManager();
    enum { IDD = IDD_FUNCTIONTESTER_DIALOG };

    VOID CompletionRoutine(UCHAR ucEP);
    VOID DeviceDetectionRoutine(CONST PVOID a_pParam);


private:

    HICON m_hIcon;

    // Default
    virtual void    DoDataExchange(CDataExchange* pDX);
    virtual BOOL    OnInitDialog();
    afx_msg void    OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg BOOL    OnDeviceChange(UINT EventType, DWORD_PTR pdwData);
    afx_msg LRESULT OnStartWork(WPARAM wParam, LPARAM lParam);
    afx_msg BOOL    OnEraseBkgnd(CDC* pDC);
    afx_msg HBRUSH  OnCtlColor(CDC* pDC, CWnd *pWnd, UINT nCtlColor);
    DECLARE_MESSAGE_MAP()


    // On  Edit Box Update
    afx_msg void OnEditSL_EP02();
    afx_msg void OnEditSL_EP82();

    // On Check Box Update
    afx_msg void OnCheck_EP02();
    afx_msg void OnCheck_EP82();

    // On Radio Button Selection Update
    afx_msg void OnRadioOpenBy_DESC();
	afx_msg void OnRadioOpenBy_SERIAL();
	afx_msg void OnRadioOpenBy_INDEX();

    // On Button Click
    afx_msg void OnButtonStart_EP02();
    afx_msg void OnButtonStart_EP82();
    afx_msg void OnButtonStop_EP02();
    afx_msg void OnButtonStop_EP82();
    afx_msg void OnButtonOpen();
    afx_msg void OnButtonClose();



    // On Initialization - set default settings/values
    VOID OnInit_WritePipe               (PEP_WritePipe_Components a_pComponents);
    VOID OnInit_ReadPipe                (PEP_ReadPipe_Components  a_pComponents);
    VOID OnInit();

    // On Edit Box Update - dynamically check values that were set
    BOOL OnEditSL_Pipe                  (PEP_Pipe_Components a_pComponents, PLARGE_INTEGER a_pllSessionLength = NULL);
    BOOL OnEditQS_Pipe                  (PEP_Pipe_Components a_pComponents, PUSHORT a_pQueueSize);

    // On Check Box Update - enable/disable window components
    VOID OnCheck_WritePipe              (PEP_WritePipe_Components a_pComponents);
    VOID OnCheck_ReadPipe               (PEP_ReadPipe_Components  a_pComponents);
    VOID EnableDisable_WritePipe         (PEP_WritePipe_Components a_pComponents, EENABLE_STATE a_eState);
    VOID EnableDisable_ReadPipe          (PEP_ReadPipe_Components a_pComponents, EENABLE_STATE a_eState);

    // On Radio Button Selection Update - disable/enable the fixed value edit box
    VOID OnRadioPayload_WritePipe       (PEP_WritePipe_Components a_pComponents, BOOL a_bDontUpdateData = FALSE);

    // On Button Click - execute task to task manager
    VOID OnButtonStart_WritePipe        (PEP_WritePipe_Components a_pComponents, PTTASK_MANAGER_PARAM a_pTaskParam);
    VOID OnButtonStop_WritePipe         (PEP_WritePipe_Components a_pComponents);
    VOID OnButtonStart_ReadPipe         (PEP_ReadPipe_Components  a_pComponents, PTTASK_MANAGER_PARAM a_pTaskParam);
    VOID OnButtonStop_ReadPipe          (PEP_ReadPipe_Components  a_pComponents);
    BOOL InternalIsAllPipesUncheckedOrDisabled();

    // For debugging / status message
    afx_msg VOID OnButtonClearOutput    ();
    afx_msg VOID OnCheckDebugConsole    ();

    // Device detection
    VOID RegisterDeviceNotification     ();
    VOID UnregisterDeviceNotification   ();
    VOID OnDeviceChange_DeviceArrival   ();
    VOID OnDeviceChange_DeviceRemoval   (BOOL a_bForce = FALSE);

    // For open controls
    VOID OnRadioOpenBy                  (int a_iButton, BOOL a_bForce = FALSE);
    VOID EnableDisable_OpenControls      (BOOL a_bEnable);
    BOOL GetUpdatedOpenByValue          ();

    BOOL InternalOnDeviceChange(DWORD_PTR pdwData);

    VOID LaunchTaskManager();

    static DWORD OnDeviceArrival(PVOID a_pvParam);
    static DWORD OnDeviceRemoval(PVOID a_pvParam);


private:

    // For processing of UI events
    CTaskManager                       *m_pTaskManager;
    CRITICAL_SECTION                    m_csCleanup;
    BOOL                                m_bTaskManagerExiting;

    // For device notification
    HDEVNOTIFY                          m_hDeviceNotify;

    // For write and read pipes
    EP_WritePipe_Components             m_WritePipe[NUM_EP_PAIRS];
    EP_ReadPipe_Components              m_ReadPipe[NUM_EP_PAIRS];
    CEdit                               m_EditVID;
    CEdit                               m_EditPID;

    // For write pipe
    CButton                             m_WritePipeCheck[NUM_EP_PAIRS];
    CButton                             m_WritePipeButton_Start[NUM_EP_PAIRS];
    CButton                             m_WritePipeButton_Stop[NUM_EP_PAIRS];
    CEdit                               m_WritePipeEdit_SL[NUM_EP_PAIRS];
    CEdit                               m_WritePipeEdit_QS[NUM_EP_PAIRS];
    TTASK_MANAGER_PARAM                 m_WriteTaskParam[NUM_EP_PAIRS];
    CRichEditCtrl                       m_WriteRate[NUM_EP_PAIRS];

    // For read pipe
    CButton                             m_ReadPipeCheck[NUM_EP_PAIRS];
    CButton                             m_ReadPipeButton_Start[NUM_EP_PAIRS];
    CButton                             m_ReadPipeButton_Stop[NUM_EP_PAIRS];
    CEdit                               m_ReadPipeEdit_SL[NUM_EP_PAIRS];
    CEdit                               m_ReadPipeEdit_QS[NUM_EP_PAIRS];
    TTASK_MANAGER_PARAM                 m_ReadTaskParam[NUM_EP_PAIRS];
    CRichEditCtrl                       m_ReadRate[NUM_EP_PAIRS];

    // For debug output
    CRichEditCtrl                       m_RichEdit_Output;
    CButton                             m_CheckDebugConsole;

    // For Open by
    #define RADIO_OPENBY_COUNT 3
    int                                 m_iRadioOpenBy;
    CButton                             m_ButtonOpen;
    CButton                             m_ButtonClose;
    CButton                             m_RadioOpenBy[RADIO_OPENBY_COUNT];
    CEdit                               m_EditOpenBy[RADIO_OPENBY_COUNT];
    CHAR                                m_szDeviceDescription[32];
	CHAR                                m_szSerialNumber[16];
	CHAR                                m_szIndex[16];
	GUID                                m_DeviceGUID[2];
    BOOL                                m_bQuitOngoing;

    BOOL                                m_bIsUsb3;
	int									m_radioPattern;
	bool								m_bDevType;

	
public:
	int m_FixedPattern;
	afx_msg void OnBnClickedRadioPatternFixed();
	CEdit m_EditFixValue;
	afx_msg void OnBnClickedRadioPatternRand();
	afx_msg void OnBnClickedRadioPatternInc();

};


