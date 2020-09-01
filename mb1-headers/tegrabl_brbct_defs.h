/*
 * Copyright (c) 2018, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_BRBCT_DEFS_H
#define INCLUDED_BRBCT_DEFS_H

#if defined(__cplusplus)
extern "C"
{
#endif

#define NV_BCT_ONE_TIME_DATA_START (32U)
#define NV_BCT_ONE_TIME_DATA_SIZE  (144U)
#define NV_BCT_ONE_TIME_DATA_END   (NV_BCT_ONE_TIME_DATA_START + NV_BCT_ONE_TIME_DATA_SIZE)

#define T18X_PT_OFFSET                         1328U
#define T18X_ACTIVE_MARKER_OFFSET              1488U
#define T18X_GPIO_BOOT_CHAIN_SELECT_OFFSET     1496U
#define T18X_FORCE_INCOMPATIBLE_OFFSET         1500U
#define T18X_GPIO_PAD_CTRL_OFFSET              1504U
#define T18X_GPIO_CONFIG_ADDR_OFFSET           1508U
#define T18X_DISABLE_SWITCH_BOOTCHAIN_OFFSET   1512U

struct active_marker_info {
	uint32_t version;
	uint32_t value;
	uint32_t gpio_boot_chain_select;
	uint32_t force_abi_incompatible;
	uint32_t gpio_pad_ctrl;
	uint32_t gpio_config_addr;
	uint32_t disable_switch_bootchain;
};

#if defined(__cplusplus)
}
#endif

#endif /*  INCLUDED_BRBCT_DEFS_H */

