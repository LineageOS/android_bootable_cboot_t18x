/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_MB2_BCT_H
#define INCLUDED_TEGRABL_MB2_BCT_H

#include <stdint.h>
#include <stdbool.h>
#include <tegrabl_compiler.h>
#include <tegrabl_nvtypes.h>

#define TEGRABL_MAX_STORAGE_DEVICES  5U

/**
 * @brief Defines bit allocation for settings of a feature
 * in mb2. If fields crosses uint64_t then move it to next data.
 */
struct tegrabl_mb2_feature_fields {
	union {
		uint64_t data1;
		struct {
			uint64_t disable_cpu_l2ecc:1;
			/* Added in mb1bct mb2 params version 2 */
			uint64_t enable_sce:1;
			uint64_t enable_ape:1;
			/* Added in mb1bct mb2 params version 3 */
			uint64_t enable_sce_safety:1;
			/* Added in mb1bct mb2 params version 4 */
			uint64_t disable_rpmb_rollback:1;
			uint64_t disable_emmc_send_cmd0_cmd1:1;
			/* Added in mb1bct mb2 params version 5 */
			uint64_t dram_ecc_error_inject:1;
			/* Added in mb1bct mb2 params version 6 */
			uint64_t boot_from_sd:1;
		};
	};
	union {
		uint64_t data2;
	};
	union {
		uint64_t data3;
	};
	union {
		uint64_t data4;
	};
};

TEGRABL_PACKED(
struct tegrabl_device {
	uint8_t type;
	uint8_t instance;
}
);

/* Added in mb1bct mb2 params version 6 */
TEGRABL_PACKED(
struct tegrabl_sd_params {
	uint8_t cd_gpio;
	uint8_t en_vdd_sd_gpio;
	uint8_t cd_gpio_polarity;
	uint8_t instance;
	uint8_t reserved[12];
}
);

/**
 * @brief Reserves some area of mb1-bct for new mb2 parameters
 * shared from mb1 to mb2. After adding member variable reduce
 * the size of reserved array.
 */
TEGRABL_PACKED(
struct tegrabl_mb1bct_mb2_params {
	uint32_t version;
	struct tegrabl_mb2_feature_fields feature_fields;
	struct tegrabl_device storage_devices[TEGRABL_MAX_STORAGE_DEVICES];
	struct tegrabl_sd_params sd_params;
	uint8_t reserved[998];
}
);

#endif /* INCLUDED_TEGRABL_MB2_BCT_H */
