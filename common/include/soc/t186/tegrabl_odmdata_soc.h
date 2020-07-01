/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef TEGRABL_ODMDATA_SOC_H
#define TEGRABL_ODMDATA_SOC_H

#include <stddef.h>
#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_odmdata_lib.h>

#define ODM_DATA_OFFSET				0x678

#define UPHY_LANE0_MASK				0x01000000
#define UPHY_LANE1_MASK				0x02000000
#define UPHY_LANE2_MASK				0x04000000
#define UPHY_LANE4_MASK				0x08000000
#define UPHY_LANE5_MASK				0x10000000
#define UPHY_LANE0_ENABLE_PCIE		0x00000000
#define UPHY_LANE0_ENABLE_XUSB		0x01000000
#define UPHY_LANE1_ENABLE_PCIE		0x00000000
#define UPHY_LANE1_ENABLE_XUSB		0x02000000
#define UPHY_LANE2_ENABLE_PCIE		0x00000000
#define UPHY_LANE2_ENABLE_XUSB		0x04000000
#define UPHY_LANE4_ENABLE_PCIE		0x00000000
#define UPHY_LANE4_ENABLE_UFS		0x08000000
#define UPHY_LANE5_ENABLE_SATA		0x00000000
#define UPHY_LANE5_ENABLE_UFS		0x10000000

#define DEBUG_CONSOLE_MASK			0xC0000
#define ENABLE_DEBUG_CONSOLE_VAL	0x80000
#define ENABLE_HIGHSPEED_UART_VAL	0x00000
#define OS_BUILD_MASK				0x3
#define ANDROID_BUILD_VAL			0x0
#define MODS_BUILD_VAL				0x1
#define L4T_BUILD_VAL				0x2
#define TEGRA_WDT_MASK				0x8000
#define ENABLE_TEGRAWDT_VAL			0x8000
#define DISABLE_TEGRAWDT_VAL		0x0000
#define DENVER_WDT_MASK				0x10000
#define DISABLE_DENVER_WDT_VAL		0x00000
#define ENABLE_DENVER_WDT_VAL		0x10000
#define PMIC_WDT_MASK				0x20000
#define DISABLE_PMIC_WDT_VAL		0x00000
#define ENABLE_PMIC_WDT_VAL			0x20000
#define SDMMC_HWCQ_MASK				0x4000
#define DISABLE_SDMMC_HWCQ_VAL			0x0000
#define ENABLE_SDMMC_HWCQ_VAL			0x4000
#define BATTERY_ADAPTER_MASK		0x300000
#define NO_BATTERY_VAL				0x000000
#define BATTERY_CONNECTED_VAL		0x100000
#define SANITY_FLASH_MASK			0x4
#define NORMAL_FLASHED_VAL			0x0
#define SANITY_FLASHED_VAL			0x4
#define TEGRA_BOOTMODE_MASK			(0x7 << 3)
#define TEGRA_BOOTMODE_ANDROID_UI_VAL		(0x0 << 3)
#define TEGRA_BOOTMODE_ANDROID_SHELL_VAL	(0x1 << 3)
#define TEGRA_BOOTMODE_ANDROID_PRESI_VAL	(0x2 << 3)
#define TEGRA_BOOTLOADER_LOCK_BIT   13
#define BOOTLOADER_LOCK_MASK		(0x1 << TEGRA_BOOTLOADER_LOCK_BIT)
#define BOOTLOADER_LOCK_VAL			(0x1 << TEGRA_BOOTLOADER_LOCK_BIT)
#define BOOTLOADER_UNLOCK_VAL		0x0

/* macro odmdata prop type */
typedef uint32_t odmdata_prop_type_t;
#define ENABLE_TEGRA_WDT 0
#define DISABLE_TEGRA_WDT 1
#define ENABLE_DEBUG_CONSOLE 2
#define DISABLE_DEBUG_CONSOLE 3
#define ANDROID_BUILD 4
#define MODS_BUILD 5
#define L4T_BUILD 6
#define DISABLE_DENVER_WDT 7
#define ENABLE_DENVER_WDT 8
#define DISABLE_PMIC_WDT 9
#define ENABLE_PMIC_WDT 10
#define DISABLE_SDMMC_HWCQ 11
#define ENABLE_SDMMC_HWCQ 12
#define NO_BATTERY 13
#define BATTERY_CONNECTED 14
#define SANITY_FLASHED 15
#define NORMAL_FLASHED 16
#define UPHY_LANE0_PCIE 17
#define UPHY_LANE0_XUSB 18
#define UPHY_LANE1_PCIE 19
#define UPHY_LANE1_XUSB 20
#define UPHY_LANE2_PCIE 21
#define UPHY_LANE2_XUSB 22
#define UPHY_LANE4_PCIE 23
#define UPHY_LANE4_UFS 24
#define UPHY_LANE5_SATA 25
#define UPHY_LANE5_UFS 26
#define BOOTLOADER_LOCK 27
#define ODMDATA_PROP_TYPE_MAX 28

#endif /* TEGRABL_ODMDATA_SOC_H */

