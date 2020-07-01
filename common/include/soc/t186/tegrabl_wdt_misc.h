/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

#ifndef TEGRABL_WDT_MISC_H
#define TEGRABL_WDT_MISC_H

/**
 * @brief WDT instance
 *
 * TEGRABL_WDT_BPMP corresponds to BPMP-WDT
 * TEGRABL_WDT_LCCPLEX corresponds to LCCPLEX_WDT
 */
enum tegrabl_wdt_instance {
	TEGRABL_WDT_BPMP,
	TEGRABL_WDT_LCCPLEX,
};

enum {
	TEGRABL_WDT_EXPIRY_1 = 0x1,
	TEGRABL_WDT_EXPIRY_2 = 0x2,
	TEGRABL_WDT_EXPIRY_3 = 0x4,
	TEGRABL_WDT_EXPIRY_4 = 0x8,
	TEGRABL_WDT_EXPIRY_5 = 0x10,
};

#endif
