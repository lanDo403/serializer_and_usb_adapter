/*
 * FT600 API Usage Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 * This file demonstrates different types of loopback testing
 * using synchronous transfers and asynchronous transfers
 * as well as the streaming feature.
 *
 */

#include "stdafx.h"
#include <process.h> // for _beginthreadex()
#include <thread>         // std::thread, std::this_thread::sleep_for


///////////////////////////////////////////////////////////////////////////////////
#define BUFFER_SIZE                 4096
#define MULTI_ASYNC_BUFFER_SIZE     4096
#define MULTI_ASYNC_NUM             4
#define NUM_ITERATIONS              1
///////////////////////////////////////////////////////////////////////////////////
#if USE_245_MODE
#define NUM_CHANNELS        1
#else // USE_245_MODE
#define NUM_CHANNELS        4
#endif // USE_245_MODE
///////////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////////
// Demonstrates single channel data transfer loopback function using synchronous write and read
// FT_WritePipe         Write data to a pipe
// FT_ReadPipe          Read data from a pipe
///////////////////////////////////////////////////////////////////////////////////
BOOL LoopbackTest()
{
    FT_STATUS ftStatus = FT_OK;
    FT_HANDLE ftHandle;
    BOOL bResult = TRUE;


	//
	// Open a device
	//
	ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
    if (FT_FAILED(ftStatus))
    {
        return FALSE;
    }
 

    ftStatus = FT_EnableGPIO(ftHandle, 0x3, 0x3); 
    if (FT_FAILED(ftStatus))
    {
        CMD_LOG(_T("\t FT_EnableGPIO failed\n"));
        bResult = FALSE;
        goto exit;
    }
    //drive the GPIO LOW for data transfer.
    // otherwise FPGA will drop the data
    ftStatus = FT_WriteGPIO(ftHandle, 0x3, 0);
    if (FT_FAILED(ftStatus))
    {
        CMD_LOG(_T("\t FT_WriteGPIO failed\n"));
        bResult = FALSE;
        goto exit;
    }

    //
    // Write and read loopback transfer
    //
    DWORD dwNumIterations = NUM_ITERATIONS;
    for (DWORD i=0; i<dwNumIterations; i++)
    {
        //
        // Write to channel 1 ep 0x02
        //
        UCHAR acWriteBuf[BUFFER_SIZE] = {0xFF};
        ULONG ulBytesWritten = 0;
        CMD_LOG(_T("\tWriting %d bytes!\n"), sizeof(acWriteBuf));
        ftStatus = FT_WritePipe(ftHandle, 0x02, acWriteBuf, sizeof(acWriteBuf), &ulBytesWritten, NULL);
        CMD_LOG(_T("\tWriting %d bytes DONE!\n"), ulBytesWritten);
        if (FT_FAILED(ftStatus))
        {
            bResult = FALSE;
            goto exit;
        }
        if (ulBytesWritten != sizeof(acWriteBuf))
        {
            bResult = FALSE;
            goto exit;
        }


        //
        // Read from channel 1 ep 0x82
        // FT_ReadPipe is a blocking/synchronous function.
        // It will not return until it has received all data requested
        //
        UCHAR acReadBuf[BUFFER_SIZE] = {0xAA};
        ULONG ulBytesRead = 0;
        CMD_LOG(_T("\tReading %d bytes!\n"), sizeof(acReadBuf));
        ftStatus = FT_ReadPipe(ftHandle, 0x82, acReadBuf, sizeof(acReadBuf), &ulBytesRead, NULL);
        CMD_LOG(_T("\tReading %d bytes DONE!\n"), ulBytesRead);
        if (FT_FAILED(ftStatus))
        {
            bResult = FALSE;
            goto exit;
        }
        if (ulBytesRead != sizeof(acReadBuf))
        {
            bResult = FALSE;
            goto exit;
        }

        //
        // Verify if data written is same as data read
        //
        if (memcmp(acWriteBuf, acReadBuf, ulBytesRead))
        {
            bResult = FALSE;
            goto exit;
        }
    }


exit:

    //
    // Close device
    //
    FT_Close(ftHandle);

    return bResult;
}


