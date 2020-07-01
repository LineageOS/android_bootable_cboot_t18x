/*
 * TCA9539 16-bit I2C I/O Expander Driver
 *
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef _TEGRABL_TCA_PLAT_CONFIG_H_
#define _TEGRABL_TCA_PLAT_CONFIG_H_

#define I2C1_NODE	"/i2c@3160000"

static struct tca9539_driver_property tca9539_chips[] = {
	{
		.chip_id = TEGRA_GPIO_TCA9539_CHIPID_BASE,
		.i2c_inst = TEGRABL_INSTANCE_I2C1,
		.i2c_name = I2C1_NODE,
		.i2c_addr = 0x77,
	},
	{
		.chip_id = TEGRA_GPIO_TCA9539_CHIPID_BASE + 1,
		.i2c_inst = TEGRABL_INSTANCE_I2C1,
		.i2c_name = I2C1_NODE,
		.i2c_addr = 0x74,
	},
};

#endif
