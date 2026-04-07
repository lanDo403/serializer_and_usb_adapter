/*
 * FT600 Data Loopback Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#include "stdafx.h"
#include "APP_PayloadGeneration.h"
#include "APP_Task.h"
#include <time.h>



#define SEMI_RANDOM_FOR_PERFORMANCE 1
#define PAYLOAD_COUNTER_OFFSET      4



////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadGenerator::CPayloadGenerator
//      This file implements the generation of payload data to be sent by the Writer threads to the device.
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

CPayloadGenerator::CPayloadGenerator():
    m_ulBufferLength(0),
    m_bCleanupDone(FALSE),
    m_ulPayloadIncrementalLastCharUsed(0),
    m_ulPayloadHeaderCounter(0)
{
    memset(&m_Payload, 0, sizeof(m_Payload));
    memset(&m_llPayloadTotal, 0, sizeof(m_llPayloadTotal));
    memset(&m_llPayloadCopied, 0, sizeof(m_llPayloadCopied));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadGenerator::~CPayloadGenerator
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

CPayloadGenerator::~CPayloadGenerator()
{
    Cleanup();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadGenerator::Initialize
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

BOOL CPayloadGenerator::Initialize(CONST PTTASK_PAYLOAD_PARAM a_pPayloadParam, CONST PLARGE_INTEGER a_pllTotalPayload, ULONG a_ulLengthForGetData)
{
    if (!a_pPayloadParam || !a_pllTotalPayload)
    {
        return FALSE;
    }

    memcpy(&m_Payload, a_pPayloadParam, sizeof(m_Payload));
    m_llPayloadCopied.QuadPart = 0;
    m_llPayloadTotal.QuadPart = a_pllTotalPayload->QuadPart;
    m_ulPayloadIncrementalLastCharUsed = 0;

    if (a_ulLengthForGetData)
    {
        m_ulBufferLength = a_ulLengthForGetData;
    }

    srand((UINT)time(NULL));

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadGenerator::Cleanup
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

VOID CPayloadGenerator::Cleanup()
{
    if (m_bCleanupDone)
    {
        return;
    }

    m_ulBufferLength = 0;

    m_bCleanupDone = TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadGenerator::ResetData
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

VOID CPayloadGenerator::ResetData()
{
    m_llPayloadCopied.QuadPart = 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadGenerator::ResetCounter
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

VOID CPayloadGenerator::ResetCounter()
{
    m_ulPayloadHeaderCounter = 0;
	m_ulPayloadIncrementalLastCharUsed = 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadGenerator::GetData
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

BOOL CPayloadGenerator::GetData(PUCHAR a_pucBuffer, ULONG a_ulBufferLength, PULONG a_pulBytesCopied)
{
	ULONG ulBytesToCopy = a_ulBufferLength;

#if 0
    // Check parameter
    if (a_ulBufferLength != m_ulBufferLength || a_pucBuffer == NULL || a_pulBytesCopied == NULL)
    {
        return FALSE;
    }

    // Compute bytes to copy
    if (m_llPayloadCopied.QuadPart == m_llPayloadTotal.QuadPart)
    {
        *a_pulBytesCopied = 0;
        return TRUE;
    }
    if (m_llPayloadCopied.QuadPart + m_ulBufferLength > m_llPayloadTotal.QuadPart)
    {
        ulBytesToCopy = (ULONG)(m_llPayloadTotal.QuadPart - m_llPayloadCopied.QuadPart);
    }
    else
    {
        ulBytesToCopy = m_ulBufferLength;
    }
#endif

    // Generate payload
    if (!GenerateData(a_pucBuffer, ulBytesToCopy))
    {
        return FALSE;
    }

    // Update counters
	m_llPayloadCopied.QuadPart += ulBytesToCopy;
    *a_pulBytesCopied = ulBytesToCopy;

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadGenerator::AddDataHeader
//
// Summary
//      Adds a header to the payload data
//
// Parameters
//      Payload data buffer
//
// Return Value
//      None
//
// Notes
//      AddDataHeader
//      Add 4-byte header to the payload
//      The 4-byte header is just a counter for better debugging purposes in the firmware
//      The header is added for every 512 bytes
////////////////////////////////////////////////////////////////////////////////////////////////////

VOID CPayloadGenerator::AddDataHeader(PUCHAR a_pucBuffer)
{
    ++m_ulPayloadHeaderCounter;

#if 0
    // big endian
    a_pucBuffer[0] = ((PUCHAR)&m_ulPayloadHeaderCounter)[3];
    a_pucBuffer[1] = ((PUCHAR)&m_ulPayloadHeaderCounter)[2];
    a_pucBuffer[2] = ((PUCHAR)&m_ulPayloadHeaderCounter)[1];
    a_pucBuffer[3] = ((PUCHAR)&m_ulPayloadHeaderCounter)[0];
#else
    // small endian
    memcpy(a_pucBuffer, (PUCHAR)&m_ulPayloadHeaderCounter, m_ulPayloadHeaderCounterSize);
#endif
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadGenerator::GenerateData
//
// Summary
//      Generate payload data
//
// Parameters
//      a_pucBuffer         - Pointer to the buffer that will contain the data
//      a_ulBytesToCopy     - Indicates the number of bytes allocated for the buffer
//
// Return Value
//      TRUE if successful, FALSE otherwise
//
// Notes
//      Generate payload data
//      Payload can be incremental, random or fixed value
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CPayloadGenerator::GenerateData(PUCHAR a_pucBuffer, ULONG a_ulBytesToCopy)
{
	//GenerateIncrementalData((PULONG)a_pucBuffer, a_ulBytesToCopy);
#if 1
	switch(m_Payload.m_eType)
    {
        //
        // Incremental payload
        //
        case ETASK_PAYLOAD_TYPE_INCREMENTAL:
        {
			if (m_Payload.m_bIntfType == 0) /* FT600 */
			{
				GenerateIncrementalData16((PUSHORT)a_pucBuffer, a_ulBytesToCopy);
			}
			else
			{
				GenerateIncrementalData((PULONG)a_pucBuffer, a_ulBytesToCopy);
			}
            break;
        }

        //
        // Random payload
        //
        case ETASK_PAYLOAD_TYPE_RANDOM:
        {
            GenerateRandomData(a_pucBuffer, a_ulBytesToCopy);
            break;
        }

        //
        // Fixed payload
        //
        case ETASK_PAYLOAD_TYPE_FIXEDVALUE:
        {
            GenerateFixedData(a_pucBuffer, a_ulBytesToCopy);
            break;
        }

        default:
        {
            return FALSE;
        }
    }
