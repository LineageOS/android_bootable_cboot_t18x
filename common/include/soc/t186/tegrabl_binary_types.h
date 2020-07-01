/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_BINARY_TYPES_H
#define INCLUDED_BINARY_TYPES_H

/**
 * @brief Defines various binaries which can be
 * loaded via loader.
 */
enum tegrabl_binary_type {
	TEGRABL_BINARY_MB1_BCT = 0,			/* 0x0  MB1-BCT */
	TEGRABL_BINARY_MTS_PREBOOT = 1,		/* 0x1  Preboot MTS binary */
	TEGRABL_BINARY_DMCE = 2,			/* 0x2  MTS DMCE binary */
	TEGRABL_BINARY_MTS = 3,				/* 0x3  MTS binary */
	TEGRABL_BINARY_EARLY_SPEFW = 4,		/* 0x4  SPE firmware */
	TEGRABL_BINARY_DRAM_ECC = 5,		/* 0x5  DRAM ECC */
	TEGRABL_BINARY_BLACKLIST_INFO = 6,	/* 0x6  Blacklist Info */
	TEGRABL_BINARY_EXTENDED_CAN = 7,	/* 0x7  TSEC firware */
	TEGRABL_BINARY_MB2 = 8,				/* 0x8  MB2 binary */
	TEGRABL_BINARY_FUSEBYPASS = 9,		/* 0x9  Fuse bypass */
	TEGRABL_BINARY_SC7_RESUME_FW = 10,	/* 0xA  SC7 resume fw
												(warmboot binary) */
	TEGRABL_BINARY_APE = 11,			/* 0xB  APE binary */
	TEGRABL_BINARY_SCE = 12,			/* 0xC  SCE binary */
	TEGRABL_BINARY_CPU_BL = 13,			/* 0xD  Tboot-CPU / CPU bootloader */
	TEGRABL_BINARY_TOS = 14,			/* 0xE  TLK image */
	TEGRABL_BINARY_EKS = 15,			/* 0xF  EKS image */
	TEGRABL_BPMP_FW = 16,				/* 0x10 BPMP Firmware */
	TEGRABL_BPMP_FW_DTB = 17,			/* 0x11 BPMP Firmware DTB */
	TEGRABL_BINARY_BR_BCT = 18,			/* 0x12 Bootrom BCT */
	TEGRABL_BINARY_SMD = 19,			/* 0x13 Slot Meta Data for A/B slots
												status */
	TEGRABL_BINARY_BL_DTB = 20,			/* 0x14 Bootloader DTB */
	TEGRABL_BINARY_KERNEL_DTB = 21,		/* 0x15 Kernel DTB */
	TEGRABL_BINARY_RPB = 22,			/* 0x16 Rollback Prevention Bypass
												token */
	TEGRABL_BINARY_MB2_RAMDUMP = 23,	/* 0x17  MB2 binary for ramdump */
	TEGRABL_BINARY_MAX = 24				/* 0x18 */
};

/**
 * @brief Binary identifier to indicate which copy of the binary needs
 * to be loaded
 */
enum tegrabl_binary_copy {
	TEGRABL_BINARY_COPY_PRIMARY,
	TEGRABL_BINARY_COPY_RECOVERY,
	TEGRABL_BINARY_COPY_MAX
};

#endif
