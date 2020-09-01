/*
 * Copyright (c) 2016-2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_CARVEOUT_ID_H
#define INCLUDED_CARVEOUT_ID_H

struct tegrabl_carveout_info {
	uint64_t base;
	uint64_t size;
};

typedef enum carve_out_type {
	CARVEOUT_NONE = 0,						/* 0x0 */
	CARVEOUT_NVDEC = 1,						/* 0x1 */
	CARVEOUT_WPR1 = 2,						/* 0x2 */
	CARVEOUT_WPR2 = 3,						/* 0x3 */
	CARVEOUT_TSECA = 4,						/* 0x4 */
	CARVEOUT_TSECB = 5,						/* 0x5 */
	CARVEOUT_BPMP = 6,						/* 0x6 */
	CARVEOUT_APE = 7,						/* 0x7 */
	CARVEOUT_SPE = 8,						/* 0x8 */
	CARVEOUT_SCE = 9,						/* 0x9 */
	CARVEOUT_APR = 10,						/* 0xA */
	CARVEOUT_TZRAM = 11,					/* 0xB */
	CARVEOUT_SE = 12,						/* 0xC */
	CARVEOUT_DMCE = 13,						/* 0xD */
	CARVEOUT_BPMP_TO_DMCE = 14,				/* 0xE */
	CARVEOUT_DMCE_TO_BPMP = 15,				/* 0xF */
	CARVEOUT_BPMP_TO_SPE = 16,				/* 0x10 */
	CARVEOUT_SPE_TO_BPMP = 17,				/* 0x11 */
	CARVEOUT_CPUTZ_TO_BPMP = 18,			/* 0x12 */
	CARVEOUT_BPMP_TO_CPUTZ = 19,			/* 0x13 */
	CARVEOUT_CPUNS_TO_BPMP = 20,			/* 0x14 */
	CARVEOUT_BPMP_TO_CPUNS = 21,			/* 0x15 */
	CARVEOUT_SE_SPE_SCE_BPMP = 22,			/* 0x16 */
	CARVEOUT_SC7_RESUME_FW = 23,			/* 0x17 */
	/* Refer to //hw/ar/doc/t18x/mc/T18x_gsc_programming.xlsx
	 for allocations of OEM_RSVD* carveouts */
	CARVEOUT_OEM_RSVD1 = 24,				/* 0x18 */
	CARVEOUT_OEM_RSVD2 = 25,				/* 0x19 */
	CARVEOUT_OEM_RSVD3 = 26,				/* 0x1A */
	/* Currently NV_RSVD1 carveout is unused. Reserved for future usecase */
	CARVEOUT_NV_RSVD1 = 27,					/* 0x1B */
	CARVEOUT_BO_MTS_PACKAGE = 28,			/* 0x1C */
	CARVEOUT_BO_MCE_PREBOOT = 29,			/* 0x1D */
	CARVEOUT_MAX_GSC_CO = 29,				/* 0x1D */
	CARVEOUT_MTS = 30,						/* 0x1E */
	CARVEOUT_VPR = 31,						/* 0x1F */
	CARVEOUT_TZDRAM = 32,					/* 0x20 */
	CARVEOUT_PRIMARY = 33,					/* 0x21 */
	CARVEOUT_EXTENDED = 34,					/* 0x22 */
	CARVEOUT_NCK = 35,						/* 0x23 */
	CARVEOUT_DEBUG = 36,					/* 0x24 */
	CARVEOUT_RAMDUMP = 37,					/* 0x25 */
	CARVEOUT_MB2 = 38,						/* 0x26 */
	CARVEOUT_CPUBL = 39,					/* 0x27 */
	CARVEOUT_MB2_HEAP = 40,					/* 0x28 */
	CARVEOUT_CPUBL_PARAMS = 41,				/* 0x29 */
	CARVEOUT_RESERVED1 = 42,				/* 0x2A */
	CARVEOUT_RESERVED2 = 43,				/* 0x2B */
	CARVEOUT_NUM = 44,						/* 0x2C */
	CARVEOUT_FORCE32 = 2147483647ULL		/* 0x7FFFFFFF */
} carve_out_type_t;

#endif