///////////////////////////////////////////////////////////////////////////////////
// Demonstrates single channel loopback function using asynchronous write and read
// FT_WritePipe             Write data to a pipe
// FT_ReadPipe              Read data from a pipe
// FT_InitializeOverlapped  Initialize overlapped parameter
// FT_GetOverlappedResult   Wait for asynchronous transfer to complete
// FT_ReleaseOverlapped     Release overlapped parameter
///////////////////////////////////////////////////////////////////////////////////
BOOL AsyncLoopbackTest()
{
    FT_STATUS ftStatus = FT_OK;
    FT_HANDLE ftHandle;
    BOOL bResult = TRUE;


	//
	// Open a device
	//
	ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
    if (FT_FAILED(ftStatus))
    {
        return FALSE;
    }


    //
    // Write and read loopback transfer
    //
    DWORD dwNumIterations = NUM_ITERATIONS;
    for (DWORD i=0; i<dwNumIterations; i++)
    {
        //
        // Write to channel 1 ep 0x02
        //
        UCHAR acWriteBuf[BUFFER_SIZE] = {0xFF};
        ULONG ulBytesWritten = 0;
        ULONG ulBytesToWrite = sizeof(acWriteBuf);
        {
            // Create the overlapped io event for asynchronous transfer
            OVERLAPPED vOverlappedWrite = {0};
            ftStatus = FT_InitializeOverlapped(ftHandle, &vOverlappedWrite);
            if (FT_FAILED(ftStatus))
            {
                bResult = FALSE;
                goto exit;
            }

            // Write asynchronously
            // FT_WritePipe is a blocking/synchronous function.
            // To make it unblocking/asynchronous operation, vOverlapped parameter is supplied.
            // When FT_WritePipe is called with overlapped io, the function will immediately return with FT_IO_PENDING
            CMD_LOG(_T("\tWriting %d bytes!\n"), ulBytesToWrite);
            ftStatus = FT_WritePipe(ftHandle, 0x02, acWriteBuf, ulBytesToWrite, &ulBytesWritten, &vOverlappedWrite);
            if (ftStatus == FT_IO_PENDING)
            {
                // Poll until all data requested ulBytesToWrite is sent
                do
                {
                    // FT_GetOverlappedResult will return FT_IO_INCOMPLETE if not yet finish
                    ftStatus = FT_GetOverlappedResult(ftHandle, &vOverlappedWrite, &ulBytesWritten, FALSE);
                    if (ftStatus == FT_IO_INCOMPLETE)
                    {
                        continue;
                    }
                    else if (FT_FAILED(ftStatus))
                    {
                        FT_ReleaseOverlapped(ftHandle, &vOverlappedWrite);
                        bResult = FALSE;
                        goto exit;
                    }
                    else //if (ftStatus == FT_OK)
                    {
                        // exit now
                        CMD_LOG(_T("\tWriting %d bytes DONE!\n"), ulBytesWritten);
                        break;
                    }
                }
                while (1);
            }
            else if (FT_FAILED(ftStatus))
            {
                FT_ReleaseOverlapped(ftHandle, &vOverlappedWrite);
                bResult = FALSE;
                goto exit;
            }
            else //if (ftStatus == FT_OK)
            {
                // do nothing
            }

            // Verify if number of bytes requested is equal to the number of bytes sent
            if (ulBytesWritten != ulBytesToWrite)
            {
                FT_ReleaseOverlapped(ftHandle, &vOverlappedWrite);
                bResult = FALSE;
                goto exit;
            }

            // Delete the overlapped io event
            FT_ReleaseOverlapped(ftHandle, &vOverlappedWrite);
        }


        //
        // Read from channel 1 ep 0x82
        //
        UCHAR acReadBuf[BUFFER_SIZE] = {0xAA};
        ULONG ulBytesRead = 0;
        ULONG ulBytesToRead = sizeof(acReadBuf);
        {
            // Create the overlapped io event for asynchronous transfer
            OVERLAPPED vOverlappedRead = {0};
            ftStatus = FT_InitializeOverlapped(ftHandle, &vOverlappedRead);
            if (FT_FAILED(ftStatus))
            {
                bResult = FALSE;
                goto exit;
            }

            // Read asynchronously
            // FT_ReadPipe is a blocking/synchronous function.
            // To make it unblocking/asynchronous operation, vOverlapped parameter is supplied.
            // When FT_ReadPipe is called with overlapped io, the function will immediately return with FT_IO_PENDING
            CMD_LOG(_T("\tReading %d bytes!\n"), ulBytesToRead);
            ftStatus = FT_ReadPipe(ftHandle, 0x82, acReadBuf, ulBytesToRead, &ulBytesRead, &vOverlappedRead);
            if (ftStatus == FT_IO_PENDING)
            {
                // Poll until all data requested ulBytesToRead is received
                do
                {
                    // FT_GetOverlappedResult will return FT_IO_INCOMPLETE if not yet finish
                    ftStatus = FT_GetOverlappedResult(ftHandle, &vOverlappedRead, &ulBytesRead, FALSE);
                    if (ftStatus == FT_IO_INCOMPLETE)
                    {
                        continue;
                    }
                    else if (FT_FAILED(ftStatus))
                    {
                        FT_ReleaseOverlapped(ftHandle, &vOverlappedRead);
                        bResult = FALSE;
                        goto exit;
                    }
                    else //if (ftStatus == FT_OK)
                    {
                        // exit now
                        CMD_LOG(_T("\tReading %d bytes DONE!\n"), ulBytesRead);
                        break;
                    }
                }
                while (1);
            }
            else if (FT_FAILED(ftStatus))
            {
                FT_ReleaseOverlapped(ftHandle, &vOverlappedRead);
                bResult = FALSE;
                goto exit;
            }
            else //if (ftStatus == FT_OK)
            {
                // do nothing
            }

            // Verify if number of bytes requested is equal to the number of bytes received
            if (ulBytesRead != ulBytesToRead)
            {
                FT_ReleaseOverlapped(ftHandle, &vOverlappedRead);
                bResult = FALSE;
                goto exit;
            }

            // Delete the overlapped io event
            FT_ReleaseOverlapped(ftHandle, &vOverlappedRead);
        }

        //
        // Verify if data written is same as data read
        //
        if (memcmp(acWriteBuf, acReadBuf, ulBytesRead))
        {
            bResult = FALSE;
            goto exit;
        }
    }


exit:

    //
    // Close device
    //
    FT_Close(ftHandle);

    return bResult;
}



