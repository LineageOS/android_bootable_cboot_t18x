/*
 * Copyright (c) 2015-2017 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_SIGHEADER_H
#define INCLUDED_TEGRABL_SIGHEADER_H

#include <stdint.h>

#define AES_BLOCK_LEN 16U
#define RSA_SIG_LEN 256
#define RESERVED_SIZE 2
#define SIGNED_SECTION_LEN 16U
#define HEADER_SIZE 400U

struct tegrabl_ecdsasig {
	uint8_t r[48];
	uint8_t s[48];
};

struct tegrabl_cryptosignature {
	uint8_t cryptohash[AES_BLOCK_LEN];
	uint8_t rsapsssig[RSA_SIG_LEN];
	struct tegrabl_ecdsasig ecdsasig;
};

/* macro tegrabl signingtype */
typedef uint32_t tegrabl_signingtype_t;
#define TEGRABL_SIGNINGTYPE_NONE 0
#define TEGRABL_SIGNINGTYPE_OEM_RSA 1
#define TEGRABL_SIGNINGTYPE_OEM_ECC 2
#define TEGRABL_SIGNINGTYPE_NVIDIA_RSA 3
#define TEGRABL_SIGNINGTYPE_NVIDIA_ECC 4 /* this entry is never valid */
#define TEGRABL_SIGNINGTYPE_SBK 5
	/* FIXME: Remove ZERO_SBK once qb tools are updated to use new enum */
#define TEGRABL_SIGNINGTYPE_ZERO_SBK TEGRABL_SIGNINGTYPE_SBK
#define TEGRABL_SIGNINGTYPE_OEM_RSA_SBK 6
#define TEGRABL_SIGNINGTYPE_FORCE 0xFFFFFFFFU

struct tegrabl_sigheader {
	/***** Unsigned section starts (384, 0x180 bytes)*****/
	uint8_t headermagic[4];
	/* 4 bytes, considering flexibility for future extension */
	uint32_t headerversion;
	struct tegrabl_cryptosignature signatures;
	/* reserved bytes for future use */
	uint8_t padding[8];
	/***** Signed section starts (16, 0x10 byte)*****/
	uint8_t binarymagic[4];/* 4 bytes, unique identifier for binary */
	tegrabl_signingtype_t signtype;
	uint32_t binarylength;
	uint16_t fw_ratchet_level;
	uint8_t reserved[RESERVED_SIZE];
};

#endif

