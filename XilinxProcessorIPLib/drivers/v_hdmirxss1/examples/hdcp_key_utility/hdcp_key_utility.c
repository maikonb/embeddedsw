/******************************************************************************
Copyright (C) 2018 - 2022 Xilinx, Inc. All rights reserved.
Copyright 2023-2024 Advanced Micro Devices, Inc. All Rights Reserved.
SPDX-License-Identifier: MIT
******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "platform.h"
#include "xparameters.h"
#if defined (XPS_BOARD_VEK280_ES) || \
	defined (XPS_BOARD_VEK280_ES_REVB)
#define XPS_BOARD_VEK280
#endif
#if defined(__arm__) || (__aarch64__)  && (!defined XPS_BOARD_VEK280)
#include "xiicps.h"
#else
#include "xiic.h"
#endif

#include "xil_io.h"
#include "aes256.h"
#include "xhdcp22_common.h"

#if defined (XPAR_XUARTPSV_NUM_INSTANCES)
#include "xuartpsv.h"
#elif defined (XPAR_XUARTLITE_NUM_INSTANCES)
#include "xuartlite_l.h"
#else
#include "xuartps.h"
#endif
#include "video_fmc.h"

/************************** Constant Definitions *****************************/
#if defined (XPS_BOARD_VEK280)
#define EEPROM_ADDRESS				0x50	 /* 0xA0 as an 8 bit number */
#else
#define EEPROM_ADDRESS				0x53	 /* 0xA0 as an 8 bit number */
#endif
#define PAGE_SIZE					16

#if defined (XPAR_XUARTPSV_NUM_INSTANCES )
#define UART_BASE_ADDRESS XPAR_XUARTPSV_0_BASEADDR
#elif defined (XPAR_XUARTLITE_NUM_INSTANCES)
#define UART_BASE_ADDRESS XPAR_MB_SS_0_AXI_UARTLITE_BASEADDR
#else
#define UART_BASE_ADDRESS XPAR_XUARTPS_0_BASEADDR
#endif

#define SIGNATURE_OFFSET			0
#define HDCP22_LC128_OFFSET			16
#define HDCP22_CERTIFICATE_OFFSET	32
#define HDCP14_KEY1_OFFSET			1024
#define HDCP14_KEY2_OFFSET			1536

#define IIC_SCLK_RATE		100000

#if defined(__arm__) || (__aarch64__)  && (!defined XPS_BOARD_VEK280)
#define I2C_REPEATED_START 0x01
#define I2C_STOP 0x00
#else
#define I2C_REPEATED_START XIIC_REPEATED_START
#define I2C_STOP XIIC_STOP
#endif

/************************** Function Prototypes ******************************/
static unsigned XHdcp_I2cSend(void *IicPtr, u16 SlaveAddr, u8 *MsgPtr,
							unsigned ByteCount, u8 Option);
static unsigned XHdcp_I2cRecv(void *IicPtr, u16 SlaveAddr, u8 *BufPtr,
							unsigned ByteCount, u8 Option);
u8 EnterPassword (u8 *Password);

void Decrypt (u8 *CipherBuffer, u8 *PlainBuffer, u8 *Key, u16 Length);
void Encrypt (u8 *PlainBuffer, u8 *CipherBuffer, u8 *Key, u16 Length);
u16 Verify (void *IicPtr, u16 Address, u8 *BufferPtr, u16 Length, u8 *Key, u8 Encrypted);
u16 Store (void *IicPtr, u16 Address, u8 *BufferPtr, u16 Length);
u16 Get (void *IicPtr, u16 Address, u8 *BufferPtr, u16 Length);
void Erase (void *IicPtr);
u16 Round16 (u16 Size);
unsigned EepromWriteByte(void *IicPtr, u16 Address, u8 *BufferPtr, u16 ByteCount);
unsigned EepromReadByte(void *IicPtr, u16 Address, u8 *BufferPtr, u16 ByteCount);

/************************** Variable Definitions **************************/
#if defined(__arm__) || (__aarch64__) && (!defined XPS_BOARD_VEK280)
XIicPs Ps_Iic0, Iic;
#define PS_IIC_CLK 100000
#else
XIic Iic;
#endif

int ErrorCount;			  /* The Error Count */

u8 WriteBuffer[PAGE_SIZE];	  /* Write buffer for writing a page */
u8 ReadBuffer[PAGE_SIZE];	  /* Read buffer for reading a page */


