/*
 * Copyright (c) 2015-2019, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_CPUBL_PARAMS_H
#define INCLUDED_TEGRABL_CPUBL_PARAMS_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include <stdint.h>
#include <tegrabl_compiler.h>
#include <tegrabl_global_data.h>
#include <tegrabl_error.h>
#include <tegrabl_rollback_prevention.h>
#include <tegrabl_mb1_bct.h>
#include <tegrabl_mb2_bct.h>

#define TBOOT_CPUBL_PARAMS_VERSION 6

/**
 * Consolidated structure to pass information sharing from MB2(Tboot-BPMP) to
 * CPU bootloader
 */
#define NUM_DRAM_BAD_PAGES 1024

TEGRABL_PACKED(
struct tboot_cpubl_params {
	/**< Global Data shared across boot-binaries */
	struct tegrabl_global_data global_data;

	union {
		uint8_t byte_array[340];

		struct {
			/**< version */
			TEGRABL_DECLARE_ALIGNED(uint32_t version, 4);

			/**< Uart instance */
			TEGRABL_DECLARE_ALIGNED(uint32_t uart_instance, 4);

			/**< Enable logs */
			TEGRABL_DECLARE_ALIGNED(uint32_t enable_log, 4);

			/**< Address of device params from mb1 bct */
			TEGRABL_DECLARE_ALIGNED(uint64_t dev_params_address, 8);

			/**< Address of i2c bus frequecy from mb1 bct */
			TEGRABL_DECLARE_ALIGNED(uint64_t i2c_bus_frequency_address, 8);

			/**< Address of controller pad settings */
			TEGRABL_DECLARE_ALIGNED(uint64_t controller_prod_settings, 8);

			/**< Total size of controller pad settings */
			TEGRABL_DECLARE_ALIGNED(uint64_t controller_prod_settings_size, 8);

			/**< Parameters for Secure_OS/TLK passed via GPR */
			TEGRABL_DECLARE_ALIGNED(uint64_t secure_os_params[4], 8);
			TEGRABL_DECLARE_ALIGNED(uint64_t secure_os_start, 8);

			/**< If tos loaded by mb2 has secureos or not.
			 * Added in version 3.
			 */
			TEGRABL_DECLARE_ALIGNED(uint32_t secureos_type, 4);
			TEGRABL_DECLARE_ALIGNED(uint64_t golden_reg_start, 8);

			/**< dtb load address */
			TEGRABL_DECLARE_ALIGNED(uint64_t bl_dtb_load_address, 8);

			/**< rollback data address */
			TEGRABL_DECLARE_ALIGNED(uint64_t rollback_data_address, 8);

			struct tegrabl_device storage_devices[TEGRABL_MAX_STORAGE_DEVICES];

			/**< SD card related params */
			TEGRABL_DECLARE_ALIGNED(uint32_t boot_from_sd, 4);
			TEGRABL_DECLARE_ALIGNED(uint32_t sd_instance, 4);
			TEGRABL_DECLARE_ALIGNED(uint32_t cd_gpio, 4);
			TEGRABL_DECLARE_ALIGNED(uint32_t cd_gpio_polarity, 4);

			uint8_t reserved[198];
		};
	};
}
);

/**
 * @brief Sets default values of newly added variable if version
 * of passed cpu bl param is smaller than current version.
 *
 * @param params Cpubl params which needs to checked.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t cpubl_params_version_check(struct tboot_cpubl_params *params);

#if defined(__cplusplus)
}
#endif

#endif /*  INCLUDED_TEGRABL_CPUBL_PARAMS_H */

