/*
 * FT600 API Usage Demo App
 *
 * Copyright (C) 2015 FTDI Chip
 *
 */

#include "stdafx.h"



static VOID DisplayDescriptor(FT_HANDLE a_FTHandle, const PFT_COMMON_DESCRIPTOR pDescriptor);


///////////////////////////////////////////////////////////////////////////////////
// Demonstrates querying of USB descriptors:
// FT_GetDeviceDescriptor           Queries the USB Device descriptor
// FT_GetConfigurationDescriptor    Queries the USB Configuration descriptor
// FT_GetInterfaceDescriptor        Queries the USB Interface descriptors
//                                  There are 2 interfaces.
//                                  1st interface is reserved. user should not touch.
//                                  2nd interface interface is for user.
// FT_GetPipeInformation            Queries the USB Pipe information; 
//                                  There are 2 pipes for 1st interface.
//                                  There are 8 pipes for the 2nd interface by default.
// FT_GetDescriptor                 Queries a USB descriptor
///////////////////////////////////////////////////////////////////////////////////
BOOL DescriptorTest()
{
    FT_DEVICE_DESCRIPTOR DeviceDescriptor = {0};
    FT_CONFIGURATION_DESCRIPTOR ConfigurationDescriptor = {0};
    FT_INTERFACE_DESCRIPTOR InterfaceDescriptor = {0};
    FT_PIPE_INFORMATION Pipe;
    FT_STATUS ftStatus = FT_OK;
    FT_HANDLE ftHandle;


	//
	// Open a device
	//
	ftStatus = FT_Create(0, FT_OPEN_BY_INDEX, &ftHandle);
    if (FT_FAILED(ftStatus))
    {
        return FALSE;
    }


    //
    // Get configuration descriptor 
    // to determine the number of interfaces (DeviceDescriptor.bNumConfigurations) in the configuration
    //
    ftStatus = FT_GetDeviceDescriptor(ftHandle, &DeviceDescriptor);
    if (FT_FAILED(ftStatus))
    {
        FT_Close(ftHandle);
        return FALSE;
    }
    DisplayDescriptor(ftHandle, (PFT_COMMON_DESCRIPTOR)&DeviceDescriptor);


    //
    // Get configuration descriptor 
    // to determine the number of interfaces (ConfigurationDescriptor.bNumInterfaces) in the configuration
    //
    ftStatus = FT_GetConfigurationDescriptor(ftHandle, &ConfigurationDescriptor);
    if (FT_FAILED(ftStatus))
    {
        FT_Close(ftHandle);
        return FALSE;
    }
    DisplayDescriptor(ftHandle, (PFT_COMMON_DESCRIPTOR)&ConfigurationDescriptor);
    if (ConfigurationDescriptor.bNumInterfaces != 2)
    {
        FT_Close(ftHandle);
        return FALSE;
    }


    // Iterate the interfaces
    for (int j=0; j<ConfigurationDescriptor.bNumInterfaces; j++)
    {
        //
        // Get interface descriptor 
        // of 2nd interface (interface[1]) to get number of pipes
        // The 1st interface is reserved for FT60X proprietary protocol design to maximize USB3.0 performance
        //
        ftStatus = FT_GetInterfaceDescriptor(ftHandle, j, &InterfaceDescriptor);
        if (FT_FAILED(ftStatus))
        {
            FT_Close(ftHandle);
            return FALSE;
        }
        DisplayDescriptor(ftHandle, (PFT_COMMON_DESCRIPTOR)&InterfaceDescriptor);

        // Iterate the endpoints of the interface
        for (int i=0; i<InterfaceDescriptor.bNumEndpoints; i++)
        {
            //
            // Get pipe information
            // to get endpoint number and endpoint type
            //
            ftStatus = FT_GetPipeInformation(ftHandle, j, i, &Pipe);
            if (FT_FAILED(ftStatus))
            {
                FT_Close(ftHandle);
                return FALSE;
            }

			CMD_LOG(_T("\n"));
            CMD_LOG(_T("\t\tPIPE INFORMATION\n"));
            CMD_LOG(_T("\t\t0x%02x\n")        , Pipe.PipeId);
            CMD_LOG(_T("\t\t0x%02x (%s)\n")   , Pipe.PipeType, 
                                              FT_IS_INTERRUPT_PIPE(Pipe.PipeType) ? _T("INTERRUPT") : _T("BULK"));
            CMD_LOG(_T("\t\t0x%04x (%d)\n")   , Pipe.MaximumPacketSize, Pipe.MaximumPacketSize);
            CMD_LOG(_T("\t\t0x%02x\n")        , Pipe.Interval);
        }
    }


    //
    // Close device handle
    //
    FT_Close(ftHandle);

    return TRUE;
}


