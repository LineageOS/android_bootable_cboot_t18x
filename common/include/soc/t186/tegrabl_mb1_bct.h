/*
 * Copyright (c) 2015-2018, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_MB1_BCT_H
#define INCLUDED_TEGRABL_MB1_BCT_H

#include <stdint.h>
#include <stdbool.h>
#include <tegrabl_compiler.h>
#include <tegrabl_nvtypes.h>
#include <nvboot_sdram_param.h>
#include <tegrabl_mb2_bct.h>

#define TEGRABL_MB1BCT_VERSION				15
#define TEGRABL_MB1BCT_AST_VERSION			2
#define TEGRABL_MB1BCT_WDT_VERSION			1
#define TEGRABL_MB1BCT_SW_CARVEOUT_VERSION	3
#define TEGRABL_MB1BCT_DEBUG_VERSION		1
#define TEGRABL_MB1BCT_AOTAG_VERSION		1
#define TEGRABL_MB1BCT_DEV_PARAM_VERSION	1
#define TEGRABL_MB1BCT_MB2_PARAM_VERSION	4
#define TEGRABL_MB1BCT_AOCLUSTER_VERSION	1

#define NON_SECURE_PACK_REGS_COUNT		289
#define SECURE_PACK_REGS_COUNT			324

#define NUM_SDRAM_PARAMS	4

#define TEGRABL_MB1BCT_FIRWARE_MAX_COPIES	2

#define TEGRABL_MB1BCT_MAX_I2C_BUSES 9

/**
 * Size to align the signed section to multiple of 16 bytes
 * its calculated based on current signed section size
 */
#define TEGRABL_MB1BCT_RESERVED_SIZE 12

/**
 * @brief Defines the BootDevice Type available
 */
typedef enum {
	TEGRABL_MB1BCT_NONE,
	TEGRABL_MB1BCT_SDMMC_BOOT,
	TEGRABL_MB1BCT_SDMMC_USER,
	TEGRABL_MB1BCT_SDMMC_RPMB,
	TEGRABL_MB1BCT_QSPI,
	TEGRABL_MB1BCT_SATA,
	TEGRABL_MB1BCT_UFS,
	TEGRABL_MB1BCT_MAX,
	TEGRABL_MB1BCT_BOOT_DEVICE_SIZE = 0x7fffffff
} tegrabl_mb1_bct_boot_device_t;

typedef enum {
	TEGRABL_MB1BCT_FIRMWARE_TYPE_PREBOOT,
	TEGRABL_MB1BCT_FIRMWARE_TYPE_BOOTPACK,
	TEGRABL_MB1BCT_FIRMWARE_TYPE_EARLY_SPEFW,
	TEGRABL_MB1BCT_FIRMWARE_TYPE_DRAM_ECC,
	TEGRABL_MB1BCT_FIRMWARE_TYPE_BLACKLIST_INFO,
	TEGRABL_MB1BCT_FIRMWARE_TYPE_EXTENDED_CAN,
	TEGRABL_MB1BCT_FIRMWARE_TYPE_MB2,
	TEGRABL_MB1BCT_FIRMWARE_TYPE_FUSEBYPASS,
	TEGRABL_MB1BCT_FIRMWARE_TYPE_SC7_RESUME_FW,
	TEGRABL_MB1BCT_FIRMWARE_TYPE_RESERVED1,
	/* Added in Version 11 */
	TEGRABL_MB1BCT_FIRMWARE_TYPE_SMD = TEGRABL_MB1BCT_FIRMWARE_TYPE_RESERVED1,
	TEGRABL_MB1BCT_FIRMWARE_TYPE_RESERVED2,
	TEGRABL_MB1BCT_FIRMWARE_TYPE_RPB = TEGRABL_MB1BCT_FIRMWARE_TYPE_RESERVED2,
	TEGRABL_MB1BCT_FIRMWARE_TYPE_RESERVED3,
	TEGRABL_MB1BCT_FIRMWARE_TYPE_MAX,
} tegrabl_mb1bct_firmware_type_t;

enum tegrabl_sata_mode {
	TEGRABL_SATA_MODE_PIO,
	TEGRABL_SATA_MODE_AHCI,
	TEGRABL_SATA_MODE_MAX,
};

enum tegrabl_sata_speed {
	TEGRABL_SATA_SPEED_GEN1,
	TEGRABL_SATA_SPEED_GEN2,
	TEGRABL_SATA_SPEED_GEN3,
	TEGRABL_SATA_SPEED_MAX,
};

