/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_NO_MODULE
#define NVBOOT_TARGET_FPGA 0

#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_odmdata_lib.h>
#include <tegrabl_brbct.h>
#include <tegrabl_odmdata_soc.h>
#include <tegrabl_partition_manager.h>
#include <tegrabl_malloc.h>
#include <nvboot_bct.h>

static uint32_t odmdata;

static struct odmdata_params odmdata_array[ODMDATA_PROP_TYPE_MAX] = {
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
};

uint32_t tegrabl_odmdata_get(void)
{
	uintptr_t brbct_address;

	brbct_address = tegrabl_brbct_get();
	if (brbct_address == 0) {
		pr_error("brbct is not initialised!!, odmdata will be 0\n");
		return odmdata;
	}

	/* TODO: Use br-bct structure and avoid this macro */
	odmdata = NV_READ32(brbct_address + ODM_DATA_OFFSET);
	return odmdata;
}

static tegrabl_error_t tegrabl_odmdata_writer(const void *buffer, uint64_t size,
											  void *aux_info)
{
	return tegrabl_partition_write((struct tegrabl_partition *)aux_info, buffer,
									size);
}

/**
 * @brief flush odmdata to storage
 * @param val odmdata value
 * @return TEGRABL_NO_ERROR if success, else appropriate code
 */
static tegrabl_error_t flush_odmdata_to_storage(uint32_t val)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_partition part;
	NvBootConfigTable *brbct = NULL;
	uint64_t bct_size = 0;
	uint64_t part_size = 0;
	tegrabl_error_t (*writer)(const void *buffer, uint64_t size,
							  void *aux_info);

	/* Need to read br-bct from storage since the sdram copy may be decrypted */
	err = tegrabl_partition_open("BCT", &part);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	bct_size = sizeof(NvBootConfigTable);
	brbct = tegrabl_malloc(bct_size);
	if (brbct == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto done;
	}

	err = tegrabl_partition_read(&part, (void *)brbct, bct_size);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	NV_WRITE32(((uintptr_t)brbct) + ODM_DATA_OFFSET, val);

	err = tegrabl_partition_seek(&part, 0, TEGRABL_PARTITION_SEEK_SET);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	part_size = tegrabl_partition_size(&part);
	writer = tegrabl_odmdata_writer;
	err = tegrabl_brbct_write_multiple(writer, (void *)brbct,
									   (void *)&part, part_size, bct_size,
									   bct_size);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	pr_info("ODMDATA write successfully to storage.\n");

done:
	if (brbct != NULL) {
		tegrabl_free(brbct);
	}

	tegrabl_partition_close(&part);

	return err;
}

tegrabl_error_t tegrabl_odmdata_set(uint32_t val)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uintptr_t brbct_address;

	brbct_address = tegrabl_brbct_get();
	if (brbct_address == 0) {
		pr_error("brbct is not initialised!!\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	/* TODO: Use br-bct structure and avoid this macro */
	NV_WRITE32(brbct_address + ODM_DATA_OFFSET, val);

	/* Update odmdata to BCT partition on storage */
	err = flush_odmdata_to_storage(val);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to flush odmdata to storage\n");
		return err;
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_odmdata_params_get(
	struct odmdata_params **podmdata_list,
	uint32_t *odmdata_array_size)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if ((odmdata_array_size == NULL) || (podmdata_list == NULL)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		pr_error("Error: %d, Invalid args in 'tegrabl_odmdata_list_get'\n",
				 err);
		return err;
	}

	*podmdata_list = odmdata_array;
	*odmdata_array_size = ARRAY_SIZE(odmdata_array);

	return err;
}

bool tegrabl_odmdata_is_device_unlocked(void)
{
	uint32_t odm_data = tegrabl_odmdata_get();
	return (odm_data & (1 << TEGRA_BOOTLOADER_LOCK_BIT)) ? false : true;
}
