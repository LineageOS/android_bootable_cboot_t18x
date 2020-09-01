/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_DPAUX

#include <tegrabl_dpaux_soc.h>
#include <tegrabl_dpaux_soc_common.h>
#include <tegrabl_module.h>
#include <tegrabl_addressmap.h>

static struct dpaux_soc_info dpaux_info[DPAUX_MAX] = {
	{
		.base_addr = NV_ADDRESS_MAP_DPAUX_BASE,
		.module = TEGRABL_MODULE_DPAUX,
	},
	{
		.base_addr = NV_ADDRESS_MAP_DPAUX1_BASE,
		.module = TEGRABL_MODULE_DPAUX1,
	},
};

void dpaux_get_soc_info(struct dpaux_soc_info **hdpaux_info)
{
	*hdpaux_info = &dpaux_info[0];
}

