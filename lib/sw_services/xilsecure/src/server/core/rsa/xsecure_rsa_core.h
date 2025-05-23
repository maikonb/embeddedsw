/******************************************************************************
* Copyright (c) 2019 - 2020 Xilinx, Inc.  All rights reserved.
* Copyright (c) 2022 - 2024 Advanced Micro Devices, Inc. All Rights Reserved.
* SPDX-License-Identifier: MIT
*******************************************************************************/

/*****************************************************************************/
/**
*
* @file xsecure_rsa_core.h
*
* This file contains Versal specific RSA core APIs.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date        Changes
* ----- ---- -------- -------------------------------------------------------
* 4.0   vns  03/09/19 Initial release
* 4.1   kpt  01/07/20 Added Macro's for Magic Numbers in
*                     xsecure_rsa_core.c
* 4.2   kpt  03/26/20 Added Error code XSECURE_RSA_ZEROIZE_ERROR
*       rpo  04/02/20 Added structure XSecure_RsaOps
*                     Added Crypto KAT APIs
*       har  04/06/20 Added function for selection of PKCS padding
* 4.3   har  06/17/20 Removed references to unused algorithms
*       am   09/24/20 Resolved MISRA C violations
*       har  10/12/20 Addressed security review comments
*       ana  10/15/20 Updated doxygen tags
* 4.6   har  07/14/21 Fixed doxygen warnings
*       gm   07/16/21 Added 64-bit address support
* 5.0   kpt  07/24/21 Moved XSecure_RsaPublicEncrypt KAT into xsecure_kat.c
* 5.2   kpt  08/20/23 Added prototype XSecure_RsaEcdsaZeroizeAndVerifyRam
* 5.4   yog  04/29/24 Fixed doxygen grouping and doxygen warnings.
*
* </pre>
*
******************************************************************************/
/**
* @addtogroup xsecure_rsa_server_apis XilSecure RSA Server APIs
* @{
*/
#ifndef XSECURE_RSA_CORE_H
#define XSECURE_RSA_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/
#include "xparameters.h"
#include "xsecure_utils.h"
/************************** Constant Definitions ****************************/
#if !defined(PLM_RSA_EXCLUDE) || !defined(PLM_ECDSA_EXCLUDE)
#define XSECURE_RSA_ECDSA_ZEROIZE_ERROR	(0x80U) /**< for RSA zeroization Error*/

#define XSECURE_RSA_MAX_BUFF		(6U) /**< RSA RAM Write Buffers */
#define XSECURE_RSA_MAX_RD_WR_CNT	(22U) /**< No of writes or reads to RSA RAM buffers */

#define XSECURE_RSA_RAM_RES_Q		(5U) /**< bit for RSA RAM Result(Q) */
#endif

#ifndef PLM_RSA_EXCLUDE
#define XSECURE_RSA_DATA_VALUE_ERROR	(0x2U) /**< for RSA private decryption
						* data should be lesser than
						* modulus */

#define XSECURE_HASH_TYPE_SHA3		(48U) /**< SHA-3 hash size */
#define XSECURE_FSBL_SIG_SIZE		(512U)/**< FSBL signature size */
/* Key size in bytes */
#define XSECURE_RSA_2048_KEY_SIZE	(2048U/8U) /**< RSA 2048 key size */
#define XSECURE_RSA_3072_KEY_SIZE	(3072U/8U) /**< RSA 3072 key size */
#define XSECURE_RSA_4096_KEY_SIZE	(4096U/8U) /**< RSA 4096 key size */

/* Key size in words */
#define XSECURE_RSA_2048_SIZE_WORDS	(64)	/**< RSA 2048 Size in words */
#define XSECURE_RSA_3072_SIZE_WORDS	(96)	/**< RSA 3072 Size in words */
#define XSECURE_RSA_4096_SIZE_WORDS	(128U)	/**< RSA 4096 Size in words */

#define XSECURE_RSA_RAM_EXPO		(0U) /**< bit for RSA RAM Exponent */
#define XSECURE_RSA_RAM_MOD		(1U) /**< bit for RSA RAM modulus */
#define XSECURE_RSA_RAM_DIGEST		(2U) /**< bit for RSA RAM Digest */
#define XSECURE_RSA_RAM_RES_Y		(4U) /**< bit for RSA RAM Result(Y) */

/** @name Control Register
 *
 * Control Register opcode definitions
 */
