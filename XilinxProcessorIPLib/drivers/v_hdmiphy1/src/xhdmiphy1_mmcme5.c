/*******************************************************************************
* Copyright (C) 2015 - 2020 Xilinx, Inc.  All rights reserved.
* Copyright (C) 2022 - 2024 Advanced Micro Devices, Inc. All Rights Reserved.
* SPDX-License-Identifier: MIT
*******************************************************************************/

/******************************************************************************/
/**
 *
 * @file xhdmiphy1_mmcme5.c
 *
 * Contains a minimal set of functions for the XHdmiphy1 driver that allow
 * access to all of the Video PHY core's functionality. See xhdmiphy1.h for a
 * detailed description of the driver.
 *
 * @note	None.
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date     Changes
 * ----- ---- -------- -----------------------------------------------
 *            dd/mm/yy
 * ----- ---- -------- -----------------------------------------------
 * 1.0   gm   10/12/18 Initial release.
 * 1.1   ssh  07/26/21 Added definitions for registers and masks
 * 1.2   ku   07/01/22 Added Support for lower clocks
 *
 * </pre>
 *
*******************************************************************************/

/******************************* Include Files ********************************/

#include "xparameters.h"
#include "xstatus.h"
#include "xhdmiphy1.h"
#include "xhdmiphy1_i.h"
#if (XPAR_HDMIPHY_SS_0_HDMI_GT_CONTROLLER_TX_CLK_PRIMITIVE == 0 || \
		XPAR_HDMIPHY_SS_0_HDMI_GT_CONTROLLER_RX_CLK_PRIMITIVE == 0)
#if ((XPAR_HDMIPHY1_0_TRANSCEIVER == XHDMIPHY1_GTYE5) || \
     (XPAR_HDMIPHY1_0_TRANSCEIVER == XHDMIPHY1_GTYP))

#define XHDMIPHY1_MMCM_DRP_CLKFBOUT_1_REG 0x0C
#define XHDMIPHY1_MMCM_DRP_CLKFBOUT_2_REG 0x0D
#define XHDMIPHY1_MMCM_DRP_DIVCLK_DIVIDE_REG 0x21
#define XHDMIPHY1_MMCM_DRP_DESKEW_REG 0x20
#define XHDMIPHY1_MMCM_DRP_CLKOUT0_REG1 0x0E
#define XHDMIPHY1_MMCM_DRP_CLKOUT0_REG2 0x0F
#define XHDMIPHY1_MMCM_DRP_CLKOUT1_REG1 0x10
#define XHDMIPHY1_MMCM_DRP_CLKOUT1_REG2 0x11
#define XHDMIPHY1_MMCM_DRP_CLKOUT2_REG1 0x12
#define XHDMIPHY1_MMCM_DRP_CLKOUT2_REG2 0x13
#define XHDMIPHY1_MMCM_DRP_CP_REG1 0x1E
#define XHDMIPHY1_MMCM_DRP_RES_REG1 0x2A
#define XHDMIPHY1_MMCM_DRP_LOCK_REG1 0x27
#define XHDMIPHY1_MMCM_DRP_LOCK_REG2 0x28

#define XHDMIPHY1_MMCM_WRITE_VAL	0xFFFF
#define XHDMIPHY1_MMCM_CP_RES_MASK	0xF
#define XHDMIPHY1_MMCM_RES_MASK		0x1E
#define XHDMIPHY1_MMCM_LOCK1_MASK1	0x8000
#define XHDMIPHY1_MMCM_LOCK1_MASK2	0x7FFF
#define XHDMIPHY1_MMCM_DRP_DESKEW_REG_VAL1 0x0400
#define XHDMIPHY1_MMCM_DRP_DESKEW_REG_VAL2 0x0000

/**************************** Function Prototypes *****************************/
static u32 XHdmiphy1_Mmcme5DividerEncoding(XHdmiphy1_MmcmDivType DivType,
		u16 Div);
static u32 XHdmiphy1_Mmcme5CpResEncoding(u16 Mult);
static u32 XHdmiphy1_Mmcme5LockReg1Reg2Encoding(u16 Mult);

/************************** Constant Definitions ******************************/

