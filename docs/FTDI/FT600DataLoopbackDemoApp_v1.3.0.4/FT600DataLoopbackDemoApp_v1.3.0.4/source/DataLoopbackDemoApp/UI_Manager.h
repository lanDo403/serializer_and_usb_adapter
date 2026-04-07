/*
 * FT600 Data Loopback Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#pragma once
#include "UI.h"
#include "APP_Task.h"



#if ENABLE_COMMANDLINE_INPUT

#define COMMANDLINE_INPUT_PARAM0        0 // file name
#define COMMANDLINE_INPUT_PARAM1        1 // run stress test?
#define COMMANDLINE_INPUT_PARAM2        2 // fifo size
#define COMMANDLINE_INPUT_PARAMCOUNT    3 // number of params

typedef struct _COMMAND_LINE_INPUT
{
    BOOL   m_bRunStressTest;
    USHORT m_uwFIFOSize;

} COMMAND_LINE_INPUT;

extern COMMAND_LINE_INPUT g_oCommandLine;

#endif // ENABLE_COMMANDLINE_INPUT



class CTaskManager;

class CUIManager : public CDialogEx
{

public:

    CUIManager(CWnd* pParent = NULL);
    ~CUIManager();
    enum { IDD = IDD_FUNCTIONTESTER_DIALOG };

    VOID CompletionRoutine(CONST PVOID a_pParam);
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
    afx_msg void OnEditSL_EP03();
    afx_msg void OnEditSL_EP04();
    afx_msg void OnEditSL_EP05();
    afx_msg void OnEditSL_EP82();
    afx_msg void OnEditSL_EP83();
    afx_msg void OnEditSL_EP84();
    afx_msg void OnEditSL_EP85();

    // On Check Box Update
    afx_msg void OnCheck_EP02();
    afx_msg void OnCheck_EP03();
    afx_msg void OnCheck_EP04();
    afx_msg void OnCheck_EP05();
    afx_msg void OnCheck_EP82();
    afx_msg void OnCheck_EP83();
    afx_msg void OnCheck_EP84();
    afx_msg void OnCheck_EP85();

    // On Radio Button Selection Update
    afx_msg void OnRadioOpenBy_DESC();
	afx_msg void OnRadioOpenBy_SERIAL();
	afx_msg void OnRadioOpenBy_INDEX();

    // On Button Click
    afx_msg void OnButtonStart_EP02();
    afx_msg void OnButtonStart_EP03();
    afx_msg void OnButtonStart_EP04();
    afx_msg void OnButtonStart_EP05();
    afx_msg void OnButtonStart_EP82();
    afx_msg void OnButtonStart_EP83();
    afx_msg void OnButtonStart_EP84();
    afx_msg void OnButtonStart_EP85();
    afx_msg void OnButtonStop_EP02();
    afx_msg void OnButtonStop_EP03();
    afx_msg void OnButtonStop_EP04();
    afx_msg void OnButtonStop_EP05();
    afx_msg void OnButtonStop_EP82();
    afx_msg void OnButtonStop_EP83();
    afx_msg void OnButtonStop_EP84();
    afx_msg void OnButtonStop_EP85();
    afx_msg void OnButtonOpen();
    afx_msg void OnButtonClose();



    // On Initialization - set default settings/values
    VOID OnInit_WritePipe               (PEP_WritePipe_Components a_pComponents);
    VOID OnInit_ReadPipe                (PEP_ReadPipe_Components  a_pComponents);
    VOID OnInit();

    // On Edit Box Update - dynamically check values that were set
    BOOL OnEditSL_WritePipe             (PEP_WritePipe_Components a_pComponents, PLARGE_INTEGER a_pllSessionLength = NULL);
    BOOL OnEditSL_ReadPipe              (PEP_ReadPipe_Components  a_pComponents, PLARGE_INTEGER a_pllSessionLength = NULL);

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
    VOID OnButtonStart_AllPipes         ();
    VOID OnButtonStop_AllPipes          ();
    BOOL InternalIsAllPipesUncheckedOrDisabled();
    VOID EnableDisable_StartAllStopAll  ();

    // Completion Routines - for completion of tasks triggered by the buttons
    BOOL CompletionRoutine_WritePipe	(CONST PTTASK_MANAGER_PARAM a_pParam);
    BOOL CompletionRoutine_ReadPipe		(CONST PTTASK_MANAGER_PARAM a_pParam);

    // For debugging / status message
    afx_msg VOID OnButtonClearOutput    ();
    afx_msg VOID OnCheckDebugConsole    ();
    afx_msg VOID OnCheckStressTest      ();

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

#if ENABLE_COMMANDLINE_INPUT
    static DWORD StressTestTrigger(PVOID a_pvParam);
#endif // ENABLE_COMMANDLINE_INPUT

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
    CButton                             m_Button_StartAll;
    CButton                             m_Button_StopAll;
    CEdit                               m_EditVID;
    CEdit                               m_EditPID;

    // For write pipe
    CButton                             m_WritePipeCheck[NUM_EP_PAIRS];
    CButton                             m_WritePipeButton_Start[NUM_EP_PAIRS];
    CButton                             m_WritePipeButton_Stop[NUM_EP_PAIRS];
    CEdit                               m_WritePipeEdit_SL[NUM_EP_PAIRS];
    TTASK_MANAGER_PARAM                 m_WriteTaskParam[NUM_EP_PAIRS];

    // For read pipe
    CButton                             m_ReadPipeCheck[NUM_EP_PAIRS];
    CButton                             m_ReadPipeButton_Start[NUM_EP_PAIRS];
    CButton                             m_ReadPipeButton_Stop[NUM_EP_PAIRS];
    CEdit                               m_ReadPipeEdit_SL[NUM_EP_PAIRS];
    TTASK_MANAGER_PARAM                 m_ReadTaskParam[NUM_EP_PAIRS];

    // For debug output
    CRichEditCtrl                       m_RichEdit_Output;
    CButton                             m_CheckDebugConsole;

    // For stress test
    CButton                             m_CheckStressTest;
    CButton                             m_CheckStressTest_RandLen;
    CEdit                               m_EditStressFIFOSize;

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

};


