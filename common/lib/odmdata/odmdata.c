/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_ODM_DATA

#include <tegrabl_odmdata_soc.h>
#include <tegrabl_odmdata_lib.h>

struct odmdata_params odmdata_array[ODMDATA_PROP_TYPE_MAX] = {
	[ENABLE_TEGRA_WDT] = {
		.mask = TEGRA_WDT_MASK,
		.val = ENABLE_TEGRAWDT_VAL,
		.name = "enable-tegra-wdt"
	},
	[DISABLE_TEGRA_WDT] = {
		.mask = TEGRA_WDT_MASK,
		.val = DISABLE_TEGRAWDT_VAL,
		.name = "disable-tegra-wdt"
	},
	[ENABLE_DEBUG_CONSOLE] = {
		.mask = DEBUG_CONSOLE_MASK,
		.val = ENABLE_DEBUG_CONSOLE_VAL,
		.name = "enable-debug-console"
	},
	[DISABLE_DEBUG_CONSOLE] = {
		.mask = DEBUG_CONSOLE_MASK,
		.val = ENABLE_HIGHSPEED_UART_VAL,
		.name = "enable-high-speed-uart"
	},
	[ANDROID_BUILD] = {
		.mask = OS_BUILD_MASK,
		.val = ANDROID_BUILD_VAL,
		.name = "android-build"
	},
	[MODS_BUILD] = {
		.mask = OS_BUILD_MASK,
		.val = MODS_BUILD_VAL,
		.name = "mods-build"
	},
	[L4T_BUILD] = {
		.mask = OS_BUILD_MASK,
		.val = L4T_BUILD_VAL,
		.name = "l4t-build"
	},
	[DISABLE_DENVER_WDT] = {
		.mask = DENVER_WDT_MASK,
		.val = DISABLE_DENVER_WDT_VAL,
		.name = "disable-denver-wdt"
	},
	[ENABLE_DENVER_WDT] = {
		.mask = DENVER_WDT_MASK,
		.val = ENABLE_DENVER_WDT_VAL,
		.name = "enable-denver-wdt"
	},
	[DISABLE_PMIC_WDT] = {
		.mask = PMIC_WDT_MASK,
		.val = DISABLE_PMIC_WDT_VAL,
		.name = "disable-pmic-wdt"
	},
	[ENABLE_PMIC_WDT] = {
		.mask = PMIC_WDT_MASK,
		.val = ENABLE_PMIC_WDT_VAL,
		.name = "enable-pmic-wdt"
	},
	[DISABLE_SDMMC_HWCQ] = {
		.mask = SDMMC_HWCQ_MASK,
		.val = DISABLE_SDMMC_HWCQ_VAL,
		.name = "disable-sdmmc-hwcq"
	},
	[ENABLE_SDMMC_HWCQ] = {
		.mask = SDMMC_HWCQ_MASK,
		.val = ENABLE_SDMMC_HWCQ_VAL,
		.name = "enable-sdmmc-hwcq",
	},
	[NO_BATTERY] = {
		.mask = BATTERY_ADAPTER_MASK,
		.val = NO_BATTERY_VAL,
		.name = "no-battery"
	},
	[BATTERY_CONNECTED] = {
		.mask = BATTERY_ADAPTER_MASK,
		.val = BATTERY_CONNECTED_VAL,
		.name = "battery-connected"
	},
	[NORMAL_FLASHED] = {
		.mask = SANITY_FLASH_MASK,
		.val = NORMAL_FLASHED_VAL,
		.name = "normal-flashed"
	},
	[SANITY_FLASHED] = {
		.mask = SANITY_FLASH_MASK,
		.val = SANITY_FLASHED_VAL,
		.name = "sanity-flashed"
	},
	[UPHY_LANE0_PCIE] = {
		.mask = UPHY_LANE0_MASK,
		.val = UPHY_LANE0_ENABLE_PCIE,
		.name = "enable-pcie-on-uphy-lane0",
	},
	[UPHY_LANE0_XUSB] = {
		.mask = UPHY_LANE0_MASK,
		.val = UPHY_LANE0_ENABLE_XUSB,
		.name = "enable-xusb-on-uphy-lane0",
	},
	[UPHY_LANE1_PCIE] = {
		.mask = UPHY_LANE1_MASK,
		.val = UPHY_LANE1_ENABLE_PCIE,
		.name = "enable-pcie-on-uphy-lane1",
	},
	[UPHY_LANE1_XUSB] = {
		.mask = UPHY_LANE1_MASK,
		.val = UPHY_LANE1_ENABLE_XUSB,
		.name = "enable-xusb-on-uphy-lane1",
	},
	[UPHY_LANE2_PCIE] = {
		.mask = UPHY_LANE2_MASK,
		.val = UPHY_LANE2_ENABLE_PCIE,
		.name = "enable-pcie-on-uphy-lane2",
	},
	[UPHY_LANE2_XUSB] = {
		.mask = UPHY_LANE2_MASK,
		.val = UPHY_LANE2_ENABLE_XUSB,
		.name = "enable-xusb-on-uphy-lane2",
	},
	[UPHY_LANE4_PCIE] = {
		.mask = UPHY_LANE4_MASK,
		.val = UPHY_LANE4_ENABLE_PCIE,
		.name = "enable-pcie-on-uphy-lane4",
	},
	[UPHY_LANE4_UFS] = {
		.mask = UPHY_LANE4_MASK,
		.val = UPHY_LANE4_ENABLE_UFS,
		.name = "enable-ufs-on-uphy-lane4",
	},
	[UPHY_LANE5_SATA] = {
		.mask = UPHY_LANE5_MASK,
		.val = UPHY_LANE5_ENABLE_SATA,
		.name = "enable-sata-on-uphy-lane5",
	},
	[UPHY_LANE5_UFS] = {
		.mask = UPHY_LANE5_MASK,
		.val = UPHY_LANE5_ENABLE_UFS,
		.name = "enable-ufs-on-uphy-lane5",
	},
	[BOOTLOADER_LOCK] = {
		.mask = BOOTLOADER_LOCK_MASK,
		.val = BOOTLOADER_LOCK_VAL,
		.name = "bootloader_locked",
	},
};