static VOID DisplayDescriptor(FT_HANDLE a_FTHandle, const PFT_COMMON_DESCRIPTOR pDescriptor)
{
    if (!pDescriptor)
    {
        return;
    }


    FT_STATUS ftResult = FT_OK;


    switch (pDescriptor->bDescriptorType)
    {
        //--------------------------------------------------------------------------------------------//
        // USB 1.1 descriptors
        //--------------------------------------------------------------------------------------------//

        case FT_DEVICE_DESCRIPTOR_TYPE:
        {
            PFT_DEVICE_DESCRIPTOR pDeviceDesc = (PFT_DEVICE_DESCRIPTOR)pDescriptor;

            FT_STRING_DESCRIPTOR StringDescriptorManufacturer = {0};
            FT_STRING_DESCRIPTOR StringDescriptorProduct = {0};
            FT_STRING_DESCRIPTOR StringSerialNumber = {0};
            ULONG ulBytesTransferred = 0;
            ftResult = FT_GetDescriptor(a_FTHandle, 
                                        FT_STRING_DESCRIPTOR_TYPE, 
                                        pDeviceDesc->iManufacturer, 
                                        (PUCHAR)&StringDescriptorManufacturer, 
                                        sizeof(StringDescriptorManufacturer), 
                                        &ulBytesTransferred
                                        );
            //ftResult = FT_GetStringDescriptor(a_FTHandle, pDeviceDesc->iManufacturer, &StringDescriptorManufacturer);
            if (FT_FAILED(ftResult))
            {
                break;
            }
            ftResult = FT_GetDescriptor(a_FTHandle, 
                                        FT_STRING_DESCRIPTOR_TYPE, 
                                        pDeviceDesc->iProduct, 
                                        (PUCHAR)&StringDescriptorProduct, 
                                        sizeof(StringDescriptorProduct), 
                                        &ulBytesTransferred
                                        );
            //ftResult = FT_GetStringDescriptor(a_FTHandle, pDeviceDesc->iProduct, &StringDescriptorProduct);
            if (FT_FAILED(ftResult))
            {
                break;
            }
            ftResult = FT_GetDescriptor(a_FTHandle, 
                                        FT_STRING_DESCRIPTOR_TYPE, 
                                        pDeviceDesc->iSerialNumber, 
                                        (PUCHAR)&StringSerialNumber, 
                                        sizeof(StringSerialNumber), 
                                        &ulBytesTransferred
                                        );
            //ftResult = FT_GetStringDescriptor(a_FTHandle, pDeviceDesc->iSerialNumber, &StringSerialNumber);
            if (FT_FAILED(ftResult))
            {
                break;
            }

			CMD_LOG(_T("\n"));
            CMD_LOG(_T("\tDEVICE DESCRIPTOR\n"));
            CMD_LOG(_T("\tbLength:                 0x%02X (%d)\n")     , pDeviceDesc->bLength, pDeviceDesc->bLength);
            CMD_LOG(_T("\tbDescriptorType:         0x%02X\n")          , pDeviceDesc->bDescriptorType);
            CMD_LOG(_T("\tbcdUSB:                  0x%04X\n")          , pDeviceDesc->bcdUSB);
            CMD_LOG(_T("\tbDeviceClass:            0x%02X\n")          , pDeviceDesc->bDeviceClass); 
            CMD_LOG(_T("\tbDeviceSubClass:         0x%02X\n")          , pDeviceDesc->bDeviceSubClass);
            CMD_LOG(_T("\tbDeviceProtocol:         0x%02X\n")          , pDeviceDesc->bDeviceProtocol);
            CMD_LOG(_T("\tbMaxPacketSize0:         0x%02X (%d)\n")     , pDeviceDesc->bMaxPacketSize0, pDeviceDesc->bMaxPacketSize0);
            CMD_LOG(_T("\tidVendor:                0x%04X\n")          , pDeviceDesc->idVendor);
            CMD_LOG(_T("\tidProduct:               0x%04X\n")          , pDeviceDesc->idProduct);
            CMD_LOG(_T("\tbcdDevice:               0x%04X\n")          , pDeviceDesc->bcdDevice);
            CMD_LOG(_T("\tiManufacturer:           0x%02X (%s)\n")     , pDeviceDesc->iManufacturer, StringDescriptorManufacturer.szString);
            CMD_LOG(_T("\tiProduct:                0x%02X (%s)\n")     , pDeviceDesc->iProduct, StringDescriptorProduct.szString);
            CMD_LOG(_T("\tiSerialNumber:           0x%02X (%s)\n")     , pDeviceDesc->iSerialNumber, StringSerialNumber.szString);
            CMD_LOG(_T("\tbNumConfigurations:      0x%02X\n\n")        , pDeviceDesc->bNumConfigurations);

            break;
        }

        case FT_CONFIGURATION_DESCRIPTOR_TYPE:
        {
            PFT_CONFIGURATION_DESCRIPTOR pConfigDesc = (PFT_CONFIGURATION_DESCRIPTOR)pDescriptor;
            ULONG ulBytesTransferred = 0;
            FT_STRING_DESCRIPTOR StringDescriptorConfiguration = {0};
            if (pConfigDesc->iConfiguration)
            {
                ftResult = FT_GetDescriptor(a_FTHandle, 
                                            FT_STRING_DESCRIPTOR_TYPE, 
                                            pConfigDesc->iConfiguration, 
                                            (PUCHAR)&StringDescriptorConfiguration, 
                                            sizeof(StringDescriptorConfiguration), 
                                            &ulBytesTransferred
                                            );
                //ftResult = FT_GetStringDescriptor(a_FTHandle, pConfigDesc->iConfiguration, &StringDescriptorConfiguration);
                if (FT_FAILED(ftResult))
                {
                    break;
                }
            }

			CMD_LOG(_T("\n"));
            CMD_LOG(_T("\tCONFIGURATION DESCRIPTOR\n"));
            CMD_LOG(_T("\tbLength:                 0x%02X (%d)\n")     , pConfigDesc->bLength, pConfigDesc->bLength);
            CMD_LOG(_T("\tbDescriptorType:         0x%02X\n")          , pConfigDesc->bDescriptorType);
            CMD_LOG(_T("\twTotalLength:            0x%04X (%d)\n")     , pConfigDesc->wTotalLength, pConfigDesc->wTotalLength);
            CMD_LOG(_T("\tbNumInterfaces:          0x%02X\n")          , pConfigDesc->bNumInterfaces);
            CMD_LOG(_T("\tbConfigurationValue:     0x%02X\n")          , pConfigDesc->bConfigurationValue);
            CMD_LOG(_T("\tiConfiguration:          0x%02X (%s)\n")     , pConfigDesc->iConfiguration, 
                                                                       pConfigDesc->iConfiguration ? StringDescriptorConfiguration.szString : _T("NULL"));
            CMD_LOG(_T("\tbmAttributes:            0x%02X (%s)\n")     , pConfigDesc->bmAttributes,
                                                                       FT_IS_SELF_POWERED(pConfigDesc->bmAttributes) ? _T("Self Powered") : _T("Bus Powered") );
            CMD_LOG(_T("\tMaxPower:                0x%02X (%d mA)\n\n"), pConfigDesc->MaxPower, pConfigDesc->MaxPower);

            break;
        }

        case FT_STRING_DESCRIPTOR_TYPE:
        {
            PFT_STRING_DESCRIPTOR pStringDesc = (PFT_STRING_DESCRIPTOR)pDescriptor;

			CMD_LOG(_T("\n"));
            CMD_LOG(_T("\tSTRING DESCRIPTOR\n"));
            CMD_LOG(_T("\tbLength:                 0x%02X (%d)\n")   , pStringDesc->bLength, pStringDesc->bLength); 
            CMD_LOG(_T("\tbDescriptorType:         0x%02X\n")        , pStringDesc->bDescriptorType);
            CMD_LOG(_T("\tszString:                %s\n\n")          , pStringDesc->szString);

            break;
        }

        case FT_INTERFACE_DESCRIPTOR_TYPE:
        {
            PFT_INTERFACE_DESCRIPTOR pIFDesc = (PFT_INTERFACE_DESCRIPTOR)pDescriptor;
            ULONG ulBytesTransferred = 0;
            FT_STRING_DESCRIPTOR StringDescriptorInterface = {0};
            if (pIFDesc->iInterface)
            {
                ftResult = FT_GetDescriptor(a_FTHandle, 
                                            FT_STRING_DESCRIPTOR_TYPE, 
                                            pIFDesc->iInterface, 
                                            (PUCHAR)&StringDescriptorInterface, 
                                            sizeof(StringDescriptorInterface), 
                                            &ulBytesTransferred
                                            );
                //ftResult = FT_GetStringDescriptor(a_FTHandle, pIFDesc->iInterface, &StringDescriptorInterface);
                if (FT_FAILED(ftResult))
                {
                    break;
                }
            }

			CMD_LOG(_T("\n"));
            CMD_LOG(_T("\tINTERFACE DESCRIPTOR\n"));
            CMD_LOG(_T("\tbLength:                 0x%02X (%d)\n")     , pIFDesc->bLength, pIFDesc->bLength);
            CMD_LOG(_T("\tDescriptorType:          0x%02X\n")          , pIFDesc->bDescriptorType); 
            CMD_LOG(_T("\tInterfaceNumber:         0x%02X\n")          , pIFDesc->bInterfaceNumber);
            CMD_LOG(_T("\tbAlternateSetting:       0x%02X\n")          , pIFDesc->bAlternateSetting); 
            CMD_LOG(_T("\tbNumEndpoints:           0x%02X\n")          , pIFDesc->bNumEndpoints);
            CMD_LOG(_T("\tbInterfaceClass:         0x%02X\n")          , pIFDesc->bInterfaceClass);
            CMD_LOG(_T("\tbInterfaceSubClass:      0x%02X\n")          , pIFDesc->bInterfaceSubClass);
            CMD_LOG(_T("\tbInterfaceProtocol:      0x%02X\n")          , pIFDesc->bInterfaceProtocol);
            CMD_LOG(_T("\tiInterface:              0x%02X (%s)\n\n")   , pIFDesc->iInterface, 
                                                                       pIFDesc->iInterface ? StringDescriptorInterface.szString : _T("NULL"));

            break;
        }

        default:
        {
			CMD_LOG(_T("\n"));
            CMD_LOG(_T("UNKNOWN DESCRIPTOR!!!\n"));
            break;
        }
    }
}







