/*
 * FT600 Data Streamer Demo App
 *
 * Copyright (C) 2016 FTDI Chip
 *
 */

#include "stdafx.h"
#include "afxdialogex.h"
#include "UI_DataStreamer.h"
#include "UI_Manager.h"
#include "APP_TaskManager.h"
#include "APP_Task.h"
#include "APP_Utils.h"
#include "APP_Device.h"



#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define USE_LOGGER 1
#if 1
#define UI_TRACE(...) CMD_LOG(__VA_ARGS__)
#else
#define UI_TRACE(...) 
#endif

static CWnd *g_pMainWindow = NULL;



////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::CUIManager
//      This class contains all the UI-related code which processes user input 
//      and converts it to a task for the Task Manager thread. 
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

CUIManager::CUIManager(CWnd* pParent /*=NULL*/): 
    CDialogEx(CUIManager::IDD, pParent)
    , m_pTaskManager(NULL)
    , m_hDeviceNotify(NULL)
    , m_iRadioOpenBy(0xFF)
    , m_bTaskManagerExiting(FALSE)
    , m_bQuitOngoing(FALSE)
    , m_bIsUsb3(TRUE)
	, m_radioPattern(1)
	, m_FixedPattern(0)
	, m_bDevType(false)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

    try
    {
        ::InitializeCriticalSection(&m_csCleanup);
    }
    catch (int) 
    {
    }

    memset(m_szDeviceDescription, 0, sizeof(m_szDeviceDescription));
    strcpy_s(m_szDeviceDescription, DEFAULT_VALUE_OPENBY_DESC);

	memset(m_szSerialNumber, 0, sizeof(m_szSerialNumber));
	strcpy_s(m_szSerialNumber, DEFAULT_VALUE_OPENBY_SERIAL);

	memset(m_szIndex, 0, sizeof(m_szIndex));
	strcpy_s(m_szIndex, DEFAULT_VALUE_OPENBY_INDEX);

    memcpy(&m_DeviceGUID[0], &GUID_DEVINTERFACE_FOR_D3XX, sizeof(GUID));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::~CUIManager
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

CUIManager::~CUIManager()
{
    UI_TRACE(_T("TERMINATION: %s"), _T(__FUNCTION__));

    m_bQuitOngoing = TRUE;

    ::DeleteCriticalSection(&m_csCleanup);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::DoDataExchange
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

void CUIManager::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);


	// Open/Close related UI controls
	DDX_Control(pDX, IDC_BUTTON_OPEN, m_ButtonOpen);
	DDX_Control(pDX, IDC_BUTTON_CLOSE, m_ButtonClose);
	DDX_Control(pDX, IDC_EDIT_OPENBY_DESC2, m_EditOpenBy[0]);
	DDX_Control(pDX, IDC_EDIT_OPENBY_GUID2, m_EditOpenBy[1]);
	DDX_Control(pDX, IDC_EDIT_OPENBY_INDEX, m_EditOpenBy[2]);
	DDX_Control(pDX, IDC_RADIO_OPENBY_DESC2, m_RadioOpenBy[0]);
	DDX_Control(pDX, IDC_RADIO_OPENBY_GUID2, m_RadioOpenBy[1]);
	DDX_Control(pDX, IDC_RADIO_OPENBY_INDEX, m_RadioOpenBy[2]);
	DDX_Control(pDX, IDC_EDIT_VID, m_EditVID);
	DDX_Control(pDX, IDC_EDIT_PID, m_EditPID);


	// Write EP related UI controls
	DDX_Control(pDX, IDC_CHECK_EP02, m_WritePipeCheck[0]);
	DDX_Control(pDX, IDC_BUTTON_START_EP02, m_WritePipeButton_Start[0]);
	DDX_Control(pDX, IDC_BUTTON_STOP_EP02, m_WritePipeButton_Stop[0]);
	DDX_Control(pDX, IDC_EDIT_SL_EP02, m_WritePipeEdit_SL[0]);
	DDX_Control(pDX, IDC_EDIT_QS_EP02, m_WritePipeEdit_QS[0]);
	DDX_Control(pDX, IDC_EDIT_RATE_EP02, m_WriteRate[0]);

	// Read EP related UI controls
	DDX_Control(pDX, IDC_CHECK_EP82, m_ReadPipeCheck[0]);
	DDX_Control(pDX, IDC_BUTTON_STOP_EP82, m_ReadPipeButton_Stop[0]);
	DDX_Control(pDX, IDC_BUTTON_START_EP82, m_ReadPipeButton_Start[0]);
	DDX_Control(pDX, IDC_EDIT_SL_EP82, m_ReadPipeEdit_SL[0]);
	DDX_Control(pDX, IDC_EDIT_QS_EP82, m_ReadPipeEdit_QS[0]);
	DDX_Control(pDX, IDC_EDIT_RATE_EP82, m_ReadRate[0]);

	// Debug related UI controls
	DDX_Control(pDX, IDC_CHECK_DEBUGCONSOLE, m_CheckDebugConsole);
	DDX_Control(pDX, IDC_RICHEDIT_OUTPUT, m_RichEdit_Output);
	DDX_Radio(pDX, IDC_RADIO_PATTERN_INC, m_radioPattern);
	DDX_Text(pDX, IDC_EDIT_FIX_VALUE, m_FixedPattern);
	DDX_Control(pDX, IDC_EDIT_FIX_VALUE, m_EditFixValue);
}


BEGIN_MESSAGE_MAP(CUIManager, CDialogEx)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_MESSAGE(WM_STARTWORK, OnStartWork)


    // Open/Close related UI controls
    ON_BN_CLICKED(IDC_BUTTON_OPEN,          &CUIManager::OnButtonOpen)
    ON_BN_CLICKED(IDC_BUTTON_CLOSE,         &CUIManager::OnButtonClose)
    ON_BN_CLICKED(IDC_RADIO_OPENBY_DESC2,   &CUIManager::OnRadioOpenBy_DESC)
    ON_BN_CLICKED(IDC_RADIO_OPENBY_GUID2,   &CUIManager::OnRadioOpenBy_SERIAL)
	ON_BN_CLICKED(IDC_RADIO_OPENBY_INDEX,	&CUIManager::OnRadioOpenBy_INDEX)

    // Write EP related UI controls
    ON_BN_CLICKED(IDC_CHECK_EP02,           &CUIManager::OnCheck_EP02)
    ON_BN_CLICKED(IDC_BUTTON_START_EP02,    &CUIManager::OnButtonStart_EP02)
    ON_BN_CLICKED(IDC_BUTTON_STOP_EP02,     &CUIManager::OnButtonStop_EP02)
    ON_EN_UPDATE (IDC_EDIT_SL_EP02,         &CUIManager::OnEditSL_EP02)

    // Read EP related UI controls
    ON_BN_CLICKED(IDC_CHECK_EP82,           &CUIManager::OnCheck_EP82)
    ON_BN_CLICKED(IDC_BUTTON_START_EP82,    &CUIManager::OnButtonStart_EP82)
    ON_BN_CLICKED(IDC_BUTTON_STOP_EP82,     &CUIManager::OnButtonStop_EP82)
    ON_EN_UPDATE (IDC_EDIT_SL_EP82,         &CUIManager::OnEditSL_EP82)

    // Debug related UI controls
    ON_BN_CLICKED(IDC_CHECK_DEBUGCONSOLE,   &CUIManager::OnCheckDebugConsole)
    ON_BN_CLICKED(IDC_BUTTON_CLEAROUTPUT,   &CUIManager::OnButtonClearOutput)

    ON_WM_DEVICECHANGE()
	ON_BN_CLICKED(IDC_RADIO_PATTERN_FIXED, &CUIManager::OnBnClickedRadioPatternFixed)
	ON_BN_CLICKED(IDC_RADIO_PATTERN_RAND, &CUIManager::OnBnClickedRadioPatternRand)
	ON_BN_CLICKED(IDC_RADIO_PATTERN_INC, &CUIManager::OnBnClickedRadioPatternInc)
