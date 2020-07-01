/*
 * Copyright (c) 2016-2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_PKC_OP

#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_brbct.h>
#include <string.h>

#define PKC_MODULUS_OFFSET 12
#define PKC_MODULUS_SIZE 256

tegrabl_error_t tegrabl_pkc_modulus_get(uint8_t *modulus)
{
	uintptr_t brbct_address;

	if (!modulus) {
		pr_trace("brbct address is NULL!\n");
		return TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 0);
	}

	brbct_address = tegrabl_brbct_get();
	if (brbct_address == 0) {
		pr_trace("brbct is not initialised!\n");
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 0);
	}

	/* BRBCT data get API has not been supported yet, read data directly as
	 * odmdata lib does
	 * TODO:
	 * Use br-bct structure and avoid this macro
	 * Replace this with tegrabl_brbct_data_get API
	 * Tracked in Bug 200202646
	 */
	memcpy((void *)modulus, (const void *)(brbct_address + PKC_MODULUS_OFFSET),
		   PKC_MODULUS_SIZE);

	return TEGRABL_NO_ERROR;
}
