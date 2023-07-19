/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_desc.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2022/08/20
 * Description        : usb device descriptor,configuration descriptor,
 *                      string descriptors and other descriptors.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "usb_desc.h"

/* Device Descriptor */
const uint8_t MyDevDescr[] = { 0x12,       // bLength
        0x01,       // bDescriptorType (Device)
        0x10, 0x01, // bcdUSB 1.10
        0x00,       // bDeviceClass
        0x00,       // bDeviceSubClass
        0x00,       // bDeviceProtocol
        DEF_USBD_UEP0_SIZE,   // bMaxPacketSize0 64
        (uint8_t) DEF_USB_VID, (uint8_t) (DEF_USB_VID >> 8),  // idVendor 0x1A86
        (uint8_t) DEF_USB_PID, (uint8_t) (DEF_USB_PID >> 8), // idProduct 0x5537
        DEF_IC_PRG_VER, 0x00, // bcdDevice 0.01
        0x01,       // iManufacturer (String Index)
        0x02,       // iProduct (String Index)
        0x00,       // iSerialNumber (String Index)
        0x01,       // bNumConfigurations 1
        };

/* Configuration Descriptor */
const uint8_t MyCfgDescr[] = {
        // Configure descriptor
        0x09, 0x02, 101, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,
        ////////////^^^^

        // Interface 0 (AudioClass) descriptor
        0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,

        //Class-specific AC Interface Descriptor (9)
        0x09, 0x24, 0x01, 0x00, 0x01, 0x09, 0x00, 0x01, 0x01,

        //MIDIStreaming Interface Descriptors
        //Standard MS Interface Descriptor (9)
        0x09, 0x04, 0x01, 0x00, 0x02, 0x01, 0x03, 0x00, 0x00,

        //Class-specific MS Interface Descriptor (7)
        0x07, 0x24, 0x01, 0x00, 0x01, 0x41, 0x00,
        //////////////////////////////^^^^ or 0x25 ~ 7+9+9+6+6

        //MIDI IN Jack Descriptor Embedded (6)
        0x06, 0x24, 0x02, 0x01, 0x01, 0x00,

        //MIDI IN Jack Descriptor (6)
        0x06, 0x24, 0x02, 0x02, 0x02, 0x00,

        //MIDI OUT Jack Descriptor Embedded(9)
        0x09, 0x24, 0x03, 0x01, 0x03, 0x01, 0x02, 0x01, 0x00,

        //MIDI OUT Jack Descriptor (9)
        0x09, 0x24, 0x03, 0x02, 0x04, 0x01, 0x01, 0x01, 0x00,

        // Standard Bulk OUT (9)
//        0x09, 0x05, 0x01, 0x02, (uint8_t) DEF_USBD_ENDP1_SIZE,
//        (uint8_t) ( DEF_USBD_ENDP1_SIZE >> 8), 0x00, 0x00, 0x00,

        0x09, 0x05, 0x02, 0x02, (uint8_t) DEF_USBD_ENDP2_SIZE,
        (uint8_t) ( DEF_USBD_ENDP2_SIZE >> 8), 0x00, 0x00, 0x00,


        // Class MS Bulk OUT (5)
        0x05, 0x25, 0x01, 0x01, 0x01,

        // Standard Bulk IN (9)
//      0x09,0x05,0x81,0x02,0x40, 0x00,0x00,0x00,0x00,
//        0x09, 0x05, 0x82, 0x02, (uint8_t) DEF_USBD_ENDP2_SIZE,
//        (uint8_t) ( DEF_USBD_ENDP2_SIZE >> 8), 0x00, 0x00, 0x00,

        0x09, 0x05, 0x81, 0x02, (uint8_t) DEF_USBD_ENDP1_SIZE,
        (uint8_t) ( DEF_USBD_ENDP1_SIZE >> 8), 0x00, 0x00, 0x00,


        // Class MS Bulk IN (5)
        0x05, 0x25, 0x01, 0x01, 0x03

};

/* Language Descriptor */
const uint8_t MyLangDescr[] = { 0x04, 0x03, 0x09, 0x04 };

/* Manufacturer Descriptor */
const uint8_t MyManuInfo[] = { 0x0E, 0x03, 's', 0, 'h', 0, 'i', 0, 'p', 0, 'p',
        0, 'o', 0 };

/* Product Information */
const uint8_t MyProdInfo[] = { 0x12, 0x03, 'b', 0x00, 'e', 0x00, 'e', 0x00, 'p',
        0x00, 'M', 0x00, 'I', 0x00, 'D', 0x00, 'I', 0x00 };

/* Serial Number Information */
const uint8_t MySerNumInfo[] = { 0x16, 0x03, '0', 0, '1', 0, '2', 0, '3', 0,
        '4', 0, '5', 0, '6', 0, '7', 0, '8', 0, '9', 0 };