END_MESSAGE_MAP()



////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Open/Close related functions
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

void CUIManager::OnRadioOpenBy_DESC()       {	OnRadioOpenBy(0);									}
void CUIManager::OnRadioOpenBy_SERIAL()     {	OnRadioOpenBy(1);									}
void CUIManager::OnRadioOpenBy_INDEX()		{	OnRadioOpenBy(2);									}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Write EP related functions
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

void CUIManager::OnCheck_EP02()             {    OnCheck_WritePipe(&m_WritePipe[0]);                            }
void CUIManager::OnButtonStart_EP02()       {    OnButtonStart_WritePipe(&m_WritePipe[0], &m_WriteTaskParam[0]);}
void CUIManager::OnButtonStop_EP02()        {    OnButtonStop_WritePipe(&m_WritePipe[0]);                       }
void CUIManager::OnEditSL_EP02()            {    OnEditSL_Pipe(&m_WritePipe[0]);                                }

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Read EP related functions
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

void CUIManager::OnCheck_EP82()             {    OnCheck_ReadPipe(&m_ReadPipe[0]);                              }
void CUIManager::OnButtonStart_EP82()       {    OnButtonStart_ReadPipe(&m_ReadPipe[0], &m_ReadTaskParam[0]);   }
void CUIManager::OnButtonStop_EP82()        {    OnButtonStop_ReadPipe(&m_ReadPipe[0]);                         }
void CUIManager::OnEditSL_EP82()            {    OnEditSL_Pipe(&m_ReadPipe[0]);                                 }

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnInitDialog
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

BOOL CUIManager::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, FALSE);        // Set small icon


    //
    // Initialize log
    //
#ifdef USE_LOGGER
    g_pLogger = new CLogger(APPLICATION_LOG_FILE_NAME, APPLICATION_TITLE);
#endif // USE_LOGGER

    SYSTEMTIME st = {0};
    GetLocalTime(&st);
    CMD_LOG_NOTIME(_T(" ------------------------------------------------------------"));
    CMD_LOG_NOTIME(_T("| %s"), APPLICATION_TITLE);
    CMD_LOG_NOTIME(_T("| Built   %s %s"), _T(__DATE__), _T(__TIME__));
    CMD_LOG_NOTIME(_T("| Started %02d  %02d %04d %02d:%02d:%02d") , st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute, st.wSecond);
    CMD_LOG_NOTIME(_T(" ------------------------------------------------------------\n"));
                    

    CFont oFont;
    oFont.CreatePointFont(100, _T("Arial"));

    //
    // Initialize GUI
    //
    for (int i=0; i<NUM_EP_PAIRS; i++)
    {
        // Set write pipe
        m_WritePipe[i].m_ucEP               = 0x02+i;
        m_WritePipe[i].m_pCheck             = &m_WritePipeCheck[i];
        m_WritePipe[i].m_pButton_Start      = &m_WritePipeButton_Start[i];
        m_WritePipe[i].m_pButton_Stop       = &m_WritePipeButton_Stop[i];
        m_WritePipe[i].m_pEdit_SL           = &m_WritePipeEdit_SL[i];
        m_WritePipe[i].m_pEdit_QS           = &m_WritePipeEdit_QS[i];
        m_WritePipe[i].m_pEdit_RATE         = &m_WriteRate[i];
        m_WritePipe[i].m_bTaskOngoing       = FALSE;
		m_WritePipe[i].m_radioPattern		= m_radioPattern;
        m_WriteRate[i].SetFont(&oFont, TRUE);

        OnInit_WritePipe(&m_WritePipe[i]);

        if (m_WritePipe[i].m_pCheck->GetCheck() == BST_UNCHECKED)
        {
            EnableDisable_WritePipe(&m_WritePipe[i], ENABLE_STATE_ENABLED);
        }

        // Set read pipe
        m_ReadPipe[i].m_ucEP                = 0x82+i;
        m_ReadPipe[i].m_pCheck              = &m_ReadPipeCheck[i];
        m_ReadPipe[i].m_pButton_Start       = &m_ReadPipeButton_Start[i];
        m_ReadPipe[i].m_pButton_Stop        = &m_ReadPipeButton_Stop[i];
        m_ReadPipe[i].m_pEdit_SL            = &m_ReadPipeEdit_SL[i];
        m_ReadPipe[i].m_pEdit_QS            = &m_ReadPipeEdit_QS[i];
        m_ReadPipe[i].m_pEdit_RATE          = &m_ReadRate[i];
        m_ReadPipe[i].m_bTaskOngoing        = FALSE;
        m_ReadRate[i].SetFont(&oFont, TRUE);

        OnInit_ReadPipe(&m_ReadPipe[i]);

        if (m_ReadPipe[i].m_pCheck->GetCheck() == BST_UNCHECKED)
        {
            EnableDisable_ReadPipe(&m_ReadPipe[i], ENABLE_STATE_ENABLED);
        }
    }

    g_pMainWindow = AfxGetMainWnd();
    g_pMainWindow->PostMessage(WM_STARTWORK);

    OnInit();

    return TRUE;  // return TRUE  unless you set the focus to a control
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnInit_WritePipe
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

