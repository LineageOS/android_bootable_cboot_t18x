/*
 * Copyright (c) 2016, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE	TEGRABL_ERR_SOCMISC

#include "build_config.h"
#include <tegrabl_io.h>
#include <tegrabl_addressmap.h>
#include <arscratch.h>
#include <tegrabl_soc_misc.h>

void tegrabl_set_boot_slot_reg(uint32_t slot_info)
{
	NV_WRITE32(NV_ADDRESS_MAP_SCRATCH_BASE + SCRATCH_SCRATCH_99, slot_info);
}

uint32_t tegrabl_get_boot_slot_reg(void)
{
	return NV_READ32(NV_ADDRESS_MAP_SCRATCH_BASE + SCRATCH_SCRATCH_99);
}
