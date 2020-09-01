/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_XUSBF

#include <tegrabl_io.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_timer.h>
#include <tegrabl_xusbf_soc.h>


void tegrabl_xusbf_soc_fpga_config(void)
{
	/* To Unplug- remove vbus */
	NV_WRITE32((NV_ADDRESS_MAP_XUSB_DEV_BASE + 0x8200), 0x24000);
	tegrabl_mdelay(1000);
	/* Override Vbus in device side. */
	NV_WRITE32((NV_ADDRESS_MAP_XUSB_DEV_BASE + 0x8200), 0x34000);
}

