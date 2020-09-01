/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_I2C

#include <stdint.h>
#include <tegrabl_utils.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_io.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_i2c_local.h>
#include <tegrabl_i2c_err_aux.h>
#include <tegrabl_i2c_soc_common.h>
#include <tegrabl_dpaux.h>

#define I2C_INSTANCES_MAX	10UL
#define I2C_MODES_MAX	4UL
#define I2C_LIMIT 65536
#define I2C_SOURCE_FREQ (136 * 1000) /* KHz */

static struct i2c_soc_info i2c_info[] = {
	{
		.base_addr = NV_ADDRESS_MAP_I2C1_BASE,
		.clk_freq = STD_SPEED,
		.is_muxed_dpaux = false,
	},
	{
		.base_addr = NV_ADDRESS_MAP_I2C2_BASE,
		.clk_freq = STD_SPEED,
		.is_muxed_dpaux = false,
	},
	{
		.base_addr = NV_ADDRESS_MAP_I2C3_BASE,
		.clk_freq = STD_SPEED,
		.is_muxed_dpaux = false,
	},
	{
		.base_addr = NV_ADDRESS_MAP_I2C4_BASE,
		.clk_freq = STD_SPEED,
		.is_muxed_dpaux = true,
		.dpaux_instance = DPAUX_INSTANCE_1,
	},
	{
		.base_addr = NV_ADDRESS_MAP_I2C5_BASE,
		.clk_freq = STD_SPEED,
		.is_bpmpfw_controlled = true,
		.is_cldvfs_required = true,
		.is_muxed_dpaux = false,
	},
	{
		.base_addr = NV_ADDRESS_MAP_I2C6_BASE,
		.clk_freq = STD_SPEED,
		.is_muxed_dpaux = true,
		.dpaux_instance = DPAUX_INSTANCE_0,
	},
	{
		.base_addr = NV_ADDRESS_MAP_I2C7_BASE,
		.clk_freq = STD_SPEED,
		.is_muxed_dpaux = false,
	},
	{
		.base_addr = NV_ADDRESS_MAP_I2C8_BASE,
		.clk_freq = STD_SPEED,
		.is_muxed_dpaux = false,
	},
	{
		.base_addr = NV_ADDRESS_MAP_I2C9_BASE,
		.clk_freq = STD_SPEED,
		.is_muxed_dpaux = false,
	}
};

struct tegrabl_i2c_prod_setting {
	uint32_t num_settings;
	uint32_t *settings;
};

static struct tegrabl_i2c_prod_setting i2c_prod_settings[I2C_INSTANCES_MAX][I2C_MODES_MAX];

void i2c_get_soc_info(struct i2c_soc_info **hi2c_info,
	uint32_t *num_of_instances)
{
	*hi2c_info = &i2c_info[0];
	*num_of_instances = ARRAY_SIZE(i2c_info);
}

uint32_t tegrabl_i2c_get_clk_source_rate(const struct tegrabl_i2c *hi2c)
{
	TEGRABL_UNUSED(hi2c);

	return I2C_SOURCE_FREQ;
}

tegrabl_error_t tegrabl_i2c_register_prod_settings(uint32_t instance,
		uint32_t mode, uint32_t *settings, uint32_t num_settings)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((instance >= I2C_INSTANCES_MAX) || (mode >= I2C_MODES_MAX)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER,
				TEGRABL_I2C_REGISTER_PROD_SETTINGS_1);
		TEGRABL_SET_ERROR_STRING(error, "instance: %d, mode: %d", instance, mode);
		goto fail;
	}

	if ((settings == NULL) || (num_settings == 0U)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER,
				TEGRABL_I2C_REGISTER_PROD_SETTINGS_2);
		TEGRABL_SET_ERROR_STRING(error, "settings: %p, num_settings: %d", settings, num_settings);
		goto fail;
	}

	i2c_prod_settings[instance][mode].num_settings = num_settings;
	i2c_prod_settings[instance][mode].settings = settings;

fail:
	return error;
}

void i2c_set_prod_settings(struct tegrabl_i2c *hi2c)
{
	uint32_t i = 0;
	uint32_t reg = 0;
	uint32_t mode;
	struct tegrabl_i2c_prod_setting *setting = NULL;
	uint32_t reg_addr = 0;
	uint32_t base_addr = 0;
	uint32_t instance = 0;

	TEGRABL_ASSERT(hi2c != NULL);

#if defined(CONFIG_POWER_I2C_BPMPFW)
	if (hi2c->is_enable_bpmpfw_i2c == true) {
		return;
	}
#endif

	if (hi2c->clk_freq > FM_PLUS_SPEED) {
		mode = 3;
	} else if (hi2c->clk_freq > FM_SPEED) {
		mode = 2;
	} else if (hi2c->clk_freq > STD_SPEED) {
		mode = 1;
	} else {
		mode = 0;
	}

	instance = hi2c->instance;
	base_addr = i2c_info[instance].base_addr;
	setting = &i2c_prod_settings[instance][mode];

	if (setting->num_settings != 0U) {
		/* Apply prod settings using <addr, mask, value> tuple */
		for (i = 0; i < (setting->num_settings * 3U); i += 3U) {
			reg_addr = setting->settings[i];

			if ((base_addr < reg_addr) || (reg_addr > (base_addr + I2C_LIMIT))) {
				continue;
			}

			reg = NV_READ32(setting->settings[i]);
			reg &= (~setting->settings[i + 1UL]);
			reg |= (setting->settings[i + 2UL] &
					setting->settings[i + 1UL]);
			NV_WRITE32(setting->settings[i], reg);
		}
	}
	return;
}