///////////////////////////////////////////////////////////////////////////////////
// Demonstrates single channel loopback function using multiple asynchronous write and read
// FT_WritePipe             Write data to a pipe
// FT_ReadPipe              Read data from a pipe
// FT_InitializeOverlapped  Initialize overlapped parameter
// FT_GetOverlappedResult   Wait for asynchronous transfer to complete
// FT_ReleaseOverlapped     Release overlapped parameter
///////////////////////////////////////////////////////////////////////////////////
BOOL MultiAsyncLoopbackTest()
{
    FT_STATUS ftStatus = FT_OK;
    FT_HANDLE ftHandle;
    UCHAR *acWriteBuf = new UCHAR[MULTI_ASYNC_NUM * MULTI_ASYNC_BUFFER_SIZE];
    UCHAR *acReadBuf = new UCHAR[MULTI_ASYNC_NUM * MULTI_ASYNC_BUFFER_SIZE];
    ULONG ulBytesWritten[MULTI_ASYNC_NUM] = {0};
    ULONG ulBytesRead[MULTI_ASYNC_NUM] = {0};
    OVERLAPPED vOverlappedWrite[MULTI_ASYNC_NUM] = {0};
    OVERLAPPED vOverlappedRead[MULTI_ASYNC_NUM] = {0};
    ULONG ulBytesToWrite = MULTI_ASYNC_BUFFER_SIZE;
    ULONG ulBytesToRead = MULTI_ASYNC_BUFFER_SIZE;
    BOOL bResult = TRUE;


	//
	// Open a device
	//
	ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
    if (FT_FAILED(ftStatus))
    {
        goto exit;
    }


    //
    // Start stream transfer
    // Optimization when using multi-async on a single pipe
    //
    ftStatus = FT_SetStreamPipe(ftHandle, FALSE, FALSE, 0x02, ulBytesToWrite);
    if (FT_FAILED(ftStatus))
    {
        bResult = FALSE;
        goto exit;
    }
    ftStatus = FT_SetStreamPipe(ftHandle, FALSE, FALSE, 0x82, ulBytesToRead);
    if (FT_FAILED(ftStatus))
    {
        bResult = FALSE;
        goto exit;
    }


    //
    // Create the overlapped io event for asynchronous transfer
    //
    for (DWORD j=0; j<MULTI_ASYNC_NUM; j++)
    {
        ftStatus = FT_InitializeOverlapped(ftHandle, &vOverlappedWrite[j]);
        if (FT_FAILED(ftStatus))
        {
            bResult = FALSE;
            goto exit;
        }

        ftStatus = FT_InitializeOverlapped(ftHandle, &vOverlappedRead[j]);
        if (FT_FAILED(ftStatus))
        {
            bResult = FALSE;
            goto exit;
        }
    }


    //
    // Write and read loopback transfer
    //
    DWORD dwNumIterations = NUM_ITERATIONS;
    for (DWORD i=0; i<dwNumIterations; i++)
    {
        //
        // Send asynchronous write transfer requests
        //
        for (DWORD j=0; j<MULTI_ASYNC_NUM; j++)
        {
            memset(&acWriteBuf[j*MULTI_ASYNC_BUFFER_SIZE], 0x55+j, ulBytesToWrite);
            vOverlappedWrite[j].Internal = 0;
            vOverlappedWrite[j].InternalHigh = 0;
            ulBytesWritten[j] = 0;

            // Write asynchronously
            // FT_WritePipe is a blocking/synchronous function.
            // To make it unblocking/asynchronous operation, vOverlapped parameter is supplied.
            // When FT_WritePipe is called with overlapped io, the function will immediately return with FT_IO_PENDING
            CMD_LOG(_T("\tWriting %d bytes!\n"), ulBytesToWrite);
            ftStatus = FT_WritePipe(ftHandle, 0x02, 
                                    &acWriteBuf[j*MULTI_ASYNC_BUFFER_SIZE], ulBytesToWrite, 
                                    &ulBytesWritten[j], &vOverlappedWrite[j]);
            if (ftStatus != FT_IO_PENDING)
            {
                bResult = FALSE;
                goto exit;
            }
        }

        //
        // Send asynchronous read transfer requests
        //
        for (DWORD j=0; j<MULTI_ASYNC_NUM; j++)
        {
            memset(&acReadBuf[j*MULTI_ASYNC_BUFFER_SIZE], 0xAA+j, ulBytesToRead);
            vOverlappedRead[j].Internal = 0;
            vOverlappedRead[j].InternalHigh = 0;
            ulBytesRead[j] = 0;

            // Read asynchronously
            // FT_ReadPipe is a blocking/synchronous function.
            // To make it unblocking/asynchronous operation, vOverlapped parameter is supplied.
            // When FT_ReadPipe is called with overlapped io, the function will immediately return with FT_IO_PENDING
            CMD_LOG(_T("\tReading %d bytes!\n"), ulBytesToRead);
            ftStatus = FT_ReadPipe( ftHandle, 0x82, 
                                    &acReadBuf[j*MULTI_ASYNC_BUFFER_SIZE], ulBytesToRead, 
                                    &ulBytesRead[j], &vOverlappedRead[j]);
            if (ftStatus != FT_IO_PENDING)
            {
                bResult = FALSE;
                goto exit;
            }
        }

        //
        // Wait for the asynchronous write and read transfer requests to finish
        //
        for (DWORD j=0; j<MULTI_ASYNC_NUM; j++)
        {
            ftStatus = FT_GetOverlappedResult(ftHandle, &vOverlappedWrite[j], &ulBytesWritten[j], TRUE);
            if (FT_FAILED(ftStatus))
            {
                bResult = FALSE;
                goto exit;
            }
            if (ulBytesWritten[j] != ulBytesToWrite)
            {
                bResult = FALSE;
                goto exit;
            }
            CMD_LOG(_T("\tWriting %d bytes DONE!\n"), ulBytesToWrite);

            ftStatus = FT_GetOverlappedResult(ftHandle, &vOverlappedRead[j], &ulBytesRead[j], TRUE);
            if (FT_FAILED(ftStatus))
            {
                bResult = FALSE;
                goto exit;
            }
            if (ulBytesRead[j] != ulBytesToRead)
            {
                bResult = FALSE;
                goto exit;
            }

            CMD_LOG(_T("\tReading %d bytes DONE!\n"), ulBytesRead[j]);

            //
            // Verify if data written is same as data read
            //
            if (memcmp(&acWriteBuf[j*MULTI_ASYNC_BUFFER_SIZE], &acReadBuf[j*MULTI_ASYNC_BUFFER_SIZE], ulBytesToRead))
            {
                bResult = FALSE;
                goto exit;
            }
        }
    }


exit:

    //
    // Delete the overlapped io event for asynchronous transfer
    //
    for (DWORD j=0; j<MULTI_ASYNC_NUM; j++)
    {
        FT_ReleaseOverlapped(ftHandle, &vOverlappedWrite[j]);
        FT_ReleaseOverlapped(ftHandle, &vOverlappedRead[j]);
    }


    //
    // Stop stream transfer
    //
    FT_ClearStreamPipe(ftHandle, FALSE, FALSE, 0x02);
    FT_ClearStreamPipe(ftHandle, FALSE, FALSE, 0x82);


    //
    // Close device
    //
    FT_Close(ftHandle);

    delete[] acWriteBuf;
    delete[] acReadBuf;
    return bResult;
}