typedef struct tegrabl_firmware_info {
	/* Storage device that contains the firmware */
	tegrabl_mb1_bct_boot_device_t device_type;
	/* Storage device instance */
	uint32_t device_instance;
	/* Start block of the firmware on the mentioned storage device */
	uint32_t start_block;
	/* Length of the partition holding the binary */
	uint32_t partition_size;
} tegrabl_firmware_info_t;

enum tegrabl_platform_config_type {
	TEGRABL_PLATFORM_CONFIG_PINMUX,
	TEGRABL_PLATFORM_CONFIG_SCR,
	TEGRABL_PLATFORM_CONFIG_PMC,
	TEGRABL_PLATFORM_CONFIG_BOOTROM_COMMAND,
	TEGRABL_PLATFORM_CONFIG_PMIC_RAIL,
	TEGRABL_PLATFORM_CONFIG_PROD_CONFIG,
	TEGRABL_PLATFORM_CONFIG_CONTROLLER_PROD_CONFIG,
	TEGRABL_PLATFORM_CONFIG_RESERVED1,
	TEGRABL_PLATFORM_CONFIG_RESERVED2,
	TEGRABL_PLATFORM_CONFIG_RESERVED3,
	TEGRABL_PLATFORM_CONFIG_MAX
};

struct tegrabl_platform_config_entry_data_header {
	/* Major number for a data block */
	uint16_t major;
	/* Minor number for a data block */
	uint16_t minor;
	/* No. of data values of 4 bytes in the data block */
	uint32_t count;
};

struct tegrabl_platform_config_entry {
	/* Offset of the data block for an entry (SCR/Pinmux/PMC/PMIC/BootROM-commands */
	uint32_t offset;
	/* Size of the data block for an entry (SCR/Pinmux/PMC/PMIC/BootROM-commands */
	uint32_t size;
};

struct tegrabl_platform_config_header {
	/* Major number of the whole platform config. */
	uint16_t major;
	/* Minor number of the whole platform config. */
	uint16_t minor;
	/* Number of entries in platform config. */
	uint32_t count;
	/* Info struct for each entry like Pinmux/SCR/PMC/PMIC/BootROM Command */
	struct tegrabl_platform_config_entry configs[TEGRABL_PLATFORM_CONFIG_MAX];
};

struct tegrabl_platform_config {
	/* Platform config header */
	struct tegrabl_platform_config_header header;
	/* Data blob for entries like Pinmux/SCR/PMC/PMIC/BootROM-commands*/
	uint8_t data[];
};

enum tegrabl_mb1bct_controller_id {
	TEGRABL_MB1BCT_CONTROLLER_GENERIC,
	TEGRABL_MB1BCT_CONTROLLER_I2C,
};

/**
 * @brief Defines bit allocation for settings of a feature
 * in mb1. If fields crosses uint64_t then move it to next data.
 */
