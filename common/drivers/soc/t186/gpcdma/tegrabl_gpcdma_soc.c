/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */


#include <stdint.h>
#include <tegrabl_gpcdma_soc_common.h>

static struct gpcdma_soc_info gpcdma_info[] = {
	{
		.io_dma_mc_burst_size   = 2U,
		.io_dma_mmio_burst_size = 8U,
	}
};

void gpcdma_get_soc_info(struct gpcdma_soc_info **ggpcdma_info)
{
	*ggpcdma_info = &gpcdma_info[0];
}