/*****************************************************************************/
/**
* This function returns the DRP encoding of ClkFbOutMult optimized for:
* Phase = 0; Dutycycle = 0.5; No Fractional division
* The calculations are based on XAPP888
*
* @param	Div is the divider to be encoded
*
* @return
*		- Encoded Value for ClkReg1 [15: 0]
*       - Encoded Value for ClkReg2 [31:16]
*
* @note		None.
*
******************************************************************************/
u32 XHdmiphy1_Mmcme5DividerEncoding(XHdmiphy1_MmcmDivType DivType, u16 Div)
{
	u32 DrpEnc;
	u32 ClkReg1;
    u32 ClkReg2;
    u8 HiTime, LoTime;
    u16 Divide = Div;

    if (DivType == XHDMIPHY1_MMCM_CLKOUT_DIVIDE) {
	/* Div is an odd number */
		if (Div % 2) {
		Divide = (Div / 2);
		}
		/* Div is an even number */
		else {
		Divide = (Div / 2) + (Div % 2);
		}
    }

	HiTime = Divide / 2;
	LoTime = HiTime;

	ClkReg2 = LoTime & 0xFF;
	ClkReg2 |= (HiTime & 0xFF) << 8;

	if (DivType == XHDMIPHY1_MMCM_CLKFBOUT_MULT_F) {
		ClkReg1 = (Divide % 2) ? 0x00001700 : 0x00001600;
	}
	else {
		/* Div is an odd number */
		if (Div % 2) {
			ClkReg1 = (Divide % 2) ? 0x0000BB00 : 0x0000BA00;
		}
		/* Div is an even number */
		else {
			ClkReg1 = (Divide % 2) ? 0x00001B00 : 0x00001A00;
		}
	}

    DrpEnc = (ClkReg2 << 16) | ClkReg1;

	return DrpEnc;
}

/*****************************************************************************/
/**
* This function returns the DRP encoding of CP and Res optimized for:
* Phase = 0; Dutycycle = 0.5; BW = low; No Fractional division
*
* @param	Mult is the divider to be encoded
*
* @return
*		- [3:0]   CP
*		- [20:17] RES
*
* @note		None.
*
******************************************************************************/
u32 XHdmiphy1_Mmcme5CpResEncoding(u16 Mult)
{
	u32 DrpEnc;
	u16 cp;
	u16 res;

    switch (Mult) {
    case 4:
         cp = 5; res = 15;
         break;
    case 5:
         cp = 6; res = 15;
         break;
    case 6:
         cp = 7; res = 15;
         break;
    case 7:
         cp = 13; res = 15;
         break;
    case 8:
         cp = 14; res = 15;
         break;
    case 9:
         cp = 15; res = 15;
         break;
    case 10:
         cp = 14; res = 7;
         break;
    case 11:
         cp = 15; res = 7;
         break;
    case 12 ... 13:
         cp = 15; res = 11;
         break;
    case 14:
         cp = 15; res = 13;
         break;
    case 15:
         cp = 15; res = 3;
         break;
    case 16 ... 17:
         cp = 14; res = 5;
         break;
    case 18 ... 19:
         cp = 15; res = 5;
         break;
    case 20 ... 21:
         cp = 15; res = 9;
         break;
    case 22 ... 23:
         cp = 14; res = 14;
         break;
    case 24 ... 26:
         cp = 15; res = 14;
         break;
    case 27 ... 28:
         cp = 14; res = 1;
         break;
    case 29 ... 33:
         cp = 15; res = 1;
         break;
    case 34 ... 37:
         cp = 14; res = 6;
         break;
    case 38 ... 44:
         cp = 15; res = 6;
         break;
    case 45 ... 57:
         cp = 15; res = 10;
         break;
    case 58 ... 63:
         cp = 13; res = 12;
         break;
    case 64 ... 70:
         cp = 14; res = 12;
         break;
    case 71 ... 86:
         cp = 15; res = 12;
         break;
    case 87 ... 93:
         cp = 14; res = 2;
         break;
    case 94:
         cp = 5; res = 15;
         break;
    case 95:
         cp = 6; res = 15;
         break;
    case 96:
         cp = 7; res = 15;
         break;
    case 97:
         cp = 13; res = 15;
         break;
    case 98:
         cp = 14; res = 15;
         break;
    case 99:
         cp = 15; res = 15;
         break;
    case 100:
         cp = 14; res = 7;
         break;
    case 101:
         cp = 15; res = 7;
         break;
    case 102 ... 103:
         cp = 15; res = 11;
         break;
    case 104:
         cp = 15; res = 13;
         break;
    case 105:
         cp = 15; res = 3;
         break;
    case 106 ... 107:
         cp = 14; res = 5;
         break;
    case 108 ... 109:
         cp = 15; res = 5;
         break;
    case 110 ... 111:
         cp = 15; res = 9;
         break;
    case 112 ... 113:
         cp = 14; res = 14;
         break;
    case 114 ... 116:
         cp = 15; res = 14;
         break;
    case 117 ... 118:
         cp = 14; res = 1;
         break;
    case 119 ... 123:
         cp = 15; res = 1;
         break;
    case 124 ... 127:
         cp = 14; res = 6;
         break;
    case 128 ... 134:
         cp = 15; res = 6;
         break;
    case 135 ... 147:
         cp = 15; res = 10;
         break;
    case 148 ... 153:
         cp = 13; res = 12;
         break;
    case 154 ... 160:
         cp = 14; res = 12;
         break;
    case 161 ... 176:
         cp = 15; res = 12;
         break;
    case 177 ... 183:
         cp = 14; res = 2;
         break;
    case 184 ... 200:
         cp = 14; res = 4;
         break;
    case 201 ... 273:
         cp = 15; res = 4;
         break;
    case 274 ... 300:
         cp = 13; res = 8;
         break;
    case 301 ... 325:
         cp = 14; res = 8;
         break;
    case 326 ... 432:
         cp = 15; res = 8;
         break;
    case 433 ... 512:
         cp = 15; res = 8;
         break;
    default:
         cp = 13; res = 8;
         break;
	}

    /* Construct the return value */
    DrpEnc = ((res & 0xf) << 17) | ((cp & 0xf) | 0x160);

	return DrpEnc;
}

