/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_GLOBAL_DATA_H
#define INCLUDED_TEGRABL_GLOBAL_DATA_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include <stdint.h>
#include <tegrabl_compiler.h>
#include <tegrabl_carveout_id.h>

#define TEGRABL_GLOBAL_DATA_VERSION 3

#define TEGRABL_MAX_VERSION_STRING 128 /* chars including null */

enum tegra_boot_type {
	/**< Specifies a default (unset) value. */
	BOOT_TYPE_NONE = 0,
	/**< Specifies a cold boot */
	BOOT_TYPE_COLD,
	/**< Specifies the BR entered RCM */
	BOOT_TYPE_RECOVERY,
	/**< Specifies UART boot (only available internal to NVIDIA) */
	BOOT_TYPE_UART,
	/**< Specifies that the BR immediately exited for debugging */
	/**< purposes. This can only occur when NOT in ODM production mode, */
	/**< and when a special BOOT_SELECT value is set. */
	BOOT_TYPE_EXITRCM,
	BOOT_TYPE_FORCE32 = 0x7fffffff
};

/**
 * Tracks the base and size of the Carveout
 */
struct carve_out_info {
	uint64_t base;
	uint64_t size;
};

#define NUM_DRAM_BAD_PAGES 1024

TEGRABL_PACKED(
struct tegrabl_global_data {
	/**< version */
	uint64_t version;

	/**< Cmac-hash (using 0 key) of the data */
	uint32_t hash[4];

	/**< Size of the data to be hashed */
	uint64_t hash_data_size;

	/**< Uart_base Address for debug prints */
	uint64_t early_uart_addr;

	/***< Address of bootrom bct */
	uint64_t brbct_carveout;

	/***< Address of carveout containing profiling data */
	uint64_t profiling_carveout;

	/**< Location blob required for rcm boot */
	uint64_t recovery_blob_carveout;

	/**< Carveout Info */
	struct carve_out_info carveout[CARVEOUT_NUM];

	/**< DRAM bad page info */
	uint64_t valid_dram_bad_page_count;
	uint64_t dram_bad_pages[NUM_DRAM_BAD_PAGES];

	/**< Boot mode can be cold boot, uart, recovery or RCM */
	enum tegra_boot_type boot_type;

	/**< Boot type set by nv3pserver based on boot command from host. */
	uint32_t recovery_boot_type;

	/**< Reset reason as read from PMIC */
	uint32_t pmic_rst_reason;

	/**< mb1 bct version information */
	uint32_t mb1_bct_version;

	/**< Address where mb1 version is present */
	uint64_t mb1_version_ptr;

	/**< Address where mb2 version is present */
	uint64_t mb2_version_ptr;

	/**< Safe data pointer, safe location to add any extra information. */
	uint64_t safe_data_ptr;

	/**< Parameter to unhalt SCE */
	uint8_t enable_sce_safety;

	uint8_t reserved[231];
}
);

#if defined(__cplusplus)
}
#endif

#endif /*  INCLUDED_TEGRABL_GLOBAL_DATA_H */

