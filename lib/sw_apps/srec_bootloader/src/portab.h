/******************************************************************************
*
* Copyright (C) 2004 - 2014 Xilinx, Inc.  All rights reserved.
* Copyright (C) 2023 - 2024 Advanced Micro Devices, Inc. All Rights Reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/
#ifndef BL_PORTAB_H
#define BL_PORTAB_H

#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int    uint32;

typedef char   int8;
typedef short  int16;
typedef int    int32;



/* An anonymous union allows the compiler to report typedef errors automatically */
/* Does not work with gcc. Might work only for g++ */

/* static union */
/* { */
/*     char int8_incorrect   [sizeof(  int8) == 1]; */
/*     char uint8_incorrect  [sizeof( uint8) == 1]; */
/*     char int16_incorrect  [sizeof( int16) == 2]; */
/*     char uint16_incorrect [sizeof(uint16) == 2]; */
/*     char int32_incorrect  [sizeof( int32) == 4]; */
/*     char uint32_incorrect [sizeof(uint32) == 4]; */
/* }; */


#ifdef __cplusplus
}
#endif
#endif /* BL_PORTTAB_H */