///////////////////////////////////////////////////////////////////////////////////
// Demonstrates single channel loopback function using multiple asynchronous write and read
// FT_WritePipe             Write data to a pipe
// FT_ReadPipe              Read data from a pipe
// FT_InitializeOverlapped  Initialize overlapped parameter
// FT_GetOverlappedResult   Wait for asynchronous transfer to complete
// FT_ReleaseOverlapped     Release overlapped parameter
///////////////////////////////////////////////////////////////////////////////////
BOOL MultiWriteAsyncLoopbackTest()
{
    FT_STATUS ftStatus = FT_OK;
    FT_HANDLE ftHandle;
    UCHAR *acWriteBuf = new UCHAR[MULTI_ASYNC_NUM * MULTI_ASYNC_BUFFER_SIZE];
    UCHAR *acReadBuf = new UCHAR[MULTI_ASYNC_NUM * MULTI_ASYNC_BUFFER_SIZE];
    ULONG ulBytesWritten[MULTI_ASYNC_NUM] = {0};
    ULONG ulBytesRead = 0,ulTotalBytesRead = 0;
    OVERLAPPED vOverlappedWrite[MULTI_ASYNC_NUM] = {0};
    OVERLAPPED vOverlappedRead = {0};
    ULONG ulBytesToWrite = MULTI_ASYNC_BUFFER_SIZE;
    ULONG ulBytesToRead = MULTI_ASYNC_BUFFER_SIZE * MULTI_ASYNC_NUM;
    BOOL bResult = TRUE;


	//
	// Open a device
	//
	ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
    if (FT_FAILED(ftStatus))
    {
        goto exit;
    }


    //
    // Start stream transfer
    // Optimization when using multi-async on a single pipe
    //
    ftStatus = FT_SetStreamPipe(ftHandle, FALSE, FALSE, 0x02, ulBytesToWrite);
    if (FT_FAILED(ftStatus))
    {
        bResult = FALSE;
        goto exit;
    }
    ftStatus = FT_SetStreamPipe(ftHandle, FALSE, FALSE, 0x82, ulBytesToRead);
    if (FT_FAILED(ftStatus))
    {
        bResult = FALSE;
        goto exit;
    }


    //
    // Create the overlapped io event for asynchronous transfer
    //
    ftStatus = FT_InitializeOverlapped(ftHandle, &vOverlappedRead);
    for (DWORD j=0; j<MULTI_ASYNC_NUM; j++)
    {
        ftStatus = FT_InitializeOverlapped(ftHandle, &vOverlappedWrite[j]);
        if (FT_FAILED(ftStatus))
        {
            bResult = FALSE;
            goto exit;
        }
    }


    //
    // Write and read loopback transfer
    //
    DWORD dwNumIterations = NUM_ITERATIONS;
    for (DWORD i=0; i<dwNumIterations; i++)
    {
        //
        // Send data so there is data to read asynchronously
        //
        for (DWORD j=0; j<MULTI_ASYNC_NUM; j++)
        {
            memset(&acWriteBuf[j*MULTI_ASYNC_BUFFER_SIZE], 0x55, ulBytesToWrite);
            vOverlappedWrite[j].Internal = 0;
            vOverlappedWrite[j].InternalHigh = 0;
            ulBytesWritten[j] = 0;

            // Write asynchronously
            // FT_WritePipe is a blocking/synchronous function.
            // To make it unblocking/asynchronous operation, vOverlapped parameter is supplied.
            // When FT_WritePipe is called with overlapped io, the function will immediately return with FT_IO_PENDING
            CMD_LOG(_T("\tWriting %d bytes!\n"), ulBytesToWrite);
            ftStatus = FT_WritePipe(ftHandle, 0x02, &acWriteBuf[j*MULTI_ASYNC_BUFFER_SIZE], ulBytesToWrite, &ulBytesWritten[j], &vOverlappedWrite[j]);
            if (ftStatus != FT_IO_PENDING)
            {
                bResult = FALSE;
                goto exit;
            }

        }


        //
        // Send asynchronous read transfer requests
        //
        {
            memset(acReadBuf, 0xAA, ulBytesToRead);
            vOverlappedRead.Internal = 0;
            vOverlappedRead.InternalHigh = 0;
            ulBytesRead = 0;

            // Read asynchronously
            // FT_ReadPipe is a blocking/synchronous function.
            // To make it unblocking/asynchronous operation, vOverlapped parameter is supplied.
            // When FT_ReadPipe is called with overlapped io, the function will immediately return with FT_IO_PENDING
            CMD_LOG(_T("\tReading %d bytes!\n"), ulBytesToRead);
            ftStatus = FT_ReadPipe(ftHandle, 0x82, acReadBuf, ulBytesToRead, &ulBytesRead, &vOverlappedRead);
            if (ftStatus != FT_IO_PENDING)
            {
                bResult = FALSE;
                goto exit;
            }
        }


        //
        // Wait for the asynchronous read transfer requests to finish
        //
        for (DWORD j=0; j<MULTI_ASYNC_NUM; j++)
        {
            ftStatus = FT_GetOverlappedResult(ftHandle, &vOverlappedWrite[j], &ulBytesWritten[j], TRUE);
            if (FT_FAILED(ftStatus))
            {
                bResult = FALSE;
                goto exit;
            }
            if (ulBytesWritten[j] != ulBytesToWrite)
            {
                bResult = FALSE;
                goto exit;
            }
            CMD_LOG(_T("\tWriting %d bytes DONE!\n"), ulBytesToWrite);
        }


        //
        // Wait for the asynchronous write transfer requests to finish
        //
        do
        {
            ftStatus = FT_GetOverlappedResult(ftHandle, &vOverlappedRead, &ulBytesRead, TRUE);
            if (FT_FAILED(ftStatus))
            {
                bResult = FALSE;
                goto exit;
            }
            ulTotalBytesRead += ulBytesRead;
            if (ulTotalBytesRead != ulBytesToRead)
            {
                CMD_LOG(_T("\tReading %d bytes DONE!\n"), ulBytesRead);
                vOverlappedRead.Internal = 0;
                vOverlappedRead.InternalHigh = 0;

                ftStatus = FT_ReadPipe(ftHandle,
                    0x82,
                    acReadBuf + ulTotalBytesRead, 
                    ulBytesToRead - ulTotalBytesRead, 
                    &ulBytesRead, 
                    &vOverlappedRead);

                if (ftStatus != FT_IO_PENDING)
                {
                    bResult = FALSE;
                    goto exit;
                }

                continue;
            }
            CMD_LOG(_T("\tReading %d bytes DONE!\n"), ulBytesToRead);
            break;
        }
        while (1);

        //
        // Verify if data written is same as data read
        //
        if (memcmp(acWriteBuf, acReadBuf, ulBytesToRead))
        {
            bResult = FALSE;
            goto exit;
        }
    }


exit:

    //
    // Delete the overlapped io event for asynchronous transfer
    //
    for (DWORD j=0; j<MULTI_ASYNC_NUM; j++)
    {
        FT_ReleaseOverlapped(ftHandle, &vOverlappedWrite[j]);
    }
    FT_ReleaseOverlapped(ftHandle, &vOverlappedRead);


    //
    // Stop stream transfer
    //
    FT_ClearStreamPipe(ftHandle, FALSE, FALSE, 0x02);
    FT_ClearStreamPipe(ftHandle, FALSE, FALSE, 0x82);


    //
    // Close device
    //
    FT_Close(ftHandle);

    delete[] acWriteBuf;
    delete[] acReadBuf;
    return bResult;
}





