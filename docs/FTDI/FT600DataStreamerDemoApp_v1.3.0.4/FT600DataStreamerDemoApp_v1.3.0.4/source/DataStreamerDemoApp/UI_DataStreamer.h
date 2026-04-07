/*
 * FT600 Data Streamer Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#pragma once

#ifndef __AFXWIN_H__
    #error "include 'stdafx.h' before including this file for PCH"
#endif



#include "resource.h"



class CFunctionTesterApp : public CWinApp
{

public:
    CFunctionTesterApp();

public:
    virtual BOOL InitInstance();
    DECLARE_MESSAGE_MAP()

};

extern CFunctionTesterApp theApp;


