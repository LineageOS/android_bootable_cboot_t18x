/*
 * Copyright (c) 2015-2018, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_SOC_MISC_H
#define INCLUDED_TEGRABL_SOC_MISC_H

#include "build_config.h"
#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_blockdev.h>
#include <tegrabl_binary_types.h>

/*
 * @brief specifies the reset source
 */
/* macro tegrabl rst source */
typedef uint32_t tegrabl_rst_source_t;
#define RST_SOURCE_SYS_RESET_N 0U
#define RST_SOURCE_AOWDT 1U
#define RST_SOURCE_BCCPLEXWDT 2U
#define RST_SOURCE_BPMPWDT 3U
#define RST_SOURCE_SCEWDT 4U
#define RST_SOURCE_SPEWDT 5U
#define RST_SOURCE_APEWDT 6U
#define RST_SOURCE_LCCPLEXWDT 7U
#define RST_SOURCE_SENSOR 8U
#define RST_SOURCE_AOTAG 9U
#define RST_SOURCE_VFSENSOR 10U
#define RST_SOURCE_MAINSWRST 11U
#define RST_SOURCE_SC7 12U
#define RST_SOURCE_HSM 13U
#define RST_SOURCE_CSITE 14U

/**
 * @brief specifies boot type either normal cold boot
 * or recovery cold boot.
 */
/* macro tegrabl boot chain type */
typedef uint32_t tegrabl_boot_chain_type_t;
#define TEGRABL_BOOT_CHAIN_PRIMARY 0U
#define TEGRABL_BOOT_CHAIN_RECOVERY 1U

/*
 * @brief specifies the fields in the miscreg strap register
 */
/* macro tegrabl strap field */
typedef uint32_t tegrabl_strap_field_t;
#define BOOT_SELECT_FIELD 0U
#define RAM_CODE_FIELD 1U

/*
 * @brief specifies the reset level
 */
/* macro tegrabl rst level */
typedef uint32_t tegrabl_rst_level_t;
#define RST_LEVEL_L0 0U
#define RST_LEVEL_L1 1U
#define RST_LEVEL_L2 2U
#define RST_LEVEL_WARM 3U

/* pmc scratch0 bit positions */
/* macro tegrabl scratch0 flag */
typedef uint32_t tegrabl_scratch0_flag_t;
#define TEGRABL_PMC_SCRATCH0_FLAG_FORCED_RECOVERY 1U
#define TEGRABL_PMC_SCRATCH0_FLAG_FASTBOOT 30U
#define TEGRABL_PMC_SCRATCH0_FLAG_BOOT_RECOVERY_KERNEL 31U

/*
 * @brief specifies the source/recipient of doorbell ring
 */
/* macro tegrabl dbell client */
typedef uint32_t tegrabl_dbell_client_t;
#define TEGRABL_DBELL_CLIENT_CCPLEX 1U		/* 0x1 */
#define TEGRABL_DBELL_CLIENT_DPMU 2U			/* 0x2 */
#define TEGRABL_DBELL_CLIENT_BPMP 3U			/* 0x3 */
#define TEGRABL_DBELL_CLIENT_SPE 4U			/* 0x4 */
#define TEGRABL_DBELL_CLIENT_SCE 5U			/* 0x5 */
#define TEGRABL_DBELL_CLIENT_DMA 6U			/* 0x6 */
#define TEGRABL_DBELL_CLIENT_TSECA 7U			/* 0x7 */
#define TEGRABL_DBELL_CLIENT_TSECB 8U			/* 0x8 */
#define TEGRABL_DBELL_CLIENT_JTAGM 9U			/* 0x9 */
#define TEGRABL_DBELL_CLIENT_CSITE 10U		/* 0xA */
#define TEGRABL_DBELL_CLIENT_APE 11U			/* 0xB */
#define TEGRABL_DBELL_CLIENT_MAX 12U			/* 0xC */

/*
 * @brief specifies the target for doorbell ring
 */
/* macro tegrabl dbell target */
typedef uint32_t tegrabl_dbell_target_t;
#define TEGRABL_DBELL_TARGET_DPMU 0U /* 0x0 */
#define TEGRABL_DBELL_TARGET_CCPLEX_TZ_NS 1U
#define TEGRABL_DBELL_TARGET_CCPLEX_TZ_S 2U
#define TEGRABL_DBELL_TARGET_BPMP 3U /* 0x3 */
#define TEGRABL_DBELL_TARGET_SPE 4U
#define TEGRABL_DBELL_TARGET_SCE 5U
#define TEGRABL_DBELL_TARGET_APE 6U /*0x6 */
#define TEGRABL_DBELL_TARGET_MAX 7U