/*****************************************************************************/
/**
* This function returns the DRP encoding of Lock Reg1 & Reg2 optimized for:
* Phase = 0; Dutycycle = 0.5; BW = low; No Fractional division
*
* @param	Mult is the divider to be encoded
*
* @return
*		- [15:0]  Lock_1 Reg
*		- [31:16] Lock_2 Reg
*
* @note		None.
*
******************************************************************************/
u32 XHdmiphy1_Mmcme5LockReg1Reg2Encoding(u16 Mult)
{
	u32 DrpEnc;
	u16 Lock_1;
	u16 Lock_2;
	u16 lock_ref_dly;
	u16 lock_fb_dly;
	u16 lock_cnt;
	u16 lock_sat_high = 9;

	switch (Mult) {
		case 4:
			lock_ref_dly = 4;
			lock_fb_dly = 4;
			lock_cnt = 1000;
			break;
		case 5:
			lock_ref_dly = 6;
			lock_fb_dly = 6;
			lock_cnt = 1000;
			break;
		case 6 ... 7:
			lock_ref_dly = 7;
			lock_fb_dly = 7;
			lock_cnt = 1000;
			break;
		case 8:
			lock_ref_dly = 9;
			lock_fb_dly = 9;
			lock_cnt = 1000;
			break;
		case 9 ... 10:
			lock_ref_dly = 10;
			lock_fb_dly = 10;
			lock_cnt = 1000;
			break;
		case 11:
			lock_ref_dly = 11;
			lock_fb_dly = 11;
			lock_cnt = 1000;
			break;
		case 12:
			lock_ref_dly = 13;
			lock_fb_dly = 13;
			lock_cnt = 1000;
			break;
		case 13 ... 14:
			lock_ref_dly = 14;
			lock_fb_dly = 14;
			lock_cnt = 1000;
			break;
		case 15:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 900;
			break;
		case 16 ... 17:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 825;
			break;
		case 18:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 750;
			break;
		case 19 ... 20:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 700;
			break;
		case 21:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 650;
			break;
		case 22 ... 23:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 625;
			break;
		case 24:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 575;
			break;
		case 25:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 550;
			break;
		case 26 ... 28:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 525;
			break;
		case 29 ... 30:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 475;
			break;
		case 31:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 450;
			break;
		case 32 ... 33:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 425;
			break;
		case 34 ... 36:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 400;
			break;
		case 37:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 375;
			break;
		case 38 ... 40:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 350;
			break;
		case 41 ... 43:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 325;
			break;
		case 44 ... 47:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 300;
			break;
		case 48 ... 51:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 275;
			break;
		case 52 ... 205:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 950;
			break;
		case 206 ... 432:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 925;
			break;
		case 433 ... 512:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 925;
			break;

		default:
			lock_ref_dly = 16;
			lock_fb_dly = 16;
			lock_cnt = 250;
			break;
	}

	/* Construct Lock_1 Reg */
	Lock_1 = ((lock_fb_dly & 0x1F) << 10) | (lock_cnt & 0x3FF);

	/* Construct Lock_2 Reg */
	Lock_2 = ((lock_ref_dly & 0x1F) << 10) | (lock_sat_high & 0x3FF);

	/* Construct Return Value */
	DrpEnc = (Lock_2 << 16) | Lock_1;

	return DrpEnc;
}

