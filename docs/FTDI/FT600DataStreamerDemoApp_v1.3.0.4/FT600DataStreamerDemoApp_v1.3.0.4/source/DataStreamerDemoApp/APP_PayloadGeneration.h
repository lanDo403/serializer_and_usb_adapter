/*
 * FT600 Data Loopback Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#pragma once
#include "APP_Task.h"

class CPayloadGenerator
{

public:

    CPayloadGenerator();
    ~CPayloadGenerator();

    BOOL Initialize(CONST PTTASK_PAYLOAD_PARAM a_pPayloadParam, CONST PLARGE_INTEGER a_pllTotalPayload, ULONG a_ulLengthForGetData);
    VOID Cleanup();

    BOOL GetData(PUCHAR a_pucBuffer, ULONG a_ulBufferLength, PULONG a_pulBytesCopied);
    VOID ResetData();
    VOID ResetCounter();


private:

    VOID AddDataHeader(PUCHAR a_pucBuffer);

    BOOL GenerateData(PUCHAR a_pucBuffer, ULONG a_ulBytesToCopy);
    VOID GenerateIncrementalData(PULONG a_pulBuffer, ULONG a_ulBytesToCopy);
	VOID GenerateIncrementalData16(PUSHORT a_pusBuffer, ULONG a_ulBytesToCopy);
    VOID GenerateRandomData(PUCHAR a_pucBuffer, ULONG a_ulBytesToCopy);
    VOID GenerateFixedData(PUCHAR a_pucBuffer, ULONG a_ulBytesToCopy);


private:

    // Constant values for payload header
    static const ULONG      m_ulPayloadHeaderCounterSize = 4;       // 4-byte header
    static const ULONG      m_ulPayloadHeaderCounterInterval = 512; // 4-byte header is added for every 512 bytes

    BOOL                    m_bCleanupDone;

    // Static - values provided during Initialize()
    TTASK_PAYLOAD_PARAM     m_Payload;
    LARGE_INTEGER           m_llPayloadTotal;
    ULONG                   m_ulBufferLength;

    // Dynamic - modified during GetData()
    LARGE_INTEGER           m_llPayloadCopied;
    ULONG                   m_ulPayloadIncrementalLastCharUsed;

    // Payload counter for the payload header
    ULONG                   m_ulPayloadHeaderCounter;

}; // CPayloadGenerator



class CUnitTestPayloadGenerator
{

private:

    static void InternalTest(
        ETASK_PAYLOAD_TYPE  a_eType,
        ULONG               a_ulFixedValue,
        LONGLONG            a_llTotalPayload,
        ULONG               a_ulLengthForGetData
        )
    {
        CPayloadGenerator oPayload;
        LARGE_INTEGER llTotalPayload;
        TTASK_PAYLOAD_PARAM PayloadParam;
        ULONG ulTotalBytesCopied = 0;
        ULONG ulBytesRead = 0;
        BOOL bRet = FALSE;
        PUCHAR pucBuffer = new UCHAR[a_ulLengthForGetData];


        PayloadParam.m_eType = a_eType;
        llTotalPayload.QuadPart = a_llTotalPayload;

        bRet = oPayload.Initialize(&PayloadParam, &llTotalPayload, a_ulLengthForGetData);

        while (1)
        {
            bRet = oPayload.GetData(pucBuffer, a_ulLengthForGetData, &ulBytesRead);
            if (!bRet)
            {
                _tprintf(_T("test GetData failed!\n"));
                break;
            }
            if (ulBytesRead == 0)
            {
                _tprintf(_T("test GetData ulBytesRead == 0!\n"));
                break;
            }

            for (ULONG i=0; i<ulBytesRead; i++)
            {
                _tprintf(_T("%03d %d\n"), ulTotalBytesCopied+i, pucBuffer[i]);
            }
            _tprintf(_T("ulBytesRead %d\n"), ulBytesRead);

            ulTotalBytesCopied += ulBytesRead;
        }

        oPayload.Cleanup();

        delete[] pucBuffer;
    }


public:

    static void Test()
    {
        InternalTest(ETASK_PAYLOAD_TYPE_INCREMENTAL, 0, 266, 245);
        InternalTest(ETASK_PAYLOAD_TYPE_RANDOM, 0, 266, 245);
        InternalTest(ETASK_PAYLOAD_TYPE_FIXEDVALUE, 0XEF, 266, 245);

        InternalTest(ETASK_PAYLOAD_TYPE_INCREMENTAL, 0, 65536, 1024);
        InternalTest(ETASK_PAYLOAD_TYPE_RANDOM, 0, 65536, 1024);
        InternalTest(ETASK_PAYLOAD_TYPE_FIXEDVALUE, 0XAA, 65536, 1024);
    }

}; // CUnitTestPayload


