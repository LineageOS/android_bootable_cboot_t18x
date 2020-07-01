/*
 * Copyright (c) 2016-2019, NVIDIA Corporation.  All rights reserved.
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
/* macro tegrabl binary type */
typedef uint32_t tegrabl_binary_type_t;
#define TEGRABL_BINARY_MB1_BCT 0U			/* 0x0  MB1-BCT */
#define TEGRABL_BINARY_MTS_PREBOOT 1U		/* 0x1  Preboot MTS binary */
#define TEGRABL_BINARY_DMCE 2U			/* 0x2  MTS DMCE binary */
#define TEGRABL_BINARY_MTS 3U				/* 0x3  MTS binary */
#define TEGRABL_BINARY_EARLY_SPEFW 4U		/* 0x4  SPE firmware */
#define TEGRABL_BINARY_DRAM_ECC 5U		/* 0x5  DRAM ECC */
#define TEGRABL_BINARY_BLACKLIST_INFO 6U	/* 0x6  Blacklist Info */
#define TEGRABL_BINARY_EXTENDED_CAN 7U	/* 0x7  TSEC firware */
#define TEGRABL_BINARY_MB2 8U				/* 0x8  MB2 binary */
#define TEGRABL_BINARY_FUSEBYPASS 9U		/* 0x9  Fuse bypass */
#define TEGRABL_BINARY_SC7_RESUME_FW 10U	/* 0xA  SC7 resume fw
												(warmboot binary) */
#define TEGRABL_BINARY_APE 11U			/* 0xB  APE binary */
#define TEGRABL_BINARY_SCE 12U			/* 0xC  SCE binary */
#define TEGRABL_BINARY_CPU_BL 13U			/* 0xD  Tboot-CPU / CPU bootloader */
#define TEGRABL_BINARY_TOS 14U			/* 0xE  TLK image */
#define TEGRABL_BINARY_EKS 15U			/* 0xF  EKS image */
#define TEGRABL_BINARY_BPMP_FW 16U				/* 0x10 BPMP Firmware */
#define TEGRABL_BINARY_BPMP_FW_DTB 17U			/* 0x11 BPMP Firmware DTB */
#define TEGRABL_BINARY_BR_BCT 18U			/* 0x12 Bootrom BCT */
#define TEGRABL_BINARY_SMD 19U			/* 0x13 Slot Meta Data for A/B slots
												status */
#define TEGRABL_BINARY_BL_DTB 20U			/* 0x14 Bootloader DTB */
#define TEGRABL_BINARY_KERNEL_DTB 21U		/* 0x15 Kernel DTB */
#define TEGRABL_BINARY_RPB 22U			/* 0x16 Rollback Prevention Bypass
												token */
#define TEGRABL_BINARY_MB2_RAMDUMP 23U	/* 0x17  MB2 binary for ramdump */
#define TEGRABL_BINARY_KERNEL 24U		/* 0x18 */
#define TEGRABL_BINARY_RECOVERY_KERNEL 25U	/* 0x19 */
#define TEGRABL_BINARY_NCT 26U		/* 0x1a */
#define TEGRABL_BINARY_KERNEL_DTBO 27U	/* 0x1b */
#if defined(CONFIG_ENABLE_L4T_RECOVERY)
#define TEGRABL_BINARY_RECOVERY_IMG 28U /* 0x1c */
#define TEGRABL_BINARY_RECOVERY_DTB 29U /* 0x1d */
#define TEGRABL_BINARY_KERNEL_BOOTCTRL 30U      /* 0x1e */
#define TEGRABL_BINARY_MAX 31U				/* 0x1f */
#else
#define TEGRABL_BINARY_MAX 28U				/* 0x1c */
#endif

/**
 * @brief Binary identifier to indicate which copy of the binary needs
 * to be loaded
 */
/* macro tegrabl binary copy */
typedef uint32_t tegrabl_binary_copy_t;
#define TEGRABL_BINARY_COPY_PRIMARY 0U
#define TEGRABL_BINARY_COPY_RECOVERY 1U
#define TEGRABL_BINARY_COPY_MAX 2U

#endif