struct tegrabl_feature_fields {
	union {
		uint64_t data1;
		struct {
			uint64_t enable_can_boot:1;		/* Bits 0-0 */
			uint64_t enable_blacklisting:1;		/* Bits 1-1 */
			uint64_t disable_sc7:1;			/* Bits 2-2 */
			uint64_t fuse_visibility:1;		/* Bits 3-3 */
			uint64_t enable_vpr_resize:1;		/* Bits 4-4 */
			uint64_t disable_el3_bl:1;		/* Bits 5-5 */
			uint64_t dram_carveout_end_of_memory:1; /* Bits 6-6 */
			uint64_t disable_staged_scrub:1;	/* Bits 7-7 */
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

/* NOTE: Add member variable in uint64_t, uint32_t, uint16_t, uint8_t
 * order.
 */

TEGRABL_PACKED(
struct tegrabl_mb1bct_clock_data {
	uint8_t bpmp_cpu_nic_divider;
	uint8_t bpmp_apb_divider;
	uint8_t axi_cbb_divider;
	uint8_t se_divider;
	uint8_t aon_cpu_nic_divider;
	uint8_t aon_apb_divider;
	uint8_t aon_can0_divider;
	uint8_t aon_can1_divider;
	uint8_t osc_drive_strength;
	uint8_t pllaon_divp;
	uint8_t pllaon_divn;
	uint8_t pllaon_divm;
	int16_t pllaon_divn_frac;
	uint8_t reserved[10];
}
);

TEGRABL_PACKED(
struct tegrabl_mb1bct_cpu_data {
	uint64_t ccplex_platform_features;
	uint64_t lsr_dvcomp_params_m_cluster;
	uint64_t lsr_dvcomp_params_b_cluster;
	uint32_t nafll_m_cluster_data;
	uint32_t nafll_b_cluster_data;
	uint32_t voltage_m_cluster;
	uint32_t voltage_b_cluster;
	uint32_t aarch32_reset_vector;
	uint8_t bootcpu;
	uint8_t reserved[19];
}
);

TEGRABL_PACKED(
struct tegrabl_mb1bct_ast_data {
	uint32_t version;
	uint32_t bpmp_fw_va;
	uint32_t mb2_va;
	uint32_t spe_fw_va;
	uint32_t sce_fw_va;
	uint32_t ape_fw_va;
	uint32_t apr_va;
	uint8_t bpmp_stream_id;
	uint8_t ape_stream_id;
	uint8_t sce_stream_id;
	uint8_t ao_stream_id;
	uint8_t reserved[8];
}
);

TEGRABL_PACKED(
struct tegrabl_mb1bct_wdt {
	uint32_t version;
	uint32_t bpmp_wdtcr;
	uint32_t sce_wdtcr;
	uint32_t aon_wdtcr;
	uint32_t rtc2_ao_wdtcr;
	uint32_t top_wdt0_wdtcr;
	uint32_t top_wdt1_wdtcr;
	uint32_t top_wdt2_wdtcr;
	uint8_t reserved[8];
}
);

TEGRABL_PACKED(
struct tegrabl_mb1bct_sw_carveout_info {
	uint64_t addr;
	uint32_t size;
	uint32_t entrypoint_offset;
	uint32_t alignment;
}
);

TEGRABL_PACKED(
struct tegrabl_mb1bct_sw_carveout_data {
	uint32_t version;
	struct tegrabl_mb1bct_sw_carveout_info cpubl;
	struct tegrabl_mb1bct_sw_carveout_info mb2;
	struct tegrabl_mb1bct_sw_carveout_info tzdram;
	struct tegrabl_mb1bct_sw_carveout_info ramdump;
	struct tegrabl_mb1bct_sw_carveout_info mb2_heap;
	struct tegrabl_mb1bct_sw_carveout_info cpubl_params;
	/* Resered space to store info of one more carveout. */
	uint8_t reserved[20];
}
);

TEGRABL_PACKED(
struct tegrabl_mb1bct_debug {
	uint32_t version;
	uint32_t features;
	uint8_t uart_instance;
	uint8_t enable_log;
	uint8_t enable_secure_settings;
	uint8_t reserved[5];
}
);

TEGRABL_PACKED(
struct tegrabl_mb1bct_aotag {
	uint32_t version;
	int32_t boot_temp_threshold;
	int32_t cooldown_temp_threshold;
	uint64_t cooldown_temp_timeout;
	uint8_t enable_shutdown;
	uint8_t reserved[3];
}
);

TEGRABL_PACKED(
struct tegrabl_mb1bct_qspi_params {
	uint32_t clk_src; /* 0 = PLLP_OUT0, 4 = PLLC4_MUXED, 6 = CLK_M */
	uint32_t clk_div;
	uint32_t width; /* 0 = x1, 1 = x2, 2 = x4 */
	uint32_t dma_type; /* 0 = DMA_GPC, 1 = DMA_BPMP, 2 = DMA_SPE */
	uint32_t xfer_mode; /* 0 = PIO, 1 = DMA */
	uint32_t read_dummy_cycles;
	uint32_t trimmer_val1; /* tx_clk_tap_delay */
	uint32_t trimmer_val2; /* rx_clk_tap_delay */
	uint8_t reserved[8];
}
);

TEGRABL_PACKED(
struct tegrabl_mb1bct_emmc_params {
	uint32_t clk_src;
	uint32_t instance;
	uint32_t best_mode;
	uint32_t tap_value;
	uint32_t trim_value;
	uint32_t pd_offset;
	uint32_t pu_offset;
	uint8_t reserved[12];
}
);

TEGRABL_PACKED(
struct tegrabl_mb1bct_sata_params {
	enum tegrabl_sata_mode mode;
	enum tegrabl_sata_speed speed;
	uint32_t instance;
	uint8_t phy_reinit;
	uint8_t reserved[11];
}
);

TEGRABL_PACKED(
struct tegrabl_mb1bct_device_params {
	uint32_t version;
	struct tegrabl_mb1bct_qspi_params qspi;
	struct tegrabl_mb1bct_emmc_params emmc;
	struct tegrabl_mb1bct_sata_params sata;
	uint8_t reserved[12];
}
);

/**
 * @brief Reserves some area of mb1-bct for data required to
 * configure AOCluster
 * Added in mb1-bct version 11.
 */
TEGRABL_PACKED(
struct tegrabl_aocluster_data {
	uint32_t version;
	uint32_t evp_reset_addr;
	uint8_t reserved[16];
}
);

/**
 * @brief Duplicate rollback struct definition(tegrabl_rollback_prevention.h)
 * here to avoid regression if the struct changes outside.
 ***************************************************************************
 * Should always be aligned with that in tegrabl_rollback_prevention.h
 ***************************************************************************
 */
TEGRABL_PACKED(
struct rb_limits {
	uint8_t boot;
	uint8_t bpmp_fw;
	uint8_t tos;
	uint8_t tsec;
	uint8_t nvdec;
	uint8_t srm;
	uint8_t tsec_gsc_ucode;
	uint8_t early_spe_fw;
	uint8_t extended_spe_fw;
}
);
TEGRABL_PACKED(
struct tegrabl_mb1bct_rollback {
	uint8_t version;
	uint8_t enabled;
	uint8_t fuse_idx;
	uint8_t level;
	struct rb_limits limits;
	uint8_t reserved[51];
}
);

/**
 * @brief Any new additional member variable of mb1-bct should
 * be added here. After adding member variable reduce
 * the size of reserved array.
 */
TEGRABL_PACKED(
struct tegrabl_mb1bct_reserved {
	uint8_t reserved[888];
}
);

struct tegrabl_sdram_pack {
	/*
	 * represents non-secure scratch array of 428 registers of which only
	 * 289 are used packing
	 */
	uint32_t scratch_data[NON_SECURE_PACK_REGS_COUNT];
	/*
	 * represents secure scratch array of 379 registers of which only
	 * 324 are used for packing
	 */
	uint32_t sec_scratch_data[SECURE_PACK_REGS_COUNT];
};

/* Entire structure would be signed and the generic header would be added
 * to it */
TEGRABL_PACKED(
struct tegrabl_mb1_bct {
	uint32_t bctsize;
	uint32_t version;
	TEGRABL_DECLARE_ALIGNED(
		struct tegrabl_feature_fields feature_fields, 8
	);
	TEGRABL_DECLARE_ALIGNED(
		struct tegrabl_mb1bct_debug debug, 8
	);
	TEGRABL_DECLARE_ALIGNED(
		struct tegrabl_mb1bct_aotag aotag, 8
	);
	TEGRABL_DECLARE_ALIGNED(
		struct tegrabl_mb1bct_clock_data clock, 8
	);
	TEGRABL_DECLARE_ALIGNED(
		struct tegrabl_mb1bct_cpu_data cpu, 8
	);
	TEGRABL_DECLARE_ALIGNED(
		struct tegrabl_mb1bct_ast_data ast, 8
	);
	TEGRABL_DECLARE_ALIGNED(
		struct tegrabl_mb1bct_sw_carveout_data sw_carveout, 8
	);
	TEGRABL_DECLARE_ALIGNED(
		struct tegrabl_firmware_info
			fw_info[TEGRABL_MB1BCT_FIRMWARE_TYPE_MAX]
					[TEGRABL_MB1BCT_FIRWARE_MAX_COPIES], 8
	);
	/* I2C bus frequency should be in KHz. */
	TEGRABL_DECLARE_ALIGNED(
		uint32_t i2c_bus_frequency[TEGRABL_MB1BCT_MAX_I2C_BUSES], 8
	);

	TEGRABL_DECLARE_ALIGNED(struct tegrabl_mb1bct_device_params dev_param, 8);

	TEGRABL_DECLARE_ALIGNED(struct tegrabl_mb1bct_mb2_params mb2_params, 8);

	TEGRABL_DECLARE_ALIGNED(struct tegrabl_mb1bct_wdt wdt, 8);

	TEGRABL_DECLARE_ALIGNED(struct tegrabl_aocluster_data aocluster_data, 8);

	TEGRABL_DECLARE_ALIGNED(uint32_t cpubl_load_offset, 8);

	TEGRABL_DECLARE_ALIGNED(struct tegrabl_mb1bct_rollback rollback, 8);

	TEGRABL_DECLARE_ALIGNED(struct tegrabl_mb1bct_reserved reserved, 8);

	TEGRABL_DECLARE_ALIGNED(
		uint8_t num_sdram_params, 8
	);

	TEGRABL_DECLARE_ALIGNED(
		NvBootSdramParams sdram_params[NUM_SDRAM_PARAMS], 8
	);

	/* 4 instances of packing structure where each instance corresponds to
	 * its respective sdram_param instance
	 */
	TEGRABL_DECLARE_ALIGNED(
		struct tegrabl_sdram_pack sdram_pack[NUM_SDRAM_PARAMS], 8
	);

	TEGRABL_DECLARE_ALIGNED(struct tegrabl_platform_config platform_config, 8);
}
);

typedef struct tegrabl_mb1_bct tegrabl_mb1_bct_t;

#endif /* INCLUDED_TEGRABL_MB1_BCT_H */
