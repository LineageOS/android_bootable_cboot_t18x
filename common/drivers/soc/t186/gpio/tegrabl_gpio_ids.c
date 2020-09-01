/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <address_map_new.h>
#include <tegrabl_utils.h>
#include <tegrabl_gpio_local.h>
#include <argpio_sw.h>
#include <argpio_aon_sw.h>
#include <tegrabl_io.h>

/* main controllor addr base list */
static const uint32_t tegra_gpio_main_bases[] = {
	GPIO_A_ENABLE_CONFIG_00_0,
	GPIO_B_ENABLE_CONFIG_00_0,
	GPIO_C_ENABLE_CONFIG_00_0,
	GPIO_D_ENABLE_CONFIG_00_0,
	GPIO_E_ENABLE_CONFIG_00_0,
	GPIO_F_ENABLE_CONFIG_00_0,
	GPIO_G_ENABLE_CONFIG_00_0,
	GPIO_H_ENABLE_CONFIG_00_0,
	GPIO_I_ENABLE_CONFIG_00_0,
	GPIO_J_ENABLE_CONFIG_00_0,
	GPIO_K_ENABLE_CONFIG_00_0,
	GPIO_L_ENABLE_CONFIG_00_0,
	GPIO_M_ENABLE_CONFIG_00_0,
	GPIO_N_ENABLE_CONFIG_00_0,
	GPIO_O_ENABLE_CONFIG_00_0,
	GPIO_P_ENABLE_CONFIG_00_0,
	GPIO_Q_ENABLE_CONFIG_00_0,
	GPIO_R_ENABLE_CONFIG_00_0,
	GPIO_T_ENABLE_CONFIG_00_0,
	GPIO_X_ENABLE_CONFIG_00_0,
	GPIO_Y_ENABLE_CONFIG_00_0,
	GPIO_BB_ENABLE_CONFIG_00_0,
	GPIO_CC_ENABLE_CONFIG_00_0,
};

struct tegrabl_gpio_id tegra_gpio_id_main = {
	.devname = "gpio-main",
	.base_addr = NV_ADDRESS_MAP_GPIO_CTL_BASE,
	.bank_count = ARRAY_SIZE(tegra_gpio_main_bases),
	.bank_bases = tegra_gpio_main_bases,
};

/* aon controllor addr base list */
static const uint32_t tegra_gpio_aon_bases[] = {
	GPIO_S_ENABLE_CONFIG_00_0,
	GPIO_U_ENABLE_CONFIG_00_0,
	GPIO_V_ENABLE_CONFIG_00_0,
	GPIO_W_ENABLE_CONFIG_00_0,
	GPIO_Z_ENABLE_CONFIG_00_0,
	GPIO_AA_ENABLE_CONFIG_00_0,
	GPIO_EE_ENABLE_CONFIG_00_0,
	GPIO_FF_ENABLE_CONFIG_00_0,
};

struct tegrabl_gpio_id tegra_gpio_id_aon = {
	.devname = "gpio-aon",
	.base_addr = NV_ADDRESS_MAP_AON_GPIO_0_BASE,
	.bank_count = ARRAY_SIZE(tegra_gpio_aon_bases),
	.bank_bases = tegra_gpio_aon_bases,
};

static const struct tegrabl_pingroup tegra18x_groups[] = {
	{"usb_vbus_en0_pl4", 102, 0xd0d0},
	{"usb_vbus_en0_pl5", 103, 0xd0d8},
};

#define padctl_readl(reg) \
	NV_READ32(NV_ADDRESS_MAP_PADCTL_A0_BASE + reg)

#define padctl_writel(reg, value) \
	NV_WRITE32(NV_ADDRESS_MAP_PADCTL_A0_BASE + reg, value)

void tegrabl_pinconfig_set(uint32_t pin_num, uint32_t pinconfig)
{
	uint32_t val, pingroup_list_item;
	uint32_t pingroup_count = ARRAY_SIZE(tegra18x_groups);

	if (pingroup_count) {
		for (pingroup_list_item = 0; pingroup_list_item < pingroup_count; pingroup_list_item++) {
			if (pin_num == tegra18x_groups[pingroup_list_item].pin) {
				val = padctl_readl(tegra18x_groups[pingroup_list_item].reg_offset);
				val &= pinconfig;
				padctl_writel(tegra18x_groups[pingroup_list_item].reg_offset, val);
			}
		}
	}
}