#endif
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadGenerator::GenerateIncrementalData
//
// Summary
//      Generates payload data using incremental values
//
// Parameters
//      a_pulBuffer         - Pointer to the buffer that will contain the data
//      a_ulBytesToCopy     - Indicates the number of bytes allocated for the buffer
//
// Return Value
//      None
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

VOID CPayloadGenerator::GenerateIncrementalData(PULONG a_pulBuffer, ULONG a_ulBytesToCopy)
{
    for (ULONG i=0; i<a_ulBytesToCopy/sizeof(ULONG); i++)
    {
      // Set the incremented payload character
		a_pulBuffer[i] = m_ulPayloadIncrementalLastCharUsed++;        
    }
}



VOID CPayloadGenerator::GenerateIncrementalData16(PUSHORT a_pusBuffer, ULONG a_ulBytesToCopy)
{
	for (ULONG i = 0; i<a_ulBytesToCopy / sizeof(USHORT); i++)
	{
		// Set the incremented payload character
		a_pusBuffer[i] = m_ulPayloadIncrementalLastCharUsed++;
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadGenerator::GenerateRandomData
//
// Summary
//      Generates payload data using random values
//
// Parameters
//      a_pucBuffer         - Pointer to the buffer that will contain the data
//      a_ulBytesToCopy     - Indicates the number of bytes allocated for the buffer
//
// Return Value
//      None
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////

VOID CPayloadGenerator::GenerateRandomData(PUCHAR a_pucBuffer, ULONG a_ulBytesToCopy)
{
    UCHAR j=0xFF;
    for (ULONG i=0; i<a_ulBytesToCopy; )
    {
        // Add the header counter for every 512 bytes
        if (i%m_ulPayloadHeaderCounterInterval == PAYLOAD_COUNTER_OFFSET && 
            a_ulBytesToCopy >= i+m_ulPayloadHeaderCounterSize)
        {
            AddDataHeader(a_pucBuffer+i);
            i+=m_ulPayloadHeaderCounterSize;
        }
        else
        {
#if SEMI_RANDOM_FOR_PERFORMANCE
            if (i+2 <= a_ulBytesToCopy)
            {
                static BOOL bRandom = TRUE;
                if (bRandom)
                {
                    // Set the random payload character
                    USHORT uwRand = rand();
                    a_pucBuffer[i++] = ((UCHAR*)&uwRand)[0];
                    a_pucBuffer[i++] = ((UCHAR*)&uwRand)[1];
                    bRandom = FALSE;
                }
                else
                {
                    a_pucBuffer[i++] = j--;
                    a_pucBuffer[i++] = j--;
                    bRandom = TRUE;
                }
            }
            else
            {
                // Set the random payload character
                LARGE_INTEGER llRandomizer = {0};
                QueryPerformanceCounter(&llRandomizer);
                a_pucBuffer[i] = (UCHAR)(rand() + llRandomizer.LowPart);
                i++;
            }
#else // SEMI_RANDOM_FOR_PERFORMANCE
            // Set the random payload character
            LARGE_INTEGER llRandomizer = {0};
            QueryPerformanceCounter(&llRandomizer);
            a_pucBuffer[i] = (rand() + llRandomizer.QuadPart);
            i++;
#endif // SEMI_RANDOM_FOR_PERFORMANCE

        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CPayloadGenerator::GenerateFixedData
//
// Summary
//      Generates payload data using fixed values
//
// Parameters
//      a_pucBuffer         - Pointer to the buffer that will contain the data
//      a_ulBytesToCopy     - Indicates the number of bytes allocated for the buffer
//
// Return Value
//      None
//
// Notes
//      None
//
////////////////////////////////////////////////////////////////////////////////////////////////////
VOID CPayloadGenerator::GenerateFixedData(PUCHAR a_pucBuffer, ULONG a_ulBytesToCopy)
{
    // Set the fixed value 
    for (ULONG i=0; i<a_ulBytesToCopy; i+=8)
    {
        if (i+4<=a_ulBytesToCopy)
        {
            *((ULONG*)(a_pucBuffer + i)) = m_Payload.m_ulFixedValue;
        }
        if (i+8<=a_ulBytesToCopy)
        {
            *((ULONG*)(a_pucBuffer + i + 4)) = m_Payload.m_ulFixedValue;
        }
    }


    // Add the header counter for every 512 bytes
    for (ULONG i=0; i<a_ulBytesToCopy; i+=m_ulPayloadHeaderCounterInterval)
    {
        if (a_ulBytesToCopy >= i+m_ulPayloadHeaderCounterSize)
        {
            if (i+4+PAYLOAD_COUNTER_OFFSET<a_ulBytesToCopy)
            {
                AddDataHeader(a_pucBuffer+i+PAYLOAD_COUNTER_OFFSET);
            }
        }
    }
}


