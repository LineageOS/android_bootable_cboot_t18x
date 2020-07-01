/*
 * Copyright (c) 2015-2016, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDE_TEGRABL_CLK_UFS_H
#define INCLUDE_TEGRABL_CLK_UFS_H

#include <tegrabl_debug.h>
#include <arclk_rst.h>
#include <tegrabl_drf.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_module.h>
#include <tegrabl_clk_rst_soc.h>
#include <arpmc_impl.h>

tegrabl_error_t tegrabl_ufs_clock_init(void);
void tegrabl_ufs_disable_device(void);
#endif