u8 HdcpSignature[16] = {"xilinx_hdcp_keys"};
u8 Hdcp22Lc128[] =
{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

u8 Hdcp22Key[] =
{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


u8 Hdcp14Key1[] =
{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

u8 Hdcp14Key2[] =
{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


int main()
{
	u16 BytesWritten;
	u16 Result;
    u8 CipherBuffer[1024];
    u8 Password[32];
    u8 Key[32];
#if defined(__arm__) || (__aarch64__)  && (!defined XPS_BOARD_VEK280)
	XIicPs_Config *XIic0Ps_ConfigPtr;
	XIicPs_Config *XIic1Ps_ConfigPtr;
#else
	XIic_Config *XIic_ConfigPtr;
#endif
	u32 Status;

    init_platform();

#if (defined (ARMR5) || (__aarch64__)) && (!defined XPS_BOARD_ZCU104) && (!defined XPS_BOARD_VEK280)
#ifndef SDT
	/* Initialize PS IIC0 */
	XIic0Ps_ConfigPtr = XIicPs_LookupConfig(XPAR_XIICPS_0_DEVICE_ID);
#else
	/* Initialize PS IIC0 */
	XIic0Ps_ConfigPtr = XIicPs_LookupConfig(XPAR_XIICPS_0_BASEADDR);
#endif
	if (NULL == XIic0Ps_ConfigPtr) {
		return XST_FAILURE;
	}

	Status = XIicPs_CfgInitialize(&Ps_Iic0, XIic0Ps_ConfigPtr, XIic0Ps_ConfigPtr->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	XIicPs_Reset(&Ps_Iic0);
	/*
	 * Set the IIC serial clock rate.
	 */
	XIicPs_SetSClk(&Ps_Iic0, PS_IIC_CLK);

#ifndef SDT
	/* Initialize PS IIC1 */
	XIic1Ps_ConfigPtr = XIicPs_LookupConfig(XPAR_XIICPS_1_DEVICE_ID);
#else
	/* Initialize PS IIC1 */
	XIic1Ps_ConfigPtr = XIicPs_LookupConfig(XPAR_XIICPS_1_BASEADDR);
#endif
	if (NULL == XIic1Ps_ConfigPtr) {
		return XST_FAILURE;
	}

	Status = XIicPs_CfgInitialize(&Iic, XIic1Ps_ConfigPtr, XIic1Ps_ConfigPtr->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	XIicPs_Reset(&Iic);
	/*
	 * Set the IIC serial clock rate.
	 */
	XIicPs_SetSClk(&Iic, PS_IIC_CLK);
#else
#if defined (XPS_BOARD_VEK280)
	XIic_ConfigPtr = XIic_LookupConfig(XPAR_CIPS_SS_0_AXI_IIC_0_DEVICE_ID);
	if (!XIic_ConfigPtr)
		return XST_FAILURE;
#else
	XIic_ConfigPtr = XIic_LookupConfig(XPAR_MB_SS_0_FMCH_AXI_IIC_DEVICE_ID);
	if (!XIic_ConfigPtr)
		return XST_FAILURE;
#endif
	Status = XIic_CfgInitialize(&Iic, XIic_ConfigPtr, XIic_ConfigPtr->BaseAddress);
	if (Status != XST_SUCCESS)
		return XST_FAILURE;

#endif
	Vfmc_I2cMuxSelect(&Iic);

	xil_printf("\r\n\r\nHDCP Key EEPROM v1.0\r\n");
	xil_printf("This tool encrypts and stores the HDCP 2.2 certificate and  \r\n");
	xil_printf("HDCP 1.4 keys into the EEPROM on the HDMI FMC board\r\n\r\n");

	EnterPassword(Password);

	// Generate password hash
	XHdcp22Cmn_Sha256Hash(Password, sizeof(Password), Key);

	xil_printf("\r\n");

	// Signature
	xil_printf("Encrypt signature ");
	Encrypt(HdcpSignature, CipherBuffer, Key, sizeof(HdcpSignature));
	xil_printf(" done\r\n");

	xil_printf("Write signature ");
	BytesWritten = Store(&Iic, SIGNATURE_OFFSET, CipherBuffer, Round16(sizeof(HdcpSignature)));
	xil_printf("(%d bytes)\r\n", BytesWritten);

	xil_printf("Verify signature ");
	Result = Verify(&Iic, SIGNATURE_OFFSET, HdcpSignature, sizeof(HdcpSignature), Key, TRUE);
	if (Result == sizeof(HdcpSignature))
		xil_printf("ok (%d bytes)\r\n", Result);
	else
		xil_printf("error (%d bytes)\r\n", Result);

	// Encrypt LC128
	xil_printf("Encrypt LC128 ");
	Encrypt(Hdcp22Lc128, CipherBuffer, Key, sizeof(Hdcp22Lc128));
	xil_printf(" done\r\n");

	// Write LC128
	xil_printf("Write LC128 ");
	BytesWritten = Store(&Iic, HDCP22_LC128_OFFSET, CipherBuffer, Round16(sizeof(Hdcp22Lc128)));
	xil_printf("(%d bytes)\r\n", BytesWritten);

	// Verify LC128
	xil_printf("Verify lc128 ");
	Result = Verify(&Iic, HDCP22_LC128_OFFSET, Hdcp22Lc128, sizeof(Hdcp22Lc128), Key, TRUE);
	if (Result == sizeof(Hdcp22Lc128))
		xil_printf("ok (%d bytes)\r\n", Result);
	else
		xil_printf("error (%d bytes)\r\n", Result);

	// Encrypt HDCP2.2 ceritificate
	xil_printf("Encrypt HDCP 2.2 Certificate ");
	Encrypt(Hdcp22Key, CipherBuffer, Key, sizeof(Hdcp22Key));
	xil_printf(" done\r\n");

	// Write Key
	xil_printf("Write HDCP 2.2 Certificate ");
	BytesWritten = Store(&Iic, HDCP22_CERTIFICATE_OFFSET, CipherBuffer, Round16(sizeof(Hdcp22Key)));
	xil_printf("(%d bytes)\r\n", BytesWritten);

	// Verify Key
	xil_printf("Verify HDCP 2.2 Certificate ");
	Result = Verify(&Iic, HDCP22_CERTIFICATE_OFFSET, Hdcp22Key, sizeof(Hdcp22Key), Key, TRUE);
	if (Result == sizeof(Hdcp22Key))
		xil_printf("ok (%d bytes)\r\n", Result);
	else
		xil_printf("error (%d bytes)\r\n", Result);

	// Encrypt HDCP 1.4 key A
	xil_printf("Encrypt HDCP 1.4 Key A ");
	Encrypt(Hdcp14Key1, CipherBuffer, Key, sizeof(Hdcp14Key1));
	xil_printf(" done\r\n");

	// Write Key
	xil_printf("Write HDCP 1.4 Key A ");
	BytesWritten = Store(&Iic, HDCP14_KEY1_OFFSET, CipherBuffer, Round16(sizeof(Hdcp14Key1)));
	xil_printf("(%d bytes)\r\n", BytesWritten);

	// Verify Key
	xil_printf("Verify HDCP 1.4 Key A ");
	Result = Verify(&Iic, HDCP14_KEY1_OFFSET, Hdcp14Key1, sizeof(Hdcp14Key1), Key, TRUE);
	if (Result == sizeof(Hdcp14Key1))
		xil_printf("ok (%d bytes)\r\n", Result);
	else
		xil_printf("error (%d bytes)\r\n", Result);

	// Encrypt HDCP 1.4 key B
	xil_printf("Encrypt HDCP 1.4 Key A ");
	Encrypt(Hdcp14Key2, CipherBuffer, Key, sizeof(Hdcp14Key2));
	xil_printf(" done\r\n");

	// Write Key
	xil_printf("Write HDCP 1.4 Key B ");
	BytesWritten = Store(&Iic, HDCP14_KEY2_OFFSET, CipherBuffer, Round16(sizeof(Hdcp14Key2)));
	xil_printf("(%d bytes)\r\n", BytesWritten);

	// Verify Key
	xil_printf("Verify HDCP 1.4 Key B ");
	Result = Verify(&Iic, HDCP14_KEY2_OFFSET, Hdcp14Key2, sizeof(Hdcp14Key2), Key, TRUE);
	if (Result == sizeof(Hdcp14Key1))
		xil_printf("ok (%d bytes)\r\n", Result);
	else
		xil_printf("error (%d bytes)\r\n", Result);

	xil_printf("\r\nHDCP Key EEPROM completed.\r\n");
	xil_printf("This application is stopped.\r\n");
	cleanup_platform();
    return 0;
}

/*****************************************************************************/
/**
*
* This function send the IIC data to EEPROM
*
* @param  IicPtr IIC instance pointer.
* @param  SlaveAddr contains the 7 bit IIC address of the device to send the
*		   specified data to.
* @param MsgPtr points to the data to be sent.
* @param ByteCount is the number of bytes to be sent.
* @param Option indicates whether to hold or free the bus after
* 		  transmitting the data.
*
* @return	The number of bytes sent.
*
* @return None.
*
* @note   None.
*
******************************************************************************/
static unsigned XHdcp_I2cSend(void *IicPtr, u16 SlaveAddr, u8 *MsgPtr,
							unsigned ByteCount, u8 Option)
{
#if defined(__arm__) || (__aarch64__)  && (!defined XPS_BOARD_VEK280)
	XIicPs *Iic_Ptr = IicPtr;
	u32 Status;

	/* Set operation to 7-bit mode */
	XIicPs_SetOptions(Iic_Ptr, XIICPS_7_BIT_ADDR_OPTION);
	XIicPs_ClearOptions(Iic_Ptr, XIICPS_10_BIT_ADDR_OPTION);

	/* Set Repeated Start option */
	if (Option == I2C_REPEATED_START) {
		XIicPs_SetOptions(Iic_Ptr, XIICPS_REP_START_OPTION);
	} else {
		XIicPs_ClearOptions(Iic_Ptr, XIICPS_REP_START_OPTION);
	}

	Status = XIicPs_MasterSendPolled(Iic_Ptr, MsgPtr, ByteCount, SlaveAddr);

	/*
	 * Wait until bus is idle to start another transfer.
	 */
	if (!(Iic_Ptr->IsRepeatedStart)) {
		while (XIicPs_BusIsBusy(Iic_Ptr));
	}

	usleep(10000);

	if (Status == XST_SUCCESS) {
		return ByteCount;
	} else {
		return 0;
	}
#else
	XIic *Iic_Ptr = IicPtr;
	usleep(1000);
	return XIic_Send(Iic_Ptr->BaseAddress, SlaveAddr, MsgPtr,
					ByteCount, Option);
#endif
}

/*****************************************************************************/
/**
*
* This function send the IIC data to EEPROM
*
* @param  IicPtr IIC instance pointer.
* @param  SlaveAddr contains the 7 bit IIC address of the device to send the
*		   specified data to.
* @param BufPtr points to the memory to write the data.
* @param ByteCount is the number of bytes to be sent.
* @param Option indicates whether to hold or free the bus after
* 		  transmitting the data.
*
* @return	The number of bytes sent.
*
* @return None.
*
* @note   None.
*
******************************************************************************/
static unsigned XHdcp_I2cRecv(void *IicPtr, u16 SlaveAddr, u8 *BufPtr,
							unsigned ByteCount, u8 Option)
{
#if defined(__arm__) || (__aarch64__)  && (!defined XPS_BOARD_VEK280)
	XIicPs *Iic_Ptr = IicPtr;
	u32 Status;

	XIicPs_SetOptions(Iic_Ptr, XIICPS_7_BIT_ADDR_OPTION);
	XIicPs_ClearOptions(Iic_Ptr, XIICPS_10_BIT_ADDR_OPTION);
	if (Option == I2C_REPEATED_START) {
		XIicPs_SetOptions(Iic_Ptr, XIICPS_REP_START_OPTION);
	} else {
		XIicPs_ClearOptions(Iic_Ptr, XIICPS_REP_START_OPTION);
	}

	Status = XIicPs_MasterRecvPolled(Iic_Ptr, BufPtr, ByteCount, SlaveAddr);

	/*
	 * Wait until bus is idle to start another transfer.
	 */
	if (!(Iic_Ptr->IsRepeatedStart)) {
		while (XIicPs_BusIsBusy(Iic_Ptr));
	}

	if (Status == XST_SUCCESS) {
		return ByteCount;
	} else {
		return 0;
	}
#else
	XIic *Iic_Ptr = IicPtr;
	usleep(1000);
	return XIic_Recv(Iic_Ptr->BaseAddress, SlaveAddr, BufPtr,
					ByteCount, Option);
#endif
}

u8 EnterPassword (u8 *Password)
{
	u8 Data;
	u8 i;
	u8 *PasswordPtr;

	// Assign pointer
	PasswordPtr = Password;

	// Clear password
	memset(PasswordPtr, 0x00, 32);

	xil_printf("Enter Password ->");

	i = 0;
	while (1) {
		/* Check if the UART has any data */
#if defined (XPS_BOARD_VEK280) || \
		defined (XPS_BOARD_VCK190)
		if (XUartPsv_IsReceiveData(UART_BASE_ADDRESS)) {
			Data = XUartPsv_RecvByte(UART_BASE_ADDRESS);
			XUartPsv_SendByte(UART_BASE_ADDRESS, '.');
#else
#if defined (XPAR_XUARTLITE_NUM_INSTANCES)
		if (!XUartLite_IsReceiveEmpty(UART_BASE_ADDRESS)) {
			/* Read data from uart */
			Data = XUartLite_RecvByte(UART_BASE_ADDRESS);

			/* Send response to user */
			XUartLite_SendByte(UART_BASE_ADDRESS, '.');
#else
		if (XUartPs_IsReceiveData(UART_BASE_ADDRESS)) {
			/* Read data from uart */
			Data = XUartPs_RecvByte(UART_BASE_ADDRESS);

			/* Send response to user */
			XUartPs_SendByte(UART_BASE_ADDRESS, '.');
#endif
#endif

			/* Execute */
			if ((Data == '\n') || (Data == '\r')) {
				return TRUE;
			}

			/* Store Data */
			else {
				if (i >= 32)
					return TRUE;
				else {
					*(PasswordPtr + i) = Data;
					i++;
				}
			}
		}
	}
}


u16 Round16 (u16 Size)
{
	if (Size % 16)
		return ((Size/16)+1) * 16;
	else
		return Size;
};

void Decrypt (u8 *CipherBufferPtr, u8 *PlainBufferPtr, u8 *Key, u16 Length)
{
    u8 i;
    u8 *AesBufferPtr;
    u16 AesLength;
    aes256_context ctx;

    // Assign local Pointer
    AesBufferPtr = CipherBufferPtr;

    // Initialize AES256
    aes256_init(&ctx, Key);

    AesLength = Length/16;
    if (Length % 16) {
	AesLength++;
    }

    for (i=0; i<AesLength; i++)
    {
	// Decrypt
	aes256_decrypt_ecb(&ctx, AesBufferPtr);

		// Increment pointer
	AesBufferPtr += 16;	// The aes always encrypts 16 bytes
    }

    // Done
    aes256_done(&ctx);

   // Clear Buffer
    memset(PlainBufferPtr, 0x00, Length);

    // Copy buffers
    memcpy(PlainBufferPtr, CipherBufferPtr, Length);
}

void Encrypt (u8 *PlainBufferPtr, u8 *CipherBufferPtr, u8 *Key, u16 Length)
{
    u8 i;
    u16 AesLength;
    u8 *AesBufferPtr;

    aes256_context ctx;

    // Assign local Pointer
    AesBufferPtr = CipherBufferPtr;

    // Clear Buffer
    memset(AesBufferPtr, 0x00, Length);

    // Copy buffers
    memcpy(AesBufferPtr, PlainBufferPtr, Length);

    // Initialize AES256
    aes256_init(&ctx, Key);

    AesLength = Length/16;
    if (Length % 16) {
	AesLength++;
    }

    for (i=0; i<AesLength; i++)
    {
	// Encrypt
	aes256_encrypt_ecb(&ctx, AesBufferPtr);

	// Increment bufferpointer
	AesBufferPtr += 16;	// The aes always encrypts 16 bytes
    }

    // Done
    aes256_done(&ctx);
}

void Erase (void *IicPtr)
{
	u8 Empty[1024];

	// Clear Buffer
    memset(Empty, 0x00, sizeof(Empty));

    // Erase EEPROM
    Store(IicPtr, 0, Empty, sizeof(Empty));
}

u16 Store (void *IicPtr, u16 Address, u8 *BufferPtr, u16 Length)
{
	u16 i;
	u8 BytesWritten;
	u16 TotalBytesWritten;
	u8 *Ptr;

	Ptr = BufferPtr;
	TotalBytesWritten = 0;
	for (i=0; i<(Length / PAGE_SIZE); i++)
	{
		BytesWritten = EepromWriteByte(IicPtr, Address, Ptr, PAGE_SIZE);
		TotalBytesWritten += BytesWritten;
		Address += BytesWritten;
		Ptr += BytesWritten;
	}

	if (Length > TotalBytesWritten)
	{
		BytesWritten = EepromWriteByte(IicPtr, Address, Ptr, (Length - TotalBytesWritten));
		TotalBytesWritten += BytesWritten;
	//	xil_printf("wr : %0d\r\n", i, BytesWritten);
	}
	return TotalBytesWritten;
}

u16 Get (void *IicPtr, u16 Address, u8 *BufferPtr, u16 Length)
{
	u16 i;
	u8 BytesRead;
	u16 TotalBytesRead;
	u8 *Ptr;

	Ptr = BufferPtr;
	TotalBytesRead = 0;
	for (i=0; i<(Length / PAGE_SIZE); i++)
	{
		BytesRead = EepromReadByte(IicPtr, Address, Ptr, PAGE_SIZE);
		TotalBytesRead += BytesRead;
		Address += BytesRead;
		Ptr += BytesRead;
	}

	if (Length > TotalBytesRead)
	{
		BytesRead = EepromReadByte(IicPtr, Address, Ptr, (Length - TotalBytesRead));
		TotalBytesRead += BytesRead;
	}
	return TotalBytesRead;
}

u16 Verify (void *IicPtr, u16 Address, u8 *BufferPtr, u16 Length, u8 *Key, u8 Encrypted)
{
	u16 i;
	u8 Error;
	u8 ScratchBuffer[1024];
	u8 CipherBuffer[1024];

	if (Encrypted)
	{
		Get(IicPtr, Address, CipherBuffer, Round16(Length));
		Decrypt(CipherBuffer, ScratchBuffer, Key, Round16(Length));
	}

	else
	{
		Get(IicPtr, Address, ScratchBuffer, Length);
	}

	i = 0;
	Error = 0;
	do
	{
		if (BufferPtr[i] != ScratchBuffer[i])
			Error = 1;
		else
			i++;
	}
	while ((i<Length) && !Error);
	return i;
}

/*****************************************************************************/
/**
* This function writes a buffer of bytes to the IIC serial EEPROM.
*
* @param	Address contains the address in the EEPROM to write to.
* @param	BufferPtr contains the address of the data to write.
* @param	ByteCount contains the number of bytes in the buffer to be written.
*		Note that this should not exceed the page size of the EEPROM as
*		noted by the constant PAGE_SIZE.
*
* @return	The number of bytes written, a value less than that which was
*		specified as an input indicates an error.
*
* @note		None.
*
****************************************************************************/
unsigned EepromWriteByte(void *IicPtr, u16 Address, u8 *BufferPtr, u16 ByteCount)
{
#if !(defined(__arm__) || (__aarch64__)) || (defined XPS_BOARD_VEK280)
	XIic *Iic_Ptr = IicPtr;
#endif
	volatile unsigned SentByteCount;
	volatile unsigned AckByteCount;
	u8 WriteBuffer[sizeof(Address) + PAGE_SIZE];
	int Index;


	/*
	 * A temporary write buffer must be used which contains both the address
	 * and the data to be written, put the address in first based upon the
	 * size of the address for the EEPROM.
	 */
	WriteBuffer[0] = (u8)(Address >> 8);
	WriteBuffer[1] = (u8)(Address);

	/*
	 * Put the data in the write buffer following the address.
	 */
	for (Index = 0; Index < ByteCount; Index++) {
		WriteBuffer[sizeof(Address) + Index] = BufferPtr[Index];
	}

	/*
	 * Set the address register to the specified address by writing
	 * the address to the device, this must be tried until it succeeds
	 * because a previous write to the device could be pending and it
	 * will not ack until that write is complete.
	 */
	do {
		SentByteCount = XHdcp_I2cSend(IicPtr, EEPROM_ADDRESS,
								(u8 *)&Address, sizeof(Address),
								I2C_STOP);
		if (SentByteCount != sizeof(Address)) {

#if defined(__arm__) || (__aarch64__)  && (!defined XPS_BOARD_VEK280)
				XIicPs_Abort(IicPtr);
#else
				/* Send is aborted so reset Tx FIFO */
				XIic_WriteReg(Iic_Ptr->BaseAddress,
						XIIC_CR_REG_OFFSET,
						XIIC_CR_TX_FIFO_RESET_MASK);
				XIic_WriteReg(Iic_Ptr->BaseAddress,
						XIIC_CR_REG_OFFSET,
						XIIC_CR_ENABLE_DEVICE_MASK);
#endif
		}

	} while (SentByteCount != sizeof(Address));

	/*
	 * Write a page of data at the specified address to the EEPROM.
	 */
	SentByteCount = XHdcp_I2cSend(IicPtr, EEPROM_ADDRESS,
							WriteBuffer, sizeof(Address) + ByteCount,
							I2C_STOP);

	/*
	 * Wait for the write to be complete by trying to do a write and
	 * the device will not ack if the write is still active.
	 */
	do {
		AckByteCount = XHdcp_I2cSend(IicPtr, EEPROM_ADDRESS,
								(u8 *)&Address, sizeof(Address),
								I2C_STOP);
		if (AckByteCount != sizeof(Address)) {

#if defined(__arm__) || (__aarch64__)  && (!defined XPS_BOARD_VEK280)
				XIicPs_Abort(IicPtr);
#else
				/* Send is aborted so reset Tx FIFO */
				XIic_WriteReg(Iic_Ptr->BaseAddress,
						XIIC_CR_REG_OFFSET,
						XIIC_CR_TX_FIFO_RESET_MASK);
				XIic_WriteReg(Iic_Ptr->BaseAddress,
						XIIC_CR_REG_OFFSET,
						XIIC_CR_ENABLE_DEVICE_MASK);
#endif
		}

	} while (AckByteCount != sizeof(Address));


	/*
	 * Return the number of bytes written to the EEPROM
	 */
	return SentByteCount - sizeof(Address);
}

/*****************************************************************************/
/**
* This function reads a number of bytes from the IIC serial EEPROM into a
* specified buffer.
*
* @param	Address contains the address in the EEPROM to read from.
* @param	BufferPtr contains the address of the data buffer to be filled.
* @param	ByteCount contains the number of bytes in the buffer to be read.
*		This value is not constrained by the page size of the device
*		such that up to 64K may be read in one call.
*
* @return	The number of bytes read. A value less than the specified input
*		value indicates an error.
*
* @note		None.
*
****************************************************************************/
unsigned EepromReadByte(void *IicPtr, u16 Address, u8 *BufferPtr, u16 ByteCount)
{
#if !(defined(__arm__) || (__aarch64__)) || (defined XPS_BOARD_VEK280)
	XIic *Iic_Ptr = IicPtr;
#endif

	volatile unsigned ReceivedByteCount;
	u16 StatusReg;
	u8 WriteBuffer[sizeof(Address)];

	WriteBuffer[0] = (u8)(Address >> 8);
	WriteBuffer[1] = (u8)(Address);

	/*
	 * Set the address register to the specified address by writing
	 * the address to the device, this must be tried until it succeeds
	 * because a previous write to the device could be pending and it
	 * will not ack until that write is complete.
	 */
	do {
#if defined(__arm__) || (__aarch64__)  && (!defined XPS_BOARD_VEK280)
		if(!(XIicPs_BusIsBusy(IicPtr))) {
#else
		StatusReg = XIic_ReadReg(Iic_Ptr->BaseAddress, XIIC_SR_REG_OFFSET);
		if(!(StatusReg & XIIC_SR_BUS_BUSY_MASK)) {
#endif
			ReceivedByteCount = XHdcp_I2cSend(IicPtr, EEPROM_ADDRESS,
									(u8*)WriteBuffer, sizeof(Address),
									I2C_STOP);

			if (ReceivedByteCount != sizeof(Address)) {

#if defined(__arm__) || (__aarch64__)  && (!defined XPS_BOARD_VEK280)
				XIicPs_Abort(IicPtr);
#else
				/* Send is aborted so reset Tx FIFO */
				XIic_WriteReg(Iic_Ptr->BaseAddress,
						XIIC_CR_REG_OFFSET,
						XIIC_CR_TX_FIFO_RESET_MASK);
				XIic_WriteReg(Iic_Ptr->BaseAddress,
						XIIC_CR_REG_OFFSET,
						XIIC_CR_ENABLE_DEVICE_MASK);
#endif
			}
		}

	} while (ReceivedByteCount != sizeof(Address));

	/*
	 * Read the number of bytes at the specified address from the EEPROM.
	 */
	ReceivedByteCount = XHdcp_I2cRecv(IicPtr, EEPROM_ADDRESS, BufferPtr,
								ByteCount, I2C_STOP);

	/*
	 * Return the number of bytes read from the EEPROM.
	 */
	return ReceivedByteCount;
}