void WriteThread(FT_HANDLE ftHandle, UCHAR* acWriteBuf, ULONG ulBytesToWrite)
{
    FT_STATUS ftStatus = FT_OK;
    OVERLAPPED vOverlappedWrite = { 0 };
    ULONG ulBytesWritten = 0;
   //
   // Start stream transfer
   //
    ftStatus = FT_SetStreamPipe(ftHandle, FALSE, FALSE, 0x02, ulBytesToWrite);
    if (FT_FAILED(ftStatus))
    {
        goto exit;
    }

    //
    // Create the overlapped io event for asynchronous transfer
    //
    ftStatus = FT_InitializeOverlapped(ftHandle, &vOverlappedWrite);


    memset(acWriteBuf, 0x55, ulBytesToWrite);
    vOverlappedWrite.Internal = 0;
    vOverlappedWrite.InternalHigh = 0;
    vOverlappedWrite.Offset = 0;
    vOverlappedWrite.OffsetHigh = 0;
    vOverlappedWrite.Pointer = 0;
    ulBytesWritten = 0;

    // Write asynchronously
    // FT_WritePipe is a blocking/synchronous function.
    // To make it unblocking/asynchronous operation, vOverlapped parameter is supplied.
    // When FT_WritePipe is called with overlapped io, the function will immediately return with FT_IO_PENDING
    CMD_LOG(_T("\tWriting %d bytes!\n"), ulBytesToWrite);
    ftStatus = FT_WritePipe(ftHandle, 0x02, acWriteBuf, ulBytesToWrite, &ulBytesWritten, &vOverlappedWrite);
    if (ftStatus != FT_IO_PENDING)
    {
        goto exit;
    }

    //
    // Wait for the asynchronous write transfer requests to finish
    //
    {
        ftStatus = FT_GetOverlappedResult(ftHandle, &vOverlappedWrite, &ulBytesWritten, TRUE);
        if (FT_FAILED(ftStatus))
        {
            goto exit;
        }
        if (ulBytesWritten != ulBytesToWrite)
        {
            goto exit;
        }
        CMD_LOG(_T("\tWriting %d bytes DONE!\n"), ulBytesToWrite);
    }

    FT_ClearStreamPipe(ftHandle, FALSE, FALSE, 0x02);
exit:
    FT_ReleaseOverlapped(ftHandle, &vOverlappedWrite);

    return;

}

