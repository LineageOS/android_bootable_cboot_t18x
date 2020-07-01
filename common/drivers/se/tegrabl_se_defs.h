/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

#ifndef TEGRABL_SE_DEFS_H
#define TEGRABL_SE_DEFS_H

#define SE0_OP_STATUS_IDLE (NV_DRF_DEF(SE0_AES0, STATUS, STATE, IDLE))
#define SE0_OP_STATUS_BUSY (NV_DRF_DEF(SE0_AES0, STATUS, STATE, BUSY))
#define RSA_MAX_EXPONENT_SIZE_BITS 2048UL
#define AES_CMAC_CONST_RB 0x87U
#define NV_ICEIL(a, b) (((a) + (b) - 1U) / (b))
#define MAX_MGF_MASKLEN (256 - 20 - 1)
#define MAX_MGF_COUNTER (NV_ICEIL(MAX_MGF_MASKLEN, 20) - 1)
#define MAX_MGF_COUNTER_LOOPS (MAX_MGF_COUNTER + 1)
#define RSA_PSS_SALT_LENGTH_BITS 256
#define SE_SHA_RESULT_SIZE_SHA256 256
#define SE_SHA_RESULT_SIZE_SHA1 160
#define SE_AES_MAX_KEYSLOTS 16U
#define SE_RSA_MAX_KEYSLOTS 4U

#endif