/*****************************************************************************/
/**
* This function will write the mixed-mode clock manager (MMCM) values currently
* stored in the driver's instance structure to hardware .
*
* @param	InstancePtr is a pointer to the XHdmiphy1 core instance.
* @param	QuadId is the GT quad ID to operate on.
* @param	Dir is an indicator for TX or RX.
*
* @return
*		- XST_SUCCESS if the MMCM write was successful.
*		- XST_FAILURE otherwise, if the configuration success bit did
*		  not go low.
*
* @note		None.
*
******************************************************************************/
u32 XHdmiphy1_MmcmWriteParameters(XHdmiphy1 *InstancePtr, u8 QuadId,
							XHdmiphy1_DirectionType Dir)
{
	u8 ChId;
	u32 DrpVal32;
	u16 DrpRdVal;
	XHdmiphy1_Mmcm *MmcmParams;

    ChId = (Dir == XHDMIPHY1_DIR_TX) ?
						XHDMIPHY1_CHANNEL_ID_TXMMCM :
						XHDMIPHY1_CHANNEL_ID_RXMMCM;

	MmcmParams = &InstancePtr->Quads[QuadId].Mmcm[Dir];

	/* Check Parameters if has been Initialized */
	if (!MmcmParams->DivClkDivide && !MmcmParams->ClkFbOutMult &&
			!MmcmParams->ClkOut0Div && !MmcmParams->ClkOut1Div &&
			!MmcmParams->ClkOut2Div) {
		return XST_FAILURE;
	}
	/* Write CLKFBOUT_1 & CLKFBOUT_2 Values */
	DrpVal32 = XHdmiphy1_Mmcme5DividerEncoding(XHDMIPHY1_MMCM_CLKFBOUT_MULT_F,
						MmcmParams->ClkFbOutMult);
	XHdmiphy1_DrpWr(InstancePtr, QuadId, ChId, XHDMIPHY1_MMCM_DRP_CLKFBOUT_1_REG,
						(u16)(DrpVal32 & XHDMIPHY1_MMCM_WRITE_VAL));
	XHdmiphy1_DrpWr(InstancePtr, QuadId, ChId, XHDMIPHY1_MMCM_DRP_CLKFBOUT_2_REG,
						(u16)((DrpVal32 >> 16) & XHDMIPHY1_MMCM_WRITE_VAL));

	/* Write DIVCLK_DIVIDE & DESKEW_2 Values */
	DrpVal32 = XHdmiphy1_Mmcme5DividerEncoding(XHDMIPHY1_MMCM_DIVCLK_DIVIDE,
						MmcmParams->DivClkDivide) ;
	XHdmiphy1_DrpWr(InstancePtr, QuadId, ChId, XHDMIPHY1_MMCM_DRP_DIVCLK_DIVIDE_REG,
						(u16)((DrpVal32 >> 16) & XHDMIPHY1_MMCM_WRITE_VAL));
	XHdmiphy1_DrpWr(InstancePtr, QuadId, ChId, XHDMIPHY1_MMCM_DRP_DESKEW_REG,
						((MmcmParams->DivClkDivide == 0) ? 0x0000 :
								((MmcmParams->DivClkDivide % 2) ?
								XHDMIPHY1_MMCM_DRP_DESKEW_REG_VAL1 : XHDMIPHY1_MMCM_DRP_DESKEW_REG_VAL2)));
	/* Write CLKOUT0_1 & CLKOUT0_2 Values */
	DrpVal32 = XHdmiphy1_Mmcme5DividerEncoding(XHDMIPHY1_MMCM_CLKOUT_DIVIDE,
						MmcmParams->ClkOut0Div);
	XHdmiphy1_DrpWr(InstancePtr, QuadId, ChId, XHDMIPHY1_MMCM_DRP_CLKOUT0_REG1,
						(u16)(DrpVal32 & XHDMIPHY1_MMCM_WRITE_VAL));
	XHdmiphy1_DrpWr(InstancePtr, QuadId, ChId, XHDMIPHY1_MMCM_DRP_CLKOUT0_REG2,
						(u16)((DrpVal32 >> 16) & XHDMIPHY1_MMCM_WRITE_VAL));

	/* Write CLKOUT1_1 & CLKOUT1_2 Values */
	DrpVal32 = XHdmiphy1_Mmcme5DividerEncoding(XHDMIPHY1_MMCM_CLKOUT_DIVIDE,
						MmcmParams->ClkOut1Div);
	XHdmiphy1_DrpWr(InstancePtr, QuadId, ChId, XHDMIPHY1_MMCM_DRP_CLKOUT1_REG1,
						(u16)(DrpVal32 & XHDMIPHY1_MMCM_WRITE_VAL));
	XHdmiphy1_DrpWr(InstancePtr, QuadId, ChId, XHDMIPHY1_MMCM_DRP_CLKOUT1_REG2,
						(u16)((DrpVal32 >> 16) & XHDMIPHY1_MMCM_WRITE_VAL));

	/* Write CLKOUT2_1 & CLKOUT2_2 Values */
	DrpVal32 = XHdmiphy1_Mmcme5DividerEncoding(XHDMIPHY1_MMCM_CLKOUT_DIVIDE,
						MmcmParams->ClkOut2Div);
	XHdmiphy1_DrpWr(InstancePtr, QuadId, ChId, XHDMIPHY1_MMCM_DRP_CLKOUT2_REG1,
						(u16)(DrpVal32 & XHDMIPHY1_MMCM_WRITE_VAL));
	XHdmiphy1_DrpWr(InstancePtr, QuadId, ChId, XHDMIPHY1_MMCM_DRP_CLKOUT2_REG2,
						(u16)((DrpVal32 >> 16) & XHDMIPHY1_MMCM_WRITE_VAL));
	/* Write CP & RES Values */
	DrpVal32 = XHdmiphy1_Mmcme5CpResEncoding(MmcmParams->ClkFbOutMult);
	/* CP */
	DrpRdVal = XHdmiphy1_DrpRd(InstancePtr, QuadId, ChId, XHDMIPHY1_MMCM_DRP_CP_REG1, &DrpRdVal);
	DrpRdVal &= ~(XHDMIPHY1_MMCM_CP_RES_MASK);

	XHdmiphy1_DrpWr(InstancePtr, QuadId, ChId, XHDMIPHY1_MMCM_DRP_CP_REG1,
						(u16)((DrpVal32 & XHDMIPHY1_MMCM_CP_RES_MASK) | DrpRdVal));

	/* RES */
	DrpRdVal = XHdmiphy1_DrpRd(InstancePtr, QuadId, ChId, XHDMIPHY1_MMCM_DRP_RES_REG1, &DrpRdVal);
	DrpRdVal &= ~(XHDMIPHY1_MMCM_RES_MASK);
	XHdmiphy1_DrpWr(InstancePtr, QuadId, ChId, XHDMIPHY1_MMCM_DRP_RES_REG1,
						(u16)(((DrpVal32 >> 15) & XHDMIPHY1_MMCM_RES_MASK) | DrpRdVal));

	/* Write Lock Reg1 & Reg2 Values */
	DrpVal32 = XHdmiphy1_Mmcme5LockReg1Reg2Encoding(MmcmParams->ClkFbOutMult);
	/* LOCK_1 */
	DrpRdVal = XHdmiphy1_DrpRd(InstancePtr, QuadId, ChId, XHDMIPHY1_MMCM_DRP_LOCK_REG1, &DrpRdVal);
	DrpRdVal &= ~(XHDMIPHY1_MMCM_LOCK1_MASK1);
	XHdmiphy1_DrpWr(InstancePtr, QuadId, ChId, XHDMIPHY1_MMCM_DRP_LOCK_REG1,
						(u16)((DrpVal32 & XHDMIPHY1_MMCM_LOCK1_MASK2) | DrpRdVal));

	/* LOCK_2 */
	DrpRdVal = XHdmiphy1_DrpRd(InstancePtr, QuadId, ChId, XHDMIPHY1_MMCM_DRP_LOCK_REG2, &DrpRdVal);
	DrpRdVal &= ~(XHDMIPHY1_MMCM_LOCK1_MASK1);
	XHdmiphy1_DrpWr(InstancePtr, QuadId, ChId, XHDMIPHY1_MMCM_DRP_LOCK_REG2,
						(u16)(((DrpVal32 >> 16) & XHDMIPHY1_MMCM_LOCK1_MASK2) | DrpRdVal));

	return XST_SUCCESS;
}

#endif
#endif