void MultiReadThread(FT_HANDLE ftHandle, UCHAR* acReadBuf, ULONG ulBytesToRead, ULONG ulMaxReadSizePerRequest)
{
    FT_STATUS ftStatus = FT_OK;
    OVERLAPPED vOverlappedRead[MULTI_ASYNC_NUM] = { 0 };
    ULONG ulBytesRead[MULTI_ASYNC_NUM] = { 0 };
    ULONG ulTotalBytesRead = 0;
    DWORD j = 0;


    ftStatus = FT_SetStreamPipe(ftHandle, FALSE, FALSE, 0x82, ulBytesToRead);
    if (FT_FAILED(ftStatus))
    {
        goto exit;
    }

    for (j = 0; j < MULTI_ASYNC_NUM; j++)
    {
        ftStatus = FT_InitializeOverlapped(ftHandle, &vOverlappedRead[j]);
        if (FT_FAILED(ftStatus))
        {
            goto exit;
        }
    }


    //
    // Send asynchronous read transfer requests
    //
    for (j = 0; j < MULTI_ASYNC_NUM; j++)
    {
        memset(&acReadBuf[j * MULTI_ASYNC_BUFFER_SIZE], 0xAA + j, ulMaxReadSizePerRequest);
        vOverlappedRead[j].Internal = 0;
        vOverlappedRead[j].InternalHigh = 0;
        vOverlappedRead[j].Offset = 0;
        vOverlappedRead[j].OffsetHigh = 0;
        vOverlappedRead[j].Pointer = 0;
        ulBytesRead[j] = 0;

        // Read asynchronously
        // FT_ReadPipe is a blocking/synchronous function.
        // To make it unblocking/asynchronous operation, vOverlapped parameter is supplied.
        // When FT_ReadPipe is called with overlapped io, the function will immediately return with FT_IO_PENDING
        CMD_LOG(_T("\tReading %d bytes!\n"), ulMaxReadSizePerRequest);
        ftStatus = FT_ReadPipe(ftHandle, 0x82, &acReadBuf[j * MULTI_ASYNC_BUFFER_SIZE], ulMaxReadSizePerRequest, &ulBytesRead[j], &vOverlappedRead[j]);
        if (ftStatus != FT_IO_PENDING)
        {
            goto exit;
        }
    }

    //
    // Wait for the asynchronous read transfer requests to finish
    //
    j = 0;
    do
    {
        ftStatus = FT_GetOverlappedResult(ftHandle, &vOverlappedRead[j], &ulBytesRead[j], TRUE);
        if (FT_FAILED(ftStatus))
        {
            goto exit;
        }
       
        CMD_LOG(_T("\tReading %d bytes DONE!\n"), ulBytesRead[j]);
        ulTotalBytesRead += ulBytesRead[j];
        if (ulBytesRead[j] < ulMaxReadSizePerRequest)
        {
            vOverlappedRead[j].Internal = 0;
            vOverlappedRead[j].InternalHigh = 0;
            vOverlappedRead[j].Offset = 0;
            vOverlappedRead[j].OffsetHigh = 0;
            vOverlappedRead[j].Pointer = 0;
            ulBytesRead[j] = 0;

            // Read asynchronously
            // FT_ReadPipe is a blocking/synchronous function.
            // To make it unblocking/asynchronous operation, vOverlapped parameter is supplied.
            // When FT_ReadPipe is called with overlapped io, the function will immediately return with FT_IO_PENDING
            CMD_LOG(_T("\tReading %d bytes!\n"), ulMaxReadSizePerRequest);
            ftStatus = FT_ReadPipe(ftHandle, 
                0x82,
                acReadBuf+ (j * MULTI_ASYNC_BUFFER_SIZE)+ ulBytesRead[j],
                ulMaxReadSizePerRequest - ulBytesRead[j],
                &ulBytesRead[j],
                &vOverlappedRead[j]);
            if (ftStatus != FT_IO_PENDING)
            {
                goto exit;
            }
        }
        j++;
        if (j == MULTI_ASYNC_NUM)
            j = 0;

    } while (ulTotalBytesRead < ulBytesToRead);

    FT_ClearStreamPipe(ftHandle, FALSE, FALSE, 0x82);

exit:
    //
    // Delete the overlapped io event for asynchronous transfer
    //
    for (DWORD j = 0; j < MULTI_ASYNC_NUM; j++)
    {
        FT_ReleaseOverlapped(ftHandle, &vOverlappedRead[j]);
    }
    return;
}
///////////////////////////////////////////////////////////////////////////////////
// Demonstrates single channel loopback function using multiple asynchronous read
// FT_WritePipe             Write data to a pipe
// FT_ReadPipe              Read data from a pipe
// FT_InitializeOverlapped  Initialize overlapped parameter
// FT_GetOverlappedResult   Wait for asynchronous transfer to complete
// FT_ReleaseOverlapped     Release overlapped parameter
///////////////////////////////////////////////////////////////////////////////////
BOOL MultiReadAsyncLoopbackTest()
{
    FT_STATUS ftStatus = FT_OK;
    FT_HANDLE ftHandle;
    UCHAR* acWriteBuf = new UCHAR[MULTI_ASYNC_NUM * MULTI_ASYNC_BUFFER_SIZE];
    UCHAR* acReadBuf = new UCHAR[MULTI_ASYNC_NUM * MULTI_ASYNC_BUFFER_SIZE];
    ULONG ulBytesToWrite = MULTI_ASYNC_BUFFER_SIZE * MULTI_ASYNC_NUM;
    ULONG ulBytesToRead = ulBytesToWrite;
    ULONG ulMaxReadSizePerRequest = MULTI_ASYNC_BUFFER_SIZE;
    BOOL bResult = TRUE;



    //
    // Open a device
    //
    ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
    if (FT_FAILED(ftStatus))
    {
        return FALSE;
    }

    std::thread t1(WriteThread, ftHandle, acWriteBuf, ulBytesToWrite);
    std::thread t2(MultiReadThread, ftHandle, acReadBuf, ulBytesToRead, ulMaxReadSizePerRequest);

    t1.join();
    t2.join();

    //
    // Verify if data written is same as data read
    //
    if (memcmp(acWriteBuf, acReadBuf, ulBytesToRead))
    {
        bResult = FALSE;
    }



    //
    // Close device
    //
    FT_Close(ftHandle);

    delete[] acWriteBuf;
    delete[] acReadBuf;
    return bResult;
}


///////////////////////////////////////////////////////////////////////////////////
// Demonstrates single channel loopback function using synchronous write and read using streaming mode
// FT_WritePipe             Write data to a pipe
// FT_ReadPipe              Read data from a pipe
// FT_SetStreamPipe         Starts streaming mode transfer
// FT_ClearStreamPipe       Ends streaming mode transfer
///////////////////////////////////////////////////////////////////////////////////
BOOL StreamingLoopbackTest()
{
    FT_STATUS ftStatus = FT_OK;
    FT_HANDLE ftHandle;
    BOOL bResult = TRUE;


	//
	// Open a device
	//
	ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
    if (FT_FAILED(ftStatus))
    {
        return FALSE;
    }


    //
    // Start stream transfer
    //
    ftStatus = FT_SetStreamPipe(ftHandle, FALSE, FALSE, 0x02, BUFFER_SIZE);
    if (FT_FAILED(ftStatus))
    {
        bResult = FALSE;
        goto exit;
    }
    ftStatus = FT_SetStreamPipe(ftHandle, FALSE, FALSE, 0x82, BUFFER_SIZE);
    if (FT_FAILED(ftStatus))
    {
        bResult = FALSE;
        goto exit;
    }


    //
    // Write and read loopback transfer
    //
    DWORD dwNumIterations = NUM_ITERATIONS;
    for (DWORD i=0; i<dwNumIterations; i++)
    {
        //
        // Write to channel 1 ep 0x02
        //
        UCHAR acWriteBuf[BUFFER_SIZE] = {0xFF};
        ULONG ulBytesWritten = 0;
        CMD_LOG(_T("\tWriting %d bytes!\n"), sizeof(acWriteBuf));
        ftStatus = FT_WritePipe(ftHandle, 0x02, acWriteBuf, sizeof(acWriteBuf), &ulBytesWritten, NULL);
        CMD_LOG(_T("\tWriting %d bytes DONE!\n"), ulBytesWritten);
        if (FT_FAILED(ftStatus) || ulBytesWritten != sizeof(acWriteBuf))
        {
            bResult = FALSE;
            goto exit;
        }


        //
        // Read from channel 1 ep 0x82
        // FT_ReadPipe is a blocking/synchronous function.
        // It will not return until it has received all data requested
        //
        UCHAR acReadBuf[BUFFER_SIZE] = {0xAA};
        ULONG ulBytesRead = 0;
        ULONG ulTotalBytesRead = 0;
        do
        {
            CMD_LOG(_T("\tReading %d bytes!\n"), sizeof(acReadBuf));
            ftStatus = FT_ReadPipe(ftHandle, 
                0x82, 
                acReadBuf + ulTotalBytesRead, 
                sizeof(acReadBuf) - ulTotalBytesRead,
                &ulBytesRead,
                NULL);
            CMD_LOG(_T("\tReading %d bytes DONE!\n"), ulBytesRead);
            if (FT_FAILED(ftStatus))
            {
                bResult = FALSE;
                goto exit;
            }
            ulTotalBytesRead += ulBytesRead;
        } while (ulTotalBytesRead < sizeof(acReadBuf));

        //
        // Verify if data written is same as data read
        //
        if (memcmp(acWriteBuf, acReadBuf, ulBytesRead))
        {
            bResult = FALSE;
            goto exit;
        }
    }


exit:

    //
    // Stop stream transfer
    //
    FT_ClearStreamPipe(ftHandle, FALSE, FALSE, 0x02);
    FT_ClearStreamPipe(ftHandle, FALSE, FALSE, 0x82);


    //
    // Close device
    //
    FT_Close(ftHandle);

    return bResult;
}