/*
 * @brief struct to hold chip info read from register
 */
struct tegrabl_chip_info {
	uint32_t chip_id;
	uint32_t major;
	uint32_t minor;
	uint32_t revision;
};

/**
 * @brief api to trigger the doorbell
 *
 * @param source specifies the source of the doorbell
 * @param target specifies the target of the doorbell
 *
 * @return TEGRABL_NO_ERROR in case of success,
 */
tegrabl_error_t tegrabl_dbell_trigger(tegrabl_dbell_client_t source,
											tegrabl_dbell_target_t target);

/**
 * @brief api to capture the ack the doorbell
 *
 * @param client specifies the recipient of the ack of the doorbell
 *
 * @return ack status of the doorbel register
 */
uint32_t tegrabl_dbell_ack(tegrabl_dbell_client_t client);

/**
 * @brief Check tegra-ap-wdt bit of odmdata 0:Tegra AP watchdog disable
 * and value of HALT_IN_FIQ to determine whether to configure watchdog
 * or not
 *
 * @return bool value specifying whether to do any wdt operation or not
.*
 */
bool tegrabl_is_wdt_enable(void);

/**
 * @brief Register prod settings to the respective module driver.
 *
 * @param prod_settings Buffer pointing to set of prod settings
 * @param num_settings Number of prod settings sets
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_register_prod_settings(uint32_t *prod_settings,
		uint32_t num_settings);

/**
 * @brief read given strap field from miscreg strap register
 *
 * @param fld specifies field from miscreg strap register
 *
 * @return returns requested strap field value,
 */
uint32_t read_miscreg_strap(tegrabl_strap_field_t fld);

/**
 * @brief print rst_source and rst_level
 */
void tegrabl_print_rst_status(void);

/**
 * @brief Get either rst_source or rst_level of the device or both
 *
 * @param rst_source a pointer where enum tegrabl_rst_source will be stored
 * @param rst_level a pointer where enum tegrabl_rst_level will be stored
 *
 * @return TEGRABL_NO_ERROR in case of success,
 */
tegrabl_error_t tegrabl_get_rst_status(tegrabl_rst_source_t *rst_source,
									   tegrabl_rst_level_t *rst_level);

/**
 * @brief find out if the device is waking up from sc8 or not
 *
 * @return true if sc8 else false,
 */
bool tegrabl_rst_is_sc8_exit(void);

/**
 * @brief Get the secondary boot-device type and instance for the SOC
 *
 * @param device Output parameter to hold boot-device type
 * @param instance Output parameer to hold boot-device instance
 *
 * @return TEGRABL_NO_ERROR in case of success,
 * TEGRABL_ERR_INVALID in case either device/instance are NULL,
 * TEGRABL_ERR_NOT_SUPPORTED if the boot-device determined from straps
 * or fuses is not supported.
 */
tegrabl_error_t tegrabl_soc_get_bootdev(
		tegrabl_storage_type_t *device, uint32_t *instance);

/**
 * @brief Clear secure scratch for bootloader.
 */
void tegrabl_clear_sec_scratch_bl(void);

#if defined(CONFIG_ENABLE_APB2JTAG_WAR)
/**
 * @brief api to invoke the apb2jtag war routine
 *
 * @param length specifies the lsb length of the jtag register
 * @param instr_id specifies the instruction id for the jtag register
 * @param cluster specifies the cluster id for the jtag register
 */
void apb2jtag_mb1(uint32_t length, uint32_t instr_id, uint32_t cluster);
#endif

/** @brief Enable soc therm
 *
 *  @return TEGRABL_NO_ERROR if successful, error-code otherwise.
 */
tegrabl_error_t tegrabl_enable_soc_therm(void);

/**
* @brief reset and boot the board in recovery mode
*/
void tegrabl_boot_recovery_mode(void);

/**
* @brief reset the board
*/
void tegrabl_pmc_reset(void);

/** @brief set pmc scratch0 for fastboot, recovery and forced recovery usecases
 *
 *  @param flag specifies type of scratch0_flag
 *  @set flag will be set if true else reseted to zero
 *
 *  @return TEGRABL_NO_ERROR if successful.
 */
tegrabl_error_t tegrabl_set_pmc_scratch0_flag(
		tegrabl_scratch0_flag_t flag, bool set);

/**
 * @brief Clear scratch registers SECURE_RSV7_0 - SECURE_RSV10_1
 */
void tegrabl_clear_pmc_rsvd(void);

/** @brief get pmc scratch0 for fastboot, recovery and forced recovery usecases
 *
 *  @param flag specifies type of scratch0_flag
 *  @is_set get whether flag is set or not
 *
 *  @return TEGRABL_NO_ERROR if successful.
 */