#define XSECURE_RSA_CONTROL_EXP		(0x01U) /**< Exponentiation Opcode */
#define XSECURE_RSA_CONTROL_EXP_PRE	(0x05U) /**< Expo. using R*R mod M */

/** @name Config registers values
 * CFG0 is for Qsel and multiplication passes
 * CFG1 is for Mont digits
 * CFG2 is for location size
 * CFG5 is for No.of groups
 */
#define XSECURE_ECDSA_RSA_CFG0_4096_VALUE	(0x0000006BU) /**< CFG0 4096 value*/
#define XSECURE_ECDSA_RSA_CFG1_4096_VALUE	(0x00000081U) /**< CFG1 4096 value*/
#define XSECURE_ECDSA_RSA_CFG2_4096_VALUE	(0x00000016U) /**< CFG2 4096 value*/
#define XSECURE_ECDSA_RSA_CFG5_4096_VALUE	(0x00000015U) /**< CFG3 4096 value*/

#define XSECURE_ECDSA_RSA_CFG0_3072_VALUE	(0x000000A0U) /**< CFG0 3072 value*/
#define XSECURE_ECDSA_RSA_CFG1_3072_VALUE	(0x00000061U) /**< CFG1 3072 value*/
#define XSECURE_ECDSA_RSA_CFG2_3072_VALUE	(0x00000016U) /**< CFG2 3072 value*/
#define XSECURE_ECDSA_RSA_CFG5_3072_VALUE	(0x00000010U) /**< CFG3 3072 value*/

#define XSECURE_ECDSA_RSA_CFG0_2048_VALUE	(0x00000016U) /**< CFG0 2048 value*/
#define XSECURE_ECDSA_RSA_CFG1_2048_VALUE	(0x00000041U) /**< CFG1 2048 value*/
#define XSECURE_ECDSA_RSA_CFG2_2048_VALUE	(0x00000016U) /**< CFG2 2048 value*/
#define XSECURE_ECDSA_RSA_CFG5_2048_VALUE	(0x0000000AU) /**< CFG3 2048 value*/

/** @name RSA status Register
 *
 * The Status Register(SR) indicates the current state of RSA device.
 *
 * Status Register Bit Definition
 */
#define XSECURE_RSA_STATUS_DONE		(0x1U) 	/**< Operation Done */
#define XSECURE_RSA_STATUS_ERROR	(0x4U) 	/**< Error */
/* @}*/

/** Used for setting the state of RSA operation. */
typedef enum {
	XSECURE_RSA_UNINITIALIZED = 0x0,/**< 0x0 */
	XSECURE_RSA_INITIALIZED			/**< 0x1 */
} XSecure_RsaState;

/** Used for selecting the RSA operation. */
typedef enum {
	XSECURE_RSA_SIGN_ENC = 0x0,		/**< 0x0 */
	XSECURE_RSA_SIGN_DEC			/**< 0x1 */
}XSecure_RsaOps;

/***************************** Type Definitions ******************************/
/**
 * The RSA driver instance data structure. A pointer to an instance data
 * structure is passed around by functions to refer to a specific driver
 * instance.
 */

typedef struct {
	UINTPTR BaseAddress; /**< Device Base Address */
	u64 ModAddr; /**< Modulus */
	u64 ModExtAddr; /**< Precalc. R sq. mod N */
	u64 ModExpoAddr; /**< Exponent */
	u8 EncDec; /**< 0 for signature verification and 1 for generation */
	u32 SizeInWords;/**< RSA key size in words */
	XSecure_RsaState RsaState;/**< RSA State */
} XSecure_Rsa;

/***************************** Function Prototypes ***************************/

/* Versal specific RSA core initialization function */
int XSecure_RsaCfgInitialize(XSecure_Rsa *InstancePtr);

/* Versal specific RSA core encryption/decryption function */
int XSecure_RsaOperation(XSecure_Rsa *InstancePtr, u64 Input,
	u64 Result, XSecure_RsaOps RsaOp, u32 KeySize);

/* Versal specific function for selection of PKCS padding */
u8* XSecure_RsaGetTPadding(void);
int XSecure_RsaZeroize(const XSecure_Rsa *InstancePtr);
#endif
#if !defined(PLM_RSA_EXCLUDE) || !defined(PLM_ECDSA_EXCLUDE)
int XSecure_RsaEcdsaZeroizeAndVerifyRam(u32 BaseAddress);
#endif

#ifdef __cplusplus
}
#endif

#endif /* XSECURE_RSA_CORE_H */
/** @} */