///////////////////////////////////////////////////////////////////////////////////
// Demonstrates multi channel loopback function using asynchronous write and read
// FT_WritePipe             Write data to a pipe
// FT_ReadPipe              Read data from a pipe
// FT_InitializeOverlapped  Initialize overlapped parameter
// FT_GetOverlappedResult   Wait for asynchronous transfer to complete
// FT_ReleaseOverlapped     Release overlapped parameter
///////////////////////////////////////////////////////////////////////////////////
BOOL MultiChannelAsyncLoopbackTest()
{
    FT_STATUS ftStatus = FT_OK;
    FT_HANDLE ftHandle;
    BOOL bResult = TRUE;


	//
	// Open a device
	//
	ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
    if (FT_FAILED(ftStatus))
    {
        return FALSE;
    }


    //
    // Create the overlapped io event for asynchronous transfer
    //
    OVERLAPPED vOverlappedWrite[NUM_CHANNELS] = {0};
    OVERLAPPED vOverlappedRead[NUM_CHANNELS] = {0};
    for (DWORD i=0; i<NUM_CHANNELS; i++)
    {
        ftStatus = FT_InitializeOverlapped(ftHandle, &vOverlappedWrite[i]);
        if (FT_FAILED(ftStatus))
        {
            bResult = FALSE;
            goto exit;
        }

        ftStatus = FT_InitializeOverlapped(ftHandle, &vOverlappedRead[i]);
        if (FT_FAILED(ftStatus))
        {
            bResult = FALSE;
            goto exit;
        }
    }


    //
    // Write and read loopback transfer
    //
    DWORD dwNumIterations = NUM_ITERATIONS;
    for (DWORD i=0; i<dwNumIterations; i++)
    {
        UCHAR acWriteBuf[NUM_CHANNELS][BUFFER_SIZE] = {0xFF};
        ULONG ulBytesWritten[NUM_CHANNELS] = {0};
        ULONG ulBytesToWrite = BUFFER_SIZE;
        UCHAR acReadBuf[NUM_CHANNELS][BUFFER_SIZE] = {0xAA};
        ULONG ulBytesRead[NUM_CHANNELS] = {0};
        ULONG ulBytesToRead = BUFFER_SIZE;


        //
        // Write asynhronously to all channels ep 0x02, 0x03, 0x04, 0x05
        // Read asynchronously to all channels ep 0x82, 0x83, 0x84, 0x85
        //
        for (UCHAR x=0; x<NUM_CHANNELS; x++)
        {
            // Write asynchronously
            // FT_WritePipe is a blocking/synchronous function.
            // To make it unblocking/asynchronous operation, vOverlapped parameter is supplied.
            // When FT_WritePipe is called with overlapped io, the function will immediately return with FT_IO_PENDING
            CMD_LOG(_T("\tWriting %d bytes!\n"), ulBytesToWrite);
            ftStatus = FT_WritePipe(ftHandle, 0x02+x, &acWriteBuf[x][0], ulBytesToWrite, &ulBytesWritten[x], &vOverlappedWrite[x]);
            if (ftStatus != FT_IO_PENDING)
            {
                bResult = FALSE;
                goto exit;
            }

            // Read asynchronously
            // FT_ReadPipe is a blocking/synchronous function.
            // To make it unblocking/asynchronous operation, vOverlapped parameter is supplied.
            // When FT_ReadPipe is called with overlapped io, the function will immediately return with FT_IO_PENDING
            CMD_LOG(_T("\tReading %d bytes!\n"), ulBytesToRead);
            ftStatus = FT_ReadPipe(ftHandle, 0x82+x, &acReadBuf[x][0], ulBytesToRead, &ulBytesRead[x], &vOverlappedRead[x]);
            if (ftStatus != FT_IO_PENDING)
            {
                bResult = FALSE;
                goto exit;
            }
        }


        //
        // Wait for all channels to finish writing
        //
        for (UCHAR x=0; x<NUM_CHANNELS; x++)
        {
            // FT_GetOverlappedResult will only return when it is completed since bWait = TRUE
            ftStatus = FT_GetOverlappedResult(ftHandle, &vOverlappedWrite[x], &ulBytesWritten[x], TRUE);
            CMD_LOG(_T("\tWriting %d bytes DONE!\n"), ulBytesWritten[x]);
            if (FT_FAILED(ftStatus))
            {
                bResult = FALSE;
                goto exit;
            }

            // Verify if number of bytes requested is equal to the number of bytes transferred
            if (ulBytesWritten[x] != ulBytesToWrite)
            {
                bResult = FALSE;
                goto exit;
            }
        }


        //
        // Wait for all channels to finish reading
        //
        for (UCHAR x=0; x<NUM_CHANNELS; x++)
        {
            // FT_GetOverlappedResult will only return when it is completed since bWait = TRUE
            ftStatus = FT_GetOverlappedResult(ftHandle, &vOverlappedRead[x], &ulBytesRead[x], TRUE);
            CMD_LOG(_T("\tReading %d bytes DONE!\n"), ulBytesRead[x]);
            if (FT_FAILED(ftStatus))
            {
                bResult = FALSE;
                goto exit;
            }

            // Verify if number of bytes requested is equal to the number of bytes transferred
            if (ulBytesRead[x] != ulBytesToRead)
            {
                bResult = FALSE;
                goto exit;
            }
        }

        //
        // Verify if data written is same as data read
        //
        for (UCHAR x = 0; x < NUM_CHANNELS; x++)
        {
            if (memcmp(&acWriteBuf[x], &acReadBuf[x], ulBytesRead[x]))
            {
                bResult = FALSE;
                goto exit;
            }
        }
     }


exit:

    //
    // Delete the overlapped io events
    //
    for (DWORD i=0; i<NUM_CHANNELS; i++)
    {
        FT_ReleaseOverlapped(ftHandle, &vOverlappedWrite[i]);
        FT_ReleaseOverlapped(ftHandle, &vOverlappedRead[i]);
    }


    //
    // Close device
    //
    FT_Close(ftHandle);

    return bResult;
}