tegrabl_error_t tegrabl_get_pmc_scratch0_flag(
		tegrabl_scratch0_flag_t flag, bool *is_set);

/** @brief set soc core voltage to given milli volts
 *
 * @param soc_mv soc core voltage to be set in milli volts
 *
 * @return TEGRABL_NO_ERROR if successful, specific error code
 *         in case of failure
 */
tegrabl_error_t tegrabl_set_soc_core_voltage(uint32_t soc_mv);

/**
 * @brief Retrieve the boot type based on contents of scratch
 * regiter.
 *
 * @return appropriate boot chain type based on scratch register value.
 */
tegrabl_boot_chain_type_t tegrabl_get_boot_chain_type(void);

/**
 * @brief Updates scratch register as per the boot chain type input.
 *
 * @param boot_chain Which chain of binaries to boot.
 */
void tegrabl_set_boot_chain_type(tegrabl_boot_chain_type_t boot_chain);

/**
 * @brief Resets the scratch register used for fallback mechanism.
 */
void tegrabl_reset_fallback_scratch(void);

/**
 * @brief Triggers recovery boot chain if current boot chain is primary or
 * vice-versa.
 */
void tegrabl_trigger_fallback_boot_chain(void);

/**
 * @brief get soc chip info from register
 *
 * @param info callee filled, chip info read
 */
void tegrabl_get_chip_info(struct tegrabl_chip_info *info);

#if defined(CONFIG_PRINT_CHIP_INFO)
/**
 * @brief prints the following information
 * bootrom patch version
 * ate fuse revision
 * opt priv sec en register
 *
 */
void mb1_print_chip_info(void);
/**
 * @brief prints the following information
 * boot configuration: denver or reilly
 *
 */

void mb1_print_cpucore_info(void);
#else
static TEGRABL_INLINE void mb1_print_chip_info(void)
{
	/* dummy implementation */
}

static TEGRABL_INLINE void mb1_print_cpucore_info(void)
{
	/* dummy implementation */
}
#endif

/**
 * @brief Select kernel to be loaded based upon value in scratch register
 *
 * @return Kernel type to be loaded
 */
tegrabl_binary_type_t tegrabl_get_kernel_type(void);

/**
 * @brief Set A/B boot active slot number info into scratch register
 *
 * @param slot_info the boot slot number with flags
 *
 * @return none
 */
void tegrabl_set_boot_slot_reg(uint32_t slot_info);

/**
 * @brief Read A/B boot slot number info from scratch register
 *
 * @return value from scratch register
 */
uint32_t tegrabl_get_boot_slot_reg(void);

/**
 * @brief Update scratch register SCRATCH_8
 *
 * @param val boot error
 */
void tegrabl_set_boot_error_scratch(uint32_t val);

/**
 * @brief Get boot error from SCRATCH_8 register
 *
 * @return boot error
 */
uint32_t tegrabl_get_boot_error(void);

/**
 * @brief Check if 28th bit is set in odmdata
 * or not
 *
 * @return bool value specifying whether ufs is enabled or not
.*
 */
bool tegrabl_is_ufs_enable(void);

/**
 * @brief checks if the given slot has non zero key
 *
 * @param keyslot to be checked
 *
 * @return true if the key is non zero
 */
bool tegrabl_keyslot_check_if_key_is_nonzero(uint8_t keyslot);

/**
 * @brief Get HSM reset reason from SCRATCH_6 register
 *
 * @return HSM reset reason
 */
uint32_t tegrabl_get_hsm_reset_reason(void);

/**
 * @brief Get bad page number from SCRATCH_7 register
 *
 * @return bad page number
 */
uint32_t tegrabl_get_bad_page_number(void);

/**
 * @brief Detect whether running on System-FPGA. This is a dummy API that
 * 		  always returns false as FPGA is unsupported on T186 anymore.
 *
 * @return false
 */
static inline bool tegrabl_is_fpga(void)
{
	return false;
}

/**
 * @brief Detect whether running on VDK
 *
 * @return true on VDK and false on other platforms
 *
 */
static inline bool tegrabl_is_vdk(void)
{
	return false;
}

/**
 * @brief get chip ecid string
 *
 * @param ecid_str - pointer to buffer to save ecid string
 *        size - buffer size
 *
 * @return TEGRABL_NO_ERROR if success; error code otherwise
 */
tegrabl_error_t tegrabl_get_ecid_str(char *ecid_str, uint32_t size);

#endif /* INCLUDED_TEGRABL_SOC_MISC_H */
