/*
 * Copyright (c) 2015, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <stdint.h>
#include <tegrabl_io.h>
#include <tegrabl_drf.h>
#include <tegrabl_addressmap.h>
#include <artsc.h>
#include <artsc_sysctr0.h>

void nvtboot_cpu_tzinit(void);
void nvtboot_cpu_tzinit(void)
{
	uint32_t val;

	/* Enable TSC (copied from boot_wrapper) */
	val = NV_READ32(NV_ADDRESS_MAP_SYSCTR0_BASE + TSC_SYSCTR0_CNTCR_0);
	val |= NV_DRF_DEF(TSC_SYSCTR0, CNTCR, EN, ENABLE);
	NV_WRITE32(NV_ADDRESS_MAP_SYSCTR0_BASE + TSC_SYSCTR0_CNTCR_0, val);

	val = NV_READ32(NV_ADDRESS_MAP_TSC_IMPL_BASE + TSC_TSCDCR_0);
	val |= NV_DRF_NUM(TSC, TSCDCR, SYNC, 1);
	val |= NV_DRF_DEF(TSC, TSCDCR, EN, ENABLE);
	NV_WRITE32(NV_ADDRESS_MAP_TSC_IMPL_BASE + TSC_TSCDCR_0, val);
}