///////////////////////////////////////////////////////////////////////////////////
// Demonstrates multi threaded multi channel loopback function using synchronous write and read
// FT_WritePipe             Write data to a pipe
// FT_ReadPipe              Read data from a pipe
///////////////////////////////////////////////////////////////////////////////////

typedef struct _THREADPARAM
{
    FT_HANDLE ftHandle;
    ULONG ulBufSize;
    ULONG ulIterations;
    UCHAR iChannel;
    BOOL bStatus;

} THREADPARAM, *PTHREADPARAM;

static UINT DoMultiChannelLoopbackTest(VOID* a_pvThreadParam)
{
    FT_STATUS ftStatus = FT_OK;
    THREADPARAM* pParam = (PTHREADPARAM)a_pvThreadParam;
    UCHAR* acWriteBuf = new UCHAR[pParam->ulBufSize];
    UCHAR* acReadBuf = new UCHAR[pParam->ulBufSize];
    BOOL bResult = TRUE;


    //
    // Write and read loopback transfer
    //
    for (DWORD i=0; i<pParam->ulIterations; i++)
    {
        //
        // Write to channel 1 ep 0x02
        //
        memset(acWriteBuf, 0xAA, pParam->ulBufSize);
        ULONG ulBytesWritten = 0;
        CMD_LOG(_T("\tWriting %d bytes!\n"), pParam->ulBufSize);
        ftStatus = FT_WritePipe(pParam->ftHandle, 0x02+pParam->iChannel, acWriteBuf, pParam->ulBufSize, &ulBytesWritten, NULL);
        CMD_LOG(_T("\tWriting %d bytes DONE!\n"), ulBytesWritten);
        if (FT_FAILED(ftStatus))
        {
            bResult = FALSE;
            goto exit;
        }
        if (ulBytesWritten != pParam->ulBufSize)
        {
            bResult = FALSE;
            goto exit;
        }


        //
        // Read from channel 1 ep 0x82
        // FT_ReadPipe is a blocking/synchronous function.
        // It will not return until it has received all data requested
        //
        memset(acReadBuf, 0x55, pParam->ulBufSize);
        ULONG ulBytesRead = 0;
        CMD_LOG(_T("\tReading %d bytes!\n"), pParam->ulBufSize);
        ftStatus = FT_ReadPipe(pParam->ftHandle, 0x82+pParam->iChannel, acReadBuf, pParam->ulBufSize, &ulBytesRead, NULL);
        CMD_LOG(_T("\tReading %d bytes DONE!\n"), ulBytesRead);
        if (FT_FAILED(ftStatus))
        {
            bResult = FALSE;
            goto exit;
        }
        if (ulBytesRead != pParam->ulBufSize)
        {
            bResult = FALSE;
            goto exit;
        }

        //
        // Verify if data written is same as data read
        //
        if (memcmp(acWriteBuf, acReadBuf, ulBytesRead))
        {
            bResult = FALSE;
            goto exit;
        }
    }


exit:

    pParam->bStatus = bResult;
    delete[] acWriteBuf;
    delete[] acReadBuf;
    return 0;
}


typedef unsigned (__stdcall *PTHREAD_START) (void *);
BOOL MultiThreadedMultiChannelLoopbackTest()
{
    FT_STATUS ftStatus = FT_OK;
    FT_HANDLE ftHandle;
    BOOL bResult = TRUE;


	//
	// Open a device
	//
	ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
    if (FT_FAILED(ftStatus))
    {
        return FALSE;
    }


    //
    // Start all 4 threads
    //
    HANDLE hThread[NUM_CHANNELS] = {NULL};
    THREADPARAM oThreadParam[NUM_CHANNELS] = {0};
    for (UCHAR i=0; i<NUM_CHANNELS; i++)
    {
        oThreadParam[i].ftHandle = ftHandle;
        oThreadParam[i].iChannel = i;
        oThreadParam[i].bStatus = TRUE;
        oThreadParam[i].ulBufSize = BUFFER_SIZE;
        oThreadParam[i].ulIterations = NUM_ITERATIONS;
        //CMD_LOG(_T("\n\tChannel %d,Threads STARTED\n"), i);
        hThread[i] = (HANDLE)::_beginthreadex(NULL, 0, (PTHREAD_START)DoMultiChannelLoopbackTest, &oThreadParam[i], 0, NULL);
    }


    //
    // Wait for all threads to finish
    //
    WaitForMultipleObjects(NUM_CHANNELS, hThread, TRUE, INFINITE);


    //
    // Check status of each thread
    //
    for (UCHAR i=0; i<NUM_CHANNELS; i++)
    {
        //CMD_LOG(_T("\n\tChannel %d,Threads ENDED,%s\n"), i, oThreadParam[i].bStatus ? _T("SUCCESS") : _T("FAILED"));
        if (!oThreadParam[i].bStatus)
        {
            bResult = FALSE;
        }
    }


    //
    // Close device
    //
    FT_Close(ftHandle);

    return bResult;
}