VOID CUIManager::OnInit_WritePipe(PEP_WritePipe_Components a_pComponents)
{
    a_pComponents->m_pEdit_SL->SetWindowText(DEFAULT_VALUE_PACKET_SIZE_USB3);
    a_pComponents->m_pEdit_QS->SetWindowText(DEFAULT_VALUE_QUEUE_SIZE_EP0282);
    a_pComponents->m_pEdit_RATE->SetWindowText(_T("000"));
    OnRadioPayload_WritePipe(a_pComponents);
    a_pComponents->m_pCheck->SetCheck(BST_CHECKED);
    a_pComponents->m_eState = ENABLE_STATE_STOPPED;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnInit_ReadPipe
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

VOID CUIManager::OnInit_ReadPipe(PEP_ReadPipe_Components a_pComponents)
{
    a_pComponents->m_pEdit_SL->SetWindowText(DEFAULT_VALUE_PACKET_SIZE_USB3);
    a_pComponents->m_pEdit_QS->SetWindowText(DEFAULT_VALUE_QUEUE_SIZE_EP0282);
    a_pComponents->m_pEdit_RATE->SetWindowText(_T("000"));
    a_pComponents->m_pCheck->SetCheck(BST_CHECKED);
    a_pComponents->m_eState = ENABLE_STATE_STOPPED;
    if (a_pComponents->m_pCheck->GetCheck() == BST_UNCHECKED)
    {
        EnableDisable_ReadPipe(a_pComponents, ENABLE_STATE_ENABLED);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnInit
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

VOID CUIManager::OnInit()
{
    INT iOpen = 1;


    m_iRadioOpenBy = iOpen;
    for (int i=0; i<RADIO_OPENBY_COUNT; i++)
    {
        if (i==iOpen)
        {
            m_RadioOpenBy[i].SetCheck(BST_CHECKED);
        }
        else
        {
            m_RadioOpenBy[i].SetCheck(BST_UNCHECKED);
        }
    }

    CString string (m_szDeviceDescription);
    m_EditOpenBy[0].SetWindowText(string.GetBuffer());

	CString string2(m_szSerialNumber);
	m_EditOpenBy[1].SetWindowText(string2.GetBuffer());

	CString string3(m_szIndex);
	m_EditOpenBy[2].SetWindowText(string3.GetBuffer());

    m_CheckDebugConsole.SetCheck(BST_UNCHECKED);
}





////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnPaint
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

void CUIManager::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this); // device context for painting

        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        // Center icon in client rectangle
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        // Draw the icon
        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CDialogEx::OnPaint();
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnEraseBkgnd
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

COLORREF refColor = RGB(223, 223, 223);//214, 223, 247);
//COLORREF refColor = RGB(0, 175, 240);

BOOL CUIManager::OnEraseBkgnd(CDC* pDC)
{
    CRect rect;
    GetClientRect(&rect);

    CBrush myBrush(refColor);    // dialog background color
    CBrush *pOld = pDC->SelectObject(&myBrush);
    BOOL bRes  = pDC->PatBlt(0, 0, rect.Width(), rect.Height(), PATCOPY);

    pDC->SelectObject(pOld);    // restore old brush
    return bRes;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnCtlColor
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

HBRUSH CUIManager::OnCtlColor(CDC* pDC, CWnd *pWnd, UINT nCtlColor)
{
    switch (nCtlColor)
    {
    case CTLCOLOR_STATIC:
        pDC->SetBkColor(refColor);
        //pDC->SetTextColor(RGB(255, 255, 255));
        return (HBRUSH)GetStockObject(NULL_BRUSH);
    case CTLCOLOR_BTN:
        pDC->SetDCBrushColor(RGB(12, 125, 175));
        return (HBRUSH)GetStockObject(NULL_BRUSH);
    default:
        return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnQueryDragIcon
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

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CUIManager::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}





////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnStartWork
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

LRESULT CUIManager::OnStartWork(WPARAM a_wParam, LPARAM a_lParam)
{
    UI_TRACE(_T("INITIALIZATION: %s"), _T(__FUNCTION__));

    LaunchTaskManager();
    return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::LaunchTaskManager
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

VOID CUIManager::LaunchTaskManager()
{
    // Sanity check
    while (m_pTaskManager)
    {
        CMD_LOG(_T("LaunchTaskManager m_pTaskManager is not NULL!!!"));
        Sleep(100);
    }


    ::EnterCriticalSection(&m_csCleanup);

    if (!GetUpdatedOpenByValue())
    {
        EnableDisable_OpenControls(TRUE);
        return;
    }

    // Create a task manager object
    if (m_iRadioOpenBy == 0)
    {
        m_pTaskManager = new CTaskManager(this, EOPEN_BY_DESC, (PVOID)m_szDeviceDescription);
    }
    else if (m_iRadioOpenBy == 1)
    {
		m_pTaskManager = new CTaskManager(this, EOPEN_BY_SERIAL, (PVOID)m_szSerialNumber);
    }
	else if (m_iRadioOpenBy == 2)
	{
		m_pTaskManager = new CTaskManager(this, EOPEN_BY_INDEX, (PVOID)m_szIndex);
	}
	else
    {
        ::LeaveCriticalSection(&m_csCleanup);
        return;
    }

    // Create a task manager thread
    m_bTaskManagerExiting = FALSE;
    m_pTaskManager->CreateThread();

    ::LeaveCriticalSection(&m_csCleanup);
}





////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnButtonOpen
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

VOID CUIManager::OnButtonOpen()
{
    if (m_hDeviceNotify)
    {
        UnregisterDeviceNotification();
        m_hDeviceNotify = NULL;
    }

    LaunchTaskManager();

    m_ButtonOpen.EnableWindow(FALSE);
    m_RadioOpenBy[0].EnableWindow(FALSE);
    m_RadioOpenBy[1].EnableWindow(FALSE);
    m_EditOpenBy[0].EnableWindow(FALSE);
    m_EditOpenBy[1].EnableWindow(FALSE);
    //EnableDisable_OpenControls(FALSE);

    m_bQuitOngoing = FALSE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnButtonClose
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

VOID CUIManager::OnButtonClose()
{
#if DISALLOW_CLOSING
    // Disallow user to press close button while data transfer is ongoing
    if (m_WritePipe[0].m_eState == ENABLE_STATE_STARTED)
    {
        APP_LOG(_T("Write transfer is ongoing! Please stop the transfer before closing!"));
        return;
    }
    if (m_ReadPipe[0].m_eState == ENABLE_STATE_STARTED)
    {
        APP_LOG(_T("Read transfer is ongoing! Please stop the transfer before closing!"));
        return;
    }
#endif // DISALLOW_CLOSING


    m_bQuitOngoing = TRUE;

    if (m_hDeviceNotify)
    {
        UnregisterDeviceNotification();
        m_hDeviceNotify = NULL;
    }

    HANDLE hdl = (HANDLE)::_beginthreadex(NULL, 0, (PTHREAD_START)OnDeviceRemoval, this, 0, NULL);
    CloseHandle(hdl);
    //OnDeviceChange_DeviceRemoval(TRUE);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnRadioOpenBy
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

VOID CUIManager::OnRadioOpenBy(int a_iButton, BOOL a_bForce)
{
    BOOL bChanged = (m_iRadioOpenBy != a_iButton);

    if (!a_bForce)
    {
        if (m_iRadioOpenBy == a_iButton)
        {
            return;
        }
    }

    for (int i=0; i<RADIO_OPENBY_COUNT; i++)
    {
        if (i==a_iButton)
        {
            // enable
            if (!m_EditOpenBy[i].IsWindowEnabled())
            {
                m_EditOpenBy[i].EnableWindow(TRUE);
            }

			if (!m_RadioOpenBy[i].GetCheck())
			{
				m_RadioOpenBy[i].SetCheck(BST_CHECKED);
			}

            if (m_iRadioOpenBy != 0xFF)
            {
				switch (i)
				{
					case 0:
					{
						UI_TRACE(_T("Open by: Description"));
						break;
					}
					case 1:
					{
						UI_TRACE(_T("Open by: Serial Number"));
						break;
					}
					case 2:
					{
						UI_TRACE(_T("Open by: Index"));
						break;
					}
				}
			}
        }
        else
        {
            // disable
            if (m_EditOpenBy[i].IsWindowEnabled())
            {
                m_EditOpenBy[i].EnableWindow(FALSE);
            }
			if (m_RadioOpenBy[i].GetCheck())
			{
				m_RadioOpenBy[i].SetCheck(BST_UNCHECKED);
			}
        }
    }

    m_iRadioOpenBy = a_iButton;

    if (bChanged)
    {
        UnregisterDeviceNotification();
        RegisterDeviceNotification();

        APP_LOG(_T("Plug-in device! Application will detect it automatically!\n"));
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::GetUpdatedOpenByValue
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

BOOL CUIManager::GetUpdatedOpenByValue()
{
    if (m_iRadioOpenBy == 0)
    {
        CString sStr;

        m_EditOpenBy[0].GetWindowText(sStr);

        if (sStr.GetLength() >= 32)
        {
            UI_TRACE(_T("Invalid Description! [%s] Description is 31 bytes maximum."), sStr.GetBuffer());
			return FALSE;
        }

        CStringA asStr(sStr);
        CStringA csDeviceDescription(m_szDeviceDescription);
        if (asStr.Compare(csDeviceDescription.GetBuffer()) != 0)
        {
            ZeroMemory(m_szDeviceDescription, sizeof(m_szDeviceDescription));
            strcpy_s(m_szDeviceDescription, asStr.GetBuffer());
        }
    }
    else if (m_iRadioOpenBy == 1)
    {
		CString sStr;

		m_EditOpenBy[1].GetWindowText(sStr);

		if (sStr.GetLength() >= 16)
		{
			UI_TRACE(_T("Invalid Serial Number! [%s] Serial Number is 15 bytes maximum."), sStr.GetBuffer());
			return FALSE;
		}

		CStringA asStr(sStr);
		CStringA csSerialNumber(m_szSerialNumber);
		if (asStr.Compare(csSerialNumber.GetBuffer()) != 0)
		{
			ZeroMemory(m_szSerialNumber, sizeof(m_szSerialNumber));
			strcpy_s(m_szSerialNumber, asStr.GetBuffer());
		}
    }
	else if (m_iRadioOpenBy == 2) // INDEX
	{
		CString sStr;

		m_EditOpenBy[2].GetWindowText(sStr);

		CStringA asStr(sStr);
		CStringA csIndex(m_szIndex);
		if (asStr.Compare(csIndex.GetBuffer()) != 0)
		{
			ZeroMemory(m_szIndex, sizeof(m_szIndex));
			strcpy_s(m_szIndex, asStr.GetBuffer());
		}
	}
    else
    {
        UI_TRACE(_T("Invalid m_iRadioOpenBy"));
        return FALSE;
    }

    return TRUE;
}





////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::EnableDisable_OpenControls
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

VOID CUIManager::EnableDisable_OpenControls(BOOL a_bEnable)
{
    if (a_bEnable)
    {
		for (int i = 0; i<RADIO_OPENBY_COUNT; i++)
			m_RadioOpenBy[i].EnableWindow(TRUE);
		OnRadioOpenBy(m_iRadioOpenBy, TRUE);
        m_ButtonOpen.EnableWindow(TRUE);
        m_ButtonClose.EnableWindow(FALSE);
        //m_CheckStressTest.EnableWindow(FALSE);
        //m_CheckStressTest_RandLen.EnableWindow(FALSE);
        //m_EditStressFIFOSize.EnableWindow(FALSE);
        //m_Button_StartAll.EnableWindow(FALSE);
        //m_Button_StopAll.EnableWindow(FALSE);
    }
    else
    {
        m_ButtonOpen.EnableWindow(FALSE);
        m_ButtonClose.EnableWindow(TRUE);
		for (int i = 0; i < RADIO_OPENBY_COUNT; i++)
		{
			m_RadioOpenBy[i].EnableWindow(FALSE);
			m_EditOpenBy[i].EnableWindow(FALSE);
		}
		//m_CheckStressTest.EnableWindow(TRUE);
        //m_CheckStressTest.SetCheck(BST_UNCHECKED);
        //m_CheckStressTest_RandLen.EnableWindow(TRUE);
        //m_CheckStressTest_RandLen.SetCheck(BST_UNCHECKED);
        //m_EditStressFIFOSize.EnableWindow(TRUE);
        //m_Button_StartAll.EnableWindow(TRUE);
        //m_Button_StopAll.EnableWindow(TRUE);
    }

	GetDlgItem(IDC_RADIO_FT600)->EnableWindow(a_bEnable);
	GetDlgItem(IDC_RADIO_FT601)->EnableWindow(a_bEnable);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::EnableDisable_WritePipe
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

VOID CUIManager::EnableDisable_WritePipe(PEP_WritePipe_Components a_pComponents, EENABLE_STATE a_eState)
{
    switch(a_eState)
    {
        case ENABLE_STATE_DISABLED:
        {
            // All component are disabled
            a_pComponents->m_pCheck->EnableWindow(FALSE);
            a_pComponents->m_pButton_Start->EnableWindow(FALSE);
            a_pComponents->m_pButton_Stop->EnableWindow(FALSE);
            a_pComponents->m_pEdit_SL->EnableWindow(FALSE);
            a_pComponents->m_pEdit_QS->EnableWindow(FALSE);
            a_pComponents->m_pEdit_RATE->EnableWindow(FALSE);
            a_pComponents->m_eState = ENABLE_STATE_DISABLED;
            break;
        }

        case ENABLE_STATE_ENABLED:
        {
            // All component are disabled except for the EP check box
            a_pComponents->m_pCheck->EnableWindow(TRUE);
            a_pComponents->m_pEdit_SL->EnableWindow(FALSE);
            a_pComponents->m_pEdit_QS->EnableWindow(FALSE);
            a_pComponents->m_pEdit_RATE->EnableWindow(FALSE);
            a_pComponents->m_pButton_Start->EnableWindow(FALSE);
            a_pComponents->m_pButton_Stop->EnableWindow(FALSE);
            a_pComponents->m_eState = ENABLE_STATE_ENABLED;
            break;
        }

        case ENABLE_STATE_STOPPED:
        {
            // All component are enabled except for the stop button
            a_pComponents->m_pCheck->EnableWindow(TRUE);
            a_pComponents->m_pEdit_SL->EnableWindow(TRUE);
            a_pComponents->m_pEdit_QS->EnableWindow(TRUE);
            a_pComponents->m_pEdit_RATE->EnableWindow(TRUE);
            a_pComponents->m_pButton_Start->EnableWindow(TRUE);
            a_pComponents->m_pButton_Stop->EnableWindow(FALSE);
            OnRadioPayload_WritePipe(a_pComponents, TRUE);
            a_pComponents->m_eState = ENABLE_STATE_STOPPED;
            break;
        }
 
        case ENABLE_STATE_STARTED:
        {
            // All component are disabled except for the stop button
            a_pComponents->m_pCheck->EnableWindow(FALSE);
            a_pComponents->m_pEdit_SL->EnableWindow(FALSE);
            a_pComponents->m_pEdit_QS->EnableWindow(FALSE);
            a_pComponents->m_pEdit_RATE->EnableWindow(FALSE);
            a_pComponents->m_pButton_Start->EnableWindow(FALSE);
            a_pComponents->m_pButton_Stop->EnableWindow(TRUE);
            a_pComponents->m_eState = ENABLE_STATE_STARTED;
            break;
        }

        default:
        {
            break;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::EnableDisable_ReadPipe
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

VOID CUIManager::EnableDisable_ReadPipe(PEP_ReadPipe_Components a_pComponents, EENABLE_STATE a_eState)
{
    switch(a_eState)
    {
        case ENABLE_STATE_DISABLED:
        {
            // All component are disabled
            a_pComponents->m_pCheck->EnableWindow(FALSE);
            a_pComponents->m_pEdit_SL->EnableWindow(FALSE);
            a_pComponents->m_pEdit_QS->EnableWindow(FALSE);
            a_pComponents->m_pEdit_RATE->EnableWindow(FALSE);
            a_pComponents->m_pButton_Start->EnableWindow(FALSE);
            a_pComponents->m_pButton_Stop->EnableWindow(FALSE);
            a_pComponents->m_eState = ENABLE_STATE_DISABLED;
            break;
        }

        case ENABLE_STATE_ENABLED:
        {
            // All component are disabled except for the EP check box
            a_pComponents->m_pCheck->EnableWindow(TRUE);
            a_pComponents->m_pEdit_SL->EnableWindow(FALSE);
            a_pComponents->m_pEdit_QS->EnableWindow(FALSE);
            a_pComponents->m_pEdit_RATE->EnableWindow(FALSE);
            a_pComponents->m_pButton_Start->EnableWindow(FALSE);
            a_pComponents->m_pButton_Stop->EnableWindow(FALSE);
            a_pComponents->m_eState = ENABLE_STATE_ENABLED;
            break;
        }

        case ENABLE_STATE_STOPPED:
        {
            // All component are enabled except for the stop button
            a_pComponents->m_pCheck->EnableWindow(TRUE);
            a_pComponents->m_pEdit_SL->EnableWindow(TRUE);
            a_pComponents->m_pEdit_QS->EnableWindow(TRUE);
            a_pComponents->m_pEdit_RATE->EnableWindow(TRUE);
            a_pComponents->m_pButton_Start->EnableWindow(TRUE);
            a_pComponents->m_pButton_Stop->EnableWindow(FALSE);
            a_pComponents->m_eState = ENABLE_STATE_STOPPED;
            break;
        }
 
        case ENABLE_STATE_STARTED:
        {
            // All component are disabled except for the stop button
            a_pComponents->m_pCheck->EnableWindow(FALSE);
            a_pComponents->m_pEdit_SL->EnableWindow(FALSE);
            a_pComponents->m_pEdit_QS->EnableWindow(FALSE);
            a_pComponents->m_pEdit_RATE->EnableWindow(FALSE);
            a_pComponents->m_pButton_Start->EnableWindow(FALSE);
            a_pComponents->m_pButton_Stop->EnableWindow(TRUE);
            a_pComponents->m_eState = ENABLE_STATE_STARTED;
            break;
        }

        default:
        {
            break;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::InternalIsAllPipesUncheckedOrDisabled
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

BOOL CUIManager::InternalIsAllPipesUncheckedOrDisabled()
{
    for (int i=0; i<NUM_EP_PAIRS; i++)
    {
        if (m_WritePipe[i].m_eState != ENABLE_STATE_ENABLED && 
            m_WritePipe[i].m_eState != ENABLE_STATE_DISABLED)
        {
            return FALSE;
        }

        if (m_ReadPipe[i].m_eState  != ENABLE_STATE_ENABLED && 
            m_ReadPipe[i].m_eState  != ENABLE_STATE_DISABLED)
        {
            return FALSE;
        }
    }

    return TRUE;
}





////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnEditSL_Pipe
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

BOOL CUIManager::OnEditSL_Pipe(PEP_Pipe_Components a_pComponents, PLARGE_INTEGER a_pllSessionLength)
{
    CString cstrSessionLength;
    LARGE_INTEGER llSessionLength;
    DWORD dwMaxPayloadSize = m_bIsUsb3 ? MAX_PAYLOAD_SIZE_CH1_USB3 : MAX_PAYLOAD_SIZE_CH1_USB2;


    a_pComponents->m_pEdit_SL->GetWindowText(cstrSessionLength);

    if (cstrSessionLength.GetLength() == 0)
    {
        if (!a_pllSessionLength)
        {
            UI_TRACE(_T("[0x%02x] Session Length: %s (%s %d))"), a_pComponents->m_ucEP, cstrSessionLength, STATUS_MSG_INVALID_PACKET_SIZE, dwMaxPayloadSize);
        }
        return FALSE;
    }

    llSessionLength.QuadPart = _tstoi64(LPCTSTR(cstrSessionLength));
    if (llSessionLength.QuadPart == 0 || llSessionLength.QuadPart > dwMaxPayloadSize) 
    {
        if (!a_pllSessionLength)
        {
            UI_TRACE(_T("[0x%02x] Session Length: %s (%s %d))"), a_pComponents->m_ucEP, cstrSessionLength, STATUS_MSG_INVALID_PACKET_SIZE, dwMaxPayloadSize);
        }
        return FALSE;
    }

    if (!a_pllSessionLength)
    {
        UI_TRACE(_T("[0x%02x] Session Length: %s"), a_pComponents->m_ucEP, cstrSessionLength);
    }

    if (a_pllSessionLength)
    {
        a_pllSessionLength->QuadPart = llSessionLength.QuadPart;
    }

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnEditQS_Pipe
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

BOOL CUIManager::OnEditQS_Pipe(PEP_Pipe_Components a_pComponents, PUSHORT a_pQueueSize)
{
    CString cstrQueueSize;
    LARGE_INTEGER llQueueSize;
    DWORD dwMaxQueueSize = MAX_QUEUE_SIZE_CH1;


    a_pComponents->m_pEdit_QS->GetWindowText(cstrQueueSize);

    if (cstrQueueSize.GetLength() == 0)
    {
        if (!a_pQueueSize)
        {
            UI_TRACE(_T("[0x%02x] Queue Size: %s (%s %d))"), a_pComponents->m_ucEP, cstrQueueSize, STATUS_MSG_INVALID_QUEUE_SIZE, dwMaxQueueSize);
        }
        return FALSE;
    }

    llQueueSize.QuadPart = _tstoi64(LPCTSTR(cstrQueueSize));
    if (llQueueSize.QuadPart <= 1 || llQueueSize.QuadPart > dwMaxQueueSize) 
    {
        if (!a_pQueueSize)
        {
            UI_TRACE(_T("[0x%02x] Queue Size: %s (%s %d))"), a_pComponents->m_ucEP, cstrQueueSize, STATUS_MSG_INVALID_QUEUE_SIZE, dwMaxQueueSize);
        }
        return FALSE;
    }

    if (!a_pQueueSize)
    {
        UI_TRACE(_T("[0x%02x] Queue Size: %s"), a_pComponents->m_ucEP, cstrQueueSize);
    }

    if (a_pQueueSize)
    {
        *a_pQueueSize = (USHORT)llQueueSize.QuadPart;
    }

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnCheck_WritePipe
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

VOID CUIManager::OnCheck_WritePipe(PEP_WritePipe_Components a_pComponents)
{
    BOOL bEnabled = a_pComponents->m_pCheck->GetCheck();
    if (bEnabled)
    {
        UI_TRACE(_T("[0x%02x] Enabled"), a_pComponents->m_ucEP);
    }
    else
    {
        UI_TRACE(_T("[0x%02x] Disabled"), a_pComponents->m_ucEP);
    }
    EnableDisable_WritePipe(a_pComponents, bEnabled ? ENABLE_STATE_STOPPED: ENABLE_STATE_ENABLED);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnCheck_ReadPipe
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

VOID CUIManager::OnCheck_ReadPipe(PEP_ReadPipe_Components a_pComponents)
{
    BOOL bEnabled = a_pComponents->m_pCheck->GetCheck();
    if (bEnabled)
    {
        UI_TRACE(_T("[0x%02x] Enabled"), a_pComponents->m_ucEP);
    }
    else
    {
        UI_TRACE(_T("[0x%02x] Disabled"), a_pComponents->m_ucEP);
    }
    EnableDisable_ReadPipe(a_pComponents, bEnabled ? ENABLE_STATE_STOPPED: ENABLE_STATE_ENABLED);

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnRadioPayload_WritePipe
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

VOID CUIManager::OnRadioPayload_WritePipe(PEP_WritePipe_Components a_pComponents, BOOL a_bDontUpdateData)
{
    if (!a_bDontUpdateData)
    {
        UpdateData(TRUE);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnButtonStart_WritePipe
//
// Summary
//      Entry point when START button is clicked for an OUT/Write endpoint
//
// Parameters
//      a_pComponents   - Pointer to the UI controls of the specified WRITE endpoint
//      a_pTaskParam    - Task parameters used for remembering the previous task for stress testing purposes
//
// Return Value
//      None
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

VOID CUIManager::OnButtonStart_WritePipe(PEP_WritePipe_Components a_pComponents, PTTASK_MANAGER_PARAM a_pTaskParam)
{
    if (!m_pTaskManager)
    {
        UI_TRACE(_T("m_pTaskManager is NULL!"));
        return;
    }

    BOOL bIsSelected = a_pComponents->m_pCheck->GetCheck();
    if (!bIsSelected)
    {
        return;
    }

    UpdateData(TRUE);

    LARGE_INTEGER llSessionLength = {0};
    BOOL bRet = OnEditSL_Pipe(a_pComponents, &llSessionLength);
    if (!bRet)
    {
        APP_LOG(_T("[EP%02X] %s %d)"), a_pComponents->m_ucEP, STATUS_MSG_INVALID_PACKET_SIZE, m_bIsUsb3 ? MAX_PAYLOAD_SIZE_CH1_USB3 : MAX_PAYLOAD_SIZE_CH1_USB2);
        return;
    }

    USHORT usQueueSize = 0;
    bRet = OnEditQS_Pipe(a_pComponents, &usQueueSize);
    if (!bRet)
    {
        APP_LOG(_T("[EP%02X] %s %d)"), a_pComponents->m_ucEP, STATUS_MSG_INVALID_QUEUE_SIZE, MAX_QUEUE_SIZE_CH1);
        return;
    }

	if (0 == GetCheckedRadioButton(IDC_RADIO_FT600, IDC_RADIO_FT601))
	{
		APP_LOG(_T("[EP%02X] Please Select the device type option and try again..."), a_pComponents->m_ucEP);
		return;
	}
    memset(a_pTaskParam, 0, sizeof(*a_pTaskParam));
    a_pTaskParam->m_ucEP = a_pComponents->m_ucEP;
    a_pTaskParam->m_bIsWrite = TRUE;
	a_pTaskParam->m_PayloadParam.m_eType = (ETASK_PAYLOAD_TYPE)m_radioPattern;
	a_pTaskParam->m_PayloadParam.m_ulFixedValue = m_FixedPattern;
	a_pTaskParam->m_PayloadParam.m_bIntfType = GetCheckedRadioButton(IDC_RADIO_FT600, IDC_RADIO_FT601) == IDC_RADIO_FT600 ? 0 : 1;
    a_pTaskParam->m_TestParam.m_ulQueueSize = (ULONG)usQueueSize;
    a_pTaskParam->m_TestParam.m_ulPacketSize = (ULONG)llSessionLength.QuadPart;
    a_pTaskParam->m_eAction = ETASK_TYPE_IO_START;
	//APP_LOG(_T("[EP%0x2X] Intf type: %d"), a_pComponents->m_ucEP, a_pTaskParam->m_PayloadParam.m_bIntfType);
    // Fix memory leak in CRichEditText if the number of lines increases
    // Memory leak is observed in stress test
    // By putting a limit on the maximum number of lines, we prevent memory leak.
    if (m_RichEdit_Output.GetLineCount() > 100)
    {
        m_RichEdit_Output.SetWindowText(_T(""));
    }

    bRet = m_pTaskManager->AddTask(a_pTaskParam);
    if (!bRet)
    {
        APP_LOG(_T("Error: ReadPipe is still ongoing!"));
        return;
    }
    a_pComponents->m_bTaskOngoing = TRUE;

    EnableDisable_WritePipe(a_pComponents, ENABLE_STATE_STARTED);

    GUI_LOG(_T("Write transfer started!"));
    UI_TRACE(_T("[0x%02x] Start"), 
        a_pTaskParam->m_ucEP);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnButtonStop_WritePipe
//
// Summary
//      Entry point when STOP button is clicked for an OUT/Write endpoint
//
// Parameters
//      a_pComponents   - Pointer to the UI controls of the specified WRITE endpoint
//
// Return Value
//      None
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

VOID CUIManager::OnButtonStop_WritePipe(PEP_WritePipe_Components a_pComponents)
{
    TTASK_MANAGER_PARAM taskParam = {0};


    if (!m_pTaskManager)
    {
        UI_TRACE(_T("OnButtonStop_WritePipe m_pTaskManager is NULL!"));
        return;
    }

    BOOL bIsSelected = a_pComponents->m_pCheck->GetCheck();
    if (!bIsSelected)
    {
        return;
    }

    taskParam.m_ucEP = a_pComponents->m_ucEP;
    taskParam.m_bIsWrite = TRUE;
    taskParam.m_eAction = ETASK_TYPE_IO_STOP;

    BOOL bRet = m_pTaskManager->AddTask(&taskParam);
    if (!bRet)
    {
        UI_TRACE(_T("OnButtonStop_WritePipe Adding of task failed!"));
        return;
    }
    a_pComponents->m_bTaskOngoing = FALSE;

    UI_TRACE(_T("[0x%02x] Stop"), a_pComponents->m_ucEP);

    a_pComponents->m_pButton_Stop->EnableWindow(FALSE);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnButtonStart_ReadPipe
//
// Summary
//      Entry point when START button is clicked for an IN/Read endpoint
//
// Parameters
//      a_pComponents   - Pointer to the UI controls of the specified WRITE endpoint
//      a_pTaskParam    - Task parameters used for remembering the previous task for stress testing purposes
//
// Return Value
//      None
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

VOID CUIManager::OnButtonStart_ReadPipe(PEP_ReadPipe_Components a_pComponents, PTTASK_MANAGER_PARAM a_pTaskParam)
{
    if (!m_pTaskManager)
    {
        UI_TRACE(_T("OnButtonStart_ReadPipe m_pTaskManager is NULL!"));
        return;
    }

    BOOL bIsSelected = a_pComponents->m_pCheck->GetCheck();
    if (!bIsSelected)
    {
        return;
    }

    LARGE_INTEGER llSessionLength = {0};
    BOOL bRet = OnEditSL_Pipe(a_pComponents, &llSessionLength);
    if (!bRet)
    {
        APP_LOG(_T("[EP%02X] %s %d)"), a_pComponents->m_ucEP, STATUS_MSG_INVALID_PACKET_SIZE, m_bIsUsb3 ? MAX_PAYLOAD_SIZE_CH1_USB3 : MAX_PAYLOAD_SIZE_CH1_USB2);
        return;
    }

    USHORT usQueueSize = 0;
    bRet = OnEditQS_Pipe(a_pComponents, &usQueueSize);
    if (!bRet)
    {
        APP_LOG(_T("[EP%02X] %s %d)"), a_pComponents->m_ucEP, STATUS_MSG_INVALID_QUEUE_SIZE, MAX_QUEUE_SIZE_CH1);
        return;
    }

    memset(a_pTaskParam, 0, sizeof(*a_pTaskParam));
    a_pTaskParam->m_ucEP = a_pComponents->m_ucEP;
    a_pTaskParam->m_bIsWrite = FALSE;
    a_pTaskParam->m_TestParam.m_ulQueueSize = (ULONG)usQueueSize;
    a_pTaskParam->m_TestParam.m_ulPacketSize = (ULONG)llSessionLength.QuadPart;
    a_pTaskParam->m_eAction = ETASK_TYPE_IO_START;

    // Fix memory leak in CRichEditText if the number of lines increases
    // Memory leak is observed in stress test
    // By putting a limit on the maximum number of lines, we prevent memory leak.
    if (m_RichEdit_Output.GetLineCount() > 100)
    {
        m_RichEdit_Output.SetWindowText(_T(""));
    }

    bRet = m_pTaskManager->AddTask(a_pTaskParam);
    if (!bRet)
    {
        APP_LOG(_T("Error: WritePipe is still ongoing!"));;
        return;
    }
    a_pComponents->m_bTaskOngoing = TRUE;

    EnableDisable_ReadPipe(a_pComponents, ENABLE_STATE_STARTED);

    GUI_LOG(_T("Read transfer started!"));
    UI_TRACE(_T("[0x%02x] Start"), 
        a_pTaskParam->m_ucEP);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnButtonStop_ReadPipe
//
// Summary
//      Entry point when STOP button is clicked for an IN/Read endpoint
//
// Parameters
//      a_pComponents   - Pointer to the UI controls of the specified WRITE endpoint
//
// Return Value
//      None
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

VOID CUIManager::OnButtonStop_ReadPipe(PEP_ReadPipe_Components a_pComponents)
{
    TTASK_MANAGER_PARAM taskParam = {0};

    if (!m_pTaskManager)
    {
        UI_TRACE(_T("OnButtonStop_ReadPipe m_pTaskManager is NULL!"));
        return;
    }

    BOOL bIsSelected = a_pComponents->m_pCheck->GetCheck();
    if (!bIsSelected)
    {
        return;
    }

    taskParam.m_ucEP = a_pComponents->m_ucEP;
    taskParam.m_bIsWrite = FALSE;
    taskParam.m_eAction = ETASK_TYPE_IO_STOP;

    BOOL bRet = m_pTaskManager->AddTask(&taskParam);
    if (!bRet)
    {
        UI_TRACE(_T("OnButtonStop_ReadPipe Adding of task failed!"));
        return;
    }
    a_pComponents->m_bTaskOngoing = FALSE;

    UI_TRACE(_T("[0x%02x] Stop"), a_pComponents->m_ucEP);

    a_pComponents->m_pButton_Stop->EnableWindow(FALSE);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnCheckDebugConsole
//
// Summary
//      Entry point when DEBUG CONSOLE checkbox is checked
//
// Parameters
//      None
//
// Return Value
//      None
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

VOID CUIManager::OnCheckDebugConsole()
{
    if (m_CheckDebugConsole.GetCheck())
    {
        CMD_LOG(_T("DebugConsole: 1"));
        CUtils::CMFCConsole::Show(TRUE, APPLICATION_TITLE);
    }
    else
    {
        CMD_LOG(_T("DebugConsole: 0"));
        CUtils::CMFCConsole::Show(FALSE);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnButtonClearOutput
//
// Summary
//      Entry point when CLEAR OUTPUT button is clicked
//
// Parameters
//      None
//
// Return Value
//      None
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

VOID CUIManager::OnButtonClearOutput()
{
    m_RichEdit_Output.SetWindowText(_T(""));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::DeviceDetectionRoutine
//
// Summary
//      Completion routine when device is detected
//
// Parameters
//      a_pucWriteEP    - Pointer to the array containing the IDs of the WRITE endpoints
//      a_ucNumWriteEP  - Indicates the number of WRITE endpoints
//      a_pucReadEP     - Pointer to the array containing the IDs of the READ endpoints
//      a_ucNumReadEP   - Indicates the number of READ endpoints
//      a_uwVID         - Indicates the vendor ID
//      a_uwPID         - Indicates the product ID
//
// Return Value
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

VOID CUIManager::DeviceDetectionRoutine(
    CONST PVOID a_pParam
    )
{
    PDeviceDetectionRoutineParams pParam = (PDeviceDetectionRoutineParams)a_pParam;


    // Check if device was found
    // 0 ep write and 0 ep read means device not found
    if (pParam->m_ucNumWriteEP == 0 && pParam->m_ucNumReadEP == 0)
    {
        EnableDisable_WritePipe(&m_WritePipe[0], ENABLE_STATE_DISABLED);
        EnableDisable_ReadPipe(&m_ReadPipe[0], ENABLE_STATE_DISABLED);
        ::EnterCriticalSection(&m_csCleanup);
        if (m_pTaskManager)
        {
            m_bTaskManagerExiting = TRUE;
            //m_pTaskManager->ExitThread();
            m_pTaskManager = NULL;
        }
        ::LeaveCriticalSection(&m_csCleanup);

        // Enable the open by GUID/Description
        EnableDisable_OpenControls(TRUE);

        APP_LOG(_T("Plug-in device! Application will detect it automatically!"));
    }
    else
    {
        if (pParam->m_ucNumWriteEP == 0)
        {
            EnableDisable_WritePipe(&m_WritePipe[0], ENABLE_STATE_DISABLED);
        }
        else
        {
            EnableDisable_WritePipe(&m_WritePipe[0], ENABLE_STATE_STOPPED);
        }
        if (pParam->m_ucNumReadEP == 0)
        {
            EnableDisable_ReadPipe(&m_ReadPipe[0], ENABLE_STATE_DISABLED);
        }
        else
        {
            EnableDisable_ReadPipe(&m_ReadPipe[0], ENABLE_STATE_STOPPED);
        }

        if (pParam->m_uwVID)
        {
            TCHAR szVID[7] = {0};
            _sntprintf_s(szVID, sizeof(szVID), _T("0x%04x"), pParam->m_uwVID);
            m_EditVID.SetWindowText(szVID);
        }
        if (pParam->m_uwPID)
        {
            TCHAR szPID[7] = {0};
            _sntprintf_s(szPID, sizeof(szPID), _T("0x%04x"), pParam->m_uwPID);
            m_EditPID.SetWindowText(szPID);
        }

		//APP_LOG(_T("Device Type: %d!"), GetCheckedRadioButton(IDC_RADIO_FT600, IDC_RADIO_FT601));
		if (pParam->m_eIntfType == FT_DEVICE_600)
		{
			CheckRadioButton(IDC_RADIO_FT600, IDC_RADIO_FT601, IDC_RADIO_FT600);
			GetDlgItem(IDC_RADIO_FT600)->EnableWindow(0);
			GetDlgItem(IDC_RADIO_FT601)->EnableWindow(0);
		}
		else if (pParam->m_eIntfType == FT_DEVICE_601)
		{
			CheckRadioButton(IDC_RADIO_FT600, IDC_RADIO_FT601, IDC_RADIO_FT601);
			GetDlgItem(IDC_RADIO_FT600)->EnableWindow(0);
			GetDlgItem(IDC_RADIO_FT601)->EnableWindow(0);
		}
		APP_LOG(_T("Device Type: %d "), pParam->m_eIntfType);

		APP_LOG(_T(""));
		APP_LOG(_T("Device Firmware Version: 0x%04X!"), pParam->m_ulFirmwareVersion);
		APP_LOG(_T("D3XX Driver Version: 0x%08X | D3XX Library Version: 0x%08X"), pParam->m_ulDriverVersion, pParam->m_ulLibraryVersion);
		if (pParam->m_ulDriverVersion != pParam->m_ulLibraryVersion)
		{
			APP_LOG(_T("Warning: D3XX driver and D3XX library does not match!"));
		}
		APP_LOG(_T(""));

        EnableDisable_OpenControls(FALSE);

        m_bIsUsb3 = pParam->m_bIsUsb3;
        if (m_bIsUsb3)
        {
            m_WritePipe[0].m_pEdit_SL->SetWindowText(DEFAULT_VALUE_PACKET_SIZE_USB3);
            m_ReadPipe[0].m_pEdit_SL->SetWindowText(DEFAULT_VALUE_PACKET_SIZE_USB3);
        }
        else
        {
            m_WritePipe[0].m_pEdit_SL->SetWindowText(DEFAULT_VALUE_PACKET_SIZE_USB2);
            m_ReadPipe[0].m_pEdit_SL->SetWindowText(DEFAULT_VALUE_PACKET_SIZE_USB2);
        }
    }

    RegisterDeviceNotification();

    CMD_LOG(_T("DeviceDetectionRoutine end!"));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::CompletionRoutine
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

VOID CUIManager::CompletionRoutine(UCHAR ucEP)
{
    if (ucEP & 0x80)
    {
        PEP_ReadPipe_Components pComponent = &m_ReadPipe[ucEP - BASE_READ_EP];
        if (pComponent->m_eState == ENABLE_STATE_STARTED)
        {
            EnableDisable_ReadPipe(pComponent, ENABLE_STATE_STOPPED);
        }
        GUI_LOG(_T("Read transfer stopped!"));
    }
    else
    {
        PEP_WritePipe_Components pComponent = &m_WritePipe[ucEP - BASE_WRITE_EP];
        if (pComponent->m_eState == ENABLE_STATE_STARTED)
        {
            EnableDisable_WritePipe(pComponent, ENABLE_STATE_STOPPED);
        }
        GUI_LOG(_T("Write transfer stopped!"));
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::RegisterDeviceNotification
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

VOID CUIManager::RegisterDeviceNotification()
{
    //CMD_LOG(_T("RegisterDeviceNotification! m_hDeviceNotify = %p m_iRadioOpenBy = %d"), m_hDeviceNotify, m_iRadioOpenBy);

    if (!m_hDeviceNotify)
    {
        DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
        ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
        NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
        NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        memcpy((PUCHAR)&NotificationFilter.dbcc_classguid, (PUCHAR)&m_DeviceGUID[0], sizeof(m_DeviceGUID[0]));
        m_hDeviceNotify = ::RegisterDeviceNotification(GetSafeHwnd(), &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
        if (NULL == m_hDeviceNotify) 
        {
            _tprintf(_T("RegisterDeviceNotification failed!\n"));
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::UnregisterDeviceNotification
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

VOID CUIManager::UnregisterDeviceNotification()
{
    CMD_LOG(_T("UnregisterDeviceNotification! m_hDeviceNotify = %p"), m_hDeviceNotify);
    if (m_hDeviceNotify)
    {
        BOOL bResult = ::UnregisterDeviceNotification(m_hDeviceNotify);
        if (!bResult) 
        {
            _tprintf(_T("UnregisterDeviceNotification failed!\n"));
        }

        m_hDeviceNotify = NULL;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnDeviceChange
//
// Summary
//      Start a thread that will call the device arrival or device removal procedures
//
// Parameters
//
// Return Value
//
// Notes
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CUIManager::OnDeviceChange(UINT EventType, DWORD_PTR pdwData)
{
    PDEV_BROADCAST_DEVICEINTERFACE pHandle = (PDEV_BROADCAST_DEVICEINTERFACE)pdwData;
    switch (EventType)
    {
        case DBT_DEVICEARRIVAL:
        {
            if (!m_pTaskManager)
            {
                APP_LOG(_T("Device attached! [%s]"), pHandle->dbcc_name ? pHandle->dbcc_name : _T("NULL"));

                if (pHandle->dbcc_name)
                {
                    WCHAR szGUID[64] = {0};
                    CUtils::CGuid::GUIDToString(&m_DeviceGUID[0], szGUID, sizeof(szGUID)/sizeof(szGUID[0]));

                    if (wcslen(pHandle->dbcc_name) > wcslen(szGUID))
                    {
                        if (wcsstr(_wcslwr(pHandle->dbcc_name), _wcslwr(szGUID)))//DEVICE_VID
                        {
                            APP_LOG(_T("Valid device attached! %s"), pHandle->dbcc_name);
                            // Do device arrival in a thread so that this system callback thread will not block the UI thread
                            OnDeviceChange_DeviceArrival();
                            //HANDLE hdl = (HANDLE)::_beginthreadex(NULL, 0, (PTHREAD_START)OnDeviceArrival, this, 0, NULL);
                            //CloseHandle(hdl);
                        }
                        else
                        {
                            APP_LOG(_T("Invalid device attached! %s"), pHandle->dbcc_name);
                        }
                    }
                    else
                    {
                        APP_LOG(_T("Invalid device attached! %s"), pHandle->dbcc_name);
                    }
                }
                else
                {
                    APP_LOG(_T("Invalid device attached! %s"), pHandle->dbcc_name);
                }
            }

            break;
        }
        case DBT_DEVICEREMOVECOMPLETE:
        {
            APP_LOG(_T("Device detached! [%s]"), pHandle->dbcc_name ? pHandle->dbcc_name : _T("NULL"));

            if (InternalOnDeviceChange(pdwData))
            {
                // Do device removal in a thread so that this system callback thread will not block the UI thread
                OnDeviceChange_DeviceRemoval();
                //HANDLE hdl = (HANDLE)::_beginthreadex(NULL, 0, (PTHREAD_START)OnDeviceRemoval, this, 0, NULL);
                //CloseHandle(hdl);
            }

            break;
        }
        case DBT_DEVICEQUERYREMOVE:
        {
            APP_LOG(_T("Device removed! %s"), pHandle->dbcc_name);
            break;
        }
        case DBT_DEVICEQUERYREMOVEFAILED:
        {
            APP_LOG(_T("Device removed failed! %s"), pHandle->dbcc_name);
            break;
        }
        case DBT_DEVICEREMOVEPENDING:
        {
            APP_LOG(_T("Device removed pending! %s"), pHandle->dbcc_name);
            break;
        }
        default:
        {
            break;
        }
    }

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::InternalOnDeviceChange
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

BOOL CUIManager::InternalOnDeviceChange(DWORD_PTR pdwData)
{
    PDEV_BROADCAST_DEVICEINTERFACE pHandle = (PDEV_BROADCAST_DEVICEINTERFACE)pdwData;

    if (m_pTaskManager && pHandle)
    {
        if (pHandle->dbcc_name)
        {
			std::wstring wstrDevicePath(pHandle->dbcc_name);
			std::string strDevicePath(wstrDevicePath.begin(), wstrDevicePath.end());

			if (m_pTaskManager->IsDevicePath(strDevicePath.c_str()))
			{
				CMD_LOG(_T("Device match!"));
				return TRUE;
			}

            //USHORT uwVID = 0;
            //USHORT uwPID = 0;
            //if (m_pTaskManager->GetVIDPID(&uwVID, &uwPID))
            //{
            //    TCHAR szVID[5] = {0};
            //    TCHAR szPID[5] = {0};
            //    _sntprintf_s(szVID, sizeof(szVID)/sizeof(szVID[0]), _T("%04x"), uwVID);
            //    _sntprintf_s(szPID, sizeof(szPID)/sizeof(szPID[0]), _T("%04x"), uwPID);
            //    _tcsupr_s(szVID);
            //    _tcsupr_s(szPID);

            //    TCHAR szTemp[256] = {0};
            //    _tcscpy_s(szTemp, pHandle->dbcc_name);
            //    _tcsupr_s(szTemp);

            //    if (_tcsstr(pHandle->dbcc_name, szVID) && _tcsstr(pHandle->dbcc_name, szPID))
            //    {
            //        CMD_LOG(_T("Device match!"));
            //        return TRUE;
            //    }
            //}
        }
    }

    return FALSE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnDeviceArrival
//
// Summary
//      Entry point of the device arrival thread which calls the device arrival procedures
//
// Parameters
//      a_pvParam   - Callback parameter
//
// Return Value
//      None
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

DWORD CUIManager::OnDeviceArrival(PVOID a_pvParam)
{
    CUIManager* pObj = (CUIManager*)a_pvParam;
    if (pObj)
    {
        pObj->OnDeviceChange_DeviceArrival();
    }

    return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnDeviceRemoval
//
// Summary
//      Entry point of the device removal thread which calls the device removal procedures
//
// Parameters
//      a_pvParam   - Callback parameter
//
// Return Value
//      None
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

DWORD CUIManager::OnDeviceRemoval(PVOID a_pvParam)
{
    CUIManager* pObj = (CUIManager*)a_pvParam;
    if (pObj)
    {
        pObj->OnDeviceChange_DeviceRemoval();
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnDeviceChange_DeviceArrival
//
// Summary
//      Contains procedures to execute when device is plugged
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

VOID CUIManager::OnDeviceChange_DeviceArrival()
{
    APP_LOG(_T("Please wait ..."));

    m_ButtonOpen.EnableWindow(FALSE);

    LaunchTaskManager();

    //EnableDisable_OpenControls(FALSE);

    //for (int i=0; i<NUM_EP_PAIRS; i++)
    //{
    //    EnableDisable_WritePipe(&m_WritePipe[i], m_WritePipe[i].m_pCheck->GetCheck() ? ENABLE_STATE_STOPPED : ENABLE_STATE_ENABLED);
    //    EnableDisable_ReadPipe (&m_ReadPipe[i],  m_ReadPipe[i].m_pCheck->GetCheck()  ? ENABLE_STATE_STOPPED : ENABLE_STATE_ENABLED);
    //    m_WritePipe[i].m_bTaskOngoing               = FALSE;
    //    m_ReadPipe[i].m_bTaskOngoing                = FALSE;
    //}
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CUIManager::OnDeviceChange_DeviceRemoval
//
// Summary
//      Contains procedures to execute when device is removed
//
// Parameters
//      a_bForce    - Indicates if some log will be displayed
//
// Return Value
//      None
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

VOID CUIManager::OnDeviceChange_DeviceRemoval(BOOL a_bForce)
{
    m_ButtonClose.EnableWindow(FALSE);
    EnableDisable_WritePipe(&m_WritePipe[0], ENABLE_STATE_DISABLED);
    EnableDisable_ReadPipe(&m_ReadPipe[0], ENABLE_STATE_DISABLED);

    m_EditVID.SetWindowText(_T(""));
    m_EditPID.SetWindowText(_T(""));

    ::EnterCriticalSection(&m_csCleanup);
    if (m_pTaskManager)
    {
        m_bTaskManagerExiting = TRUE;
        m_pTaskManager->ExitThread();
        Sleep(1000);
        m_pTaskManager = NULL;
    }
    ::LeaveCriticalSection(&m_csCleanup);

    if (!a_bForce)
    {
		APP_LOG(_T("Plug-in device! Application will detect it automatically!"));
    }

    EnableDisable_OpenControls(TRUE);
}






void CUIManager::OnBnClickedRadioPatternFixed()
{
	m_EditFixValue.EnableWindow(TRUE);
}


void CUIManager::OnBnClickedRadioPatternRand()
{
	m_EditFixValue.EnableWindow(FALSE);
}


void CUIManager::OnBnClickedRadioPatternInc()
{
	m_EditFixValue.EnableWindow(FALSE);
}
