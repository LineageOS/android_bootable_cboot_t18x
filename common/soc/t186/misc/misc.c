/*
 * Copyright (c) 2015-2019, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE	TEGRABL_ERR_SOCMISC

#include "build_config.h"
#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_io.h>
#include <tegrabl_debug.h>
#include <tegrabl_drf.h>
#include <tegrabl_blockdev.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_partition_loader.h>
#include <tegrabl_soc_misc.h>
#include <tegrabl_fuse.h>
#include <tegrabl_clock.h>
#include <arpmc_impl.h>
#include <armiscreg.h>
#include <arscratch.h>
#include <arhsp_dbell.h>
#include <arfuse.h>
#include <tegrabl_odmdata_soc.h>
#include <tegrabl_i2c_soc_common.h>
#include <tegrabl_spi.h>
#include <string.h>

#define CONFIG_PROD_CONTROLLER_SHIFT 16

#define TOP0_DBELL_WRITE(reg, val)		\
	tegrabl_trace_write32(NV_ADDRESS_MAP_TOP0_HSP_DB_0_BASE + (reg), (val))
#define TOP0_DBELL_READ(reg)			\
	tegrabl_trace_read32(NV_ADDRESS_MAP_TOP0_HSP_DB_0_BASE + (reg))

#define PMC_READ(reg)						\
		NV_READ32(NV_ADDRESS_MAP_PMC_IMPL_BASE + PMC_IMPL_##reg##_0)
#define PMC_WRITE(reg, val)					\
		NV_WRITE32(NV_ADDRESS_MAP_PMC_IMPL_BASE + PMC_IMPL_##reg##_0, val)

#define SCRATCH_WRITE(reg, val)			\
		NV_WRITE32(NV_ADDRESS_MAP_SCRATCH_BASE + SCRATCH_##reg, val)
#define SCRATCH_READ(reg)			\
		NV_READ32(NV_ADDRESS_MAP_SCRATCH_BASE + SCRATCH_##reg)

#define MISCREG_READ(reg) NV_READ32(NV_ADDRESS_MAP_MISC_BASE + (reg))

#define DBELL_ACK_TIMEOUT_US 1000000U /* us */
#define DBELL_ACK_POLL_INTERVAL_US 5U /* us */

#define RECOVERY_BOOT_CHAIN_VALUE 0xDEADBEEF
#define RECOVERY_BOOT_CHAIN_MASK  0xFFFFFFFF

uint32_t read_miscreg_strap(tegrabl_strap_field_t fld)
{
	static uint32_t boot_dev_val;
	static uint32_t ram_code_val;
	static bool is_strap_read;
	uint32_t fld_val = 0U;

	/* Read strap boot sel from SECURE_RSV3_SCRATCH_0 */
	if (!is_strap_read) {
		uint32_t strap_val = NV_READ32(NV_ADDRESS_MAP_SCRATCH_BASE +
				SCRATCH_SECURE_RSV3_SCRATCH_0);

		/* store the values and keep it future references */
		boot_dev_val = (tegrabl_fuse_boot_dev_t)(NV_DRF_VAL(MISCREG_STRAP,
						STRAPPING_OPT_A,
						BOOT_SELECT,
						strap_val));

		/* use lower 2 bits for RAM_CODE */
		ram_code_val = (NV_DRF_VAL(MISCREG_STRAP,
						STRAPPING_OPT_A,
						RAM_CODE,
						strap_val)) & 0x3U;

		is_strap_read = true;
	}

	switch (fld) {
	case BOOT_SELECT_FIELD:
		fld_val = boot_dev_val;
		break;
	case RAM_CODE_FIELD:
		fld_val = ram_code_val;
		break;
	default:
		break;
	}

	return fld_val;
}

tegrabl_error_t tegrabl_soc_get_bootdev(
		tegrabl_storage_type_t *device, uint32_t *instance)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	tegrabl_fuse_boot_dev_t fdev;

	if ((device == NULL) || (instance == NULL)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/*
	  1) Read skip dev selection strap
	  2) If it is set, read boot device from fuse
	     if not, and strap option does not say _usefuse",
	     read from "SECURE_RSV3_SCRATCH_0" register.

	  TEGRABL_FUSE_BOOT_DEV_RESVD_4 is to represent
	  to read boot device from fuses
	*/
	fdev = read_miscreg_strap(BOOT_SELECT_FIELD);
	if (tegrabl_fuse_ignore_dev_sel_straps() ||
		(fdev == TEGRABL_FUSE_BOOT_DEV_RESVD_4)) {
		/* Read boot dev value from fuse */
		err = tegrabl_fuse_read(FUSE_SEC_BOOTDEV, (uint32_t *)&fdev,
			sizeof(tegrabl_fuse_boot_dev_t));
		if (TEGRABL_NO_ERROR != err) {
			TEGRABL_SET_HIGHEST_MODULE(err);
			pr_trace("Failed to read sec boot device from fuse\n");
			goto fail;
		}
	}

	switch (fdev) {
	case TEGRABL_FUSE_BOOT_DEV_SDMMC:
		*device = TEGRABL_STORAGE_SDMMC_BOOT;
		*instance = 3;
		pr_info("Boot-device: eMMC\n");
		break;
	case TEGRABL_FUSE_BOOT_DEV_SPIFLASH:
		*device = TEGRABL_STORAGE_QSPI_FLASH;
		*instance = 0;
		pr_info("Boot-device: QSPI\n");
		break;
	case TEGRABL_FUSE_BOOT_DEV_SATA:
		*device = TEGRABL_STORAGE_SATA;
		*instance = 0;
		pr_info("Boot-device: SATA\n");
		break;
	default:
		pr_error("Unsupported boot-device strap-reg: 0x%08x\n", fdev);
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		break;
	}
fail:
	return err;
}

void tegrabl_print_rst_status(void)
{
	uint32_t reg_val = 0;

	reg_val = PMC_READ(RST_STATUS);

	pr_info("rst_source : 0x%x\n",
			(uint16_t)NV_DRF_VAL(PMC_IMPL, RST_STATUS, RST_SOURCE, reg_val));

	pr_info("rst_level : 0x%x\n",
			(uint8_t)NV_DRF_VAL(PMC_IMPL, RST_STATUS, RST_LEVEL, reg_val));
}

tegrabl_error_t tegrabl_get_rst_status(tegrabl_rst_source_t *rst_source,
									   tegrabl_rst_level_t *rst_level)
{
	uint32_t reg_val = 0;

	if ((rst_source == NULL) && (rst_level == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	reg_val = PMC_READ(RST_STATUS);

	if (rst_source != NULL) {
		*rst_source = NV_DRF_VAL(PMC_IMPL, RST_STATUS, RST_SOURCE, reg_val);
	}

	if (rst_level != NULL) {
		*rst_level = NV_DRF_VAL(PMC_IMPL, RST_STATUS, RST_LEVEL, reg_val);
	}

	return TEGRABL_NO_ERROR;
}

bool tegrabl_rst_is_sc8_exit(void)
{
	uint32_t rst_source;

	if (tegrabl_get_rst_status(&rst_source, NULL) != TEGRABL_NO_ERROR) {
		return false;
	}

	if (rst_source == RST_SOURCE_SC7) {
		return ((PMC_READ(SC7_CONFIG) & 0x1U) == 1) ? true : false;
	}

	return false;
}

tegrabl_error_t tegrabl_enable_soc_therm(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_car_clk_enable(TEGRABL_MODULE_SOC_THERM, 0, NULL);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	err = tegrabl_car_rst_clear(TEGRABL_MODULE_SOC_THERM, 0);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	return err;
}

void tegrabl_clear_sec_scratch_bl(void)
{
	uint32_t rst_source, rst_level;
	uint32_t reg_val = 0;

#define RECOVERY_MODE		(1U << 31)
#define BOOTLOADER_MODE		(1U << 30)
#define FORCED_RECOVERY_MODE	(1U << 1)

	if (tegrabl_get_rst_status(&rst_source, &rst_level) != TEGRABL_NO_ERROR) {
		return;
	}

	/* Clear reboot reason in SCRATCH_SECURE_BL_SCRATCH_0 register when
	 * L0 reset(cold reset) by SYS_RESET_N. */
	if ((rst_level == RST_LEVEL_L0) &&
			(rst_source == RST_SOURCE_SYS_RESET_N)) {
		reg_val = NV_READ32(NV_ADDRESS_MAP_SCRATCH_BASE +
					SCRATCH_SECURE_BL_SCRATCH_0);
		reg_val &= ~(RECOVERY_MODE | BOOTLOADER_MODE |
				FORCED_RECOVERY_MODE);
		NV_WRITE32(NV_ADDRESS_MAP_SCRATCH_BASE +
				SCRATCH_SECURE_BL_SCRATCH_0, reg_val);
	}
}

void tegrabl_boot_recovery_mode(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	pr_debug("Setting recovery flag\n");
	err = tegrabl_set_pmc_scratch0_flag(TEGRABL_PMC_SCRATCH0_FLAG_FORCED_RECOVERY, true);
	if (err != TEGRABL_NO_ERROR) {
		pr_info("Failed to set scratch0_flag\n");
	}
	pr_debug("Resetting\n");
	tegrabl_pmc_reset();
}

void tegrabl_pmc_reset(void)
{
	uint32_t reg = 0;

	reg = PMC_READ(CNTRL);
	reg = NV_FLD_SET_DRF_DEF(PMC_IMPL, CNTRL, MAIN_RST, ENABLE, reg);
	PMC_WRITE(CNTRL, reg);
}

void tegrabl_clear_pmc_rsvd()
{
	/* return in case of SC8 exit */
	if (tegrabl_rst_is_sc8_exit()) {
		return;
	}

	/* Sane initialisation of all reset IP */
	SCRATCH_WRITE(SECURE_RSV7_SCRATCH_0, 0);
	SCRATCH_WRITE(SECURE_RSV7_SCRATCH_1, 0);

	SCRATCH_WRITE(SECURE_RSV8_SCRATCH_0, 0);
	SCRATCH_WRITE(SECURE_RSV8_SCRATCH_1, 0);

	SCRATCH_WRITE(SECURE_RSV9_SCRATCH_0, 0);
	SCRATCH_WRITE(SECURE_RSV9_SCRATCH_1, 0);

	SCRATCH_WRITE(SECURE_RSV10_SCRATCH_0, 0);
	SCRATCH_WRITE(SECURE_RSV10_SCRATCH_1, 0);
}

tegrabl_error_t tegrabl_set_pmc_scratch0_flag(
		tegrabl_scratch0_flag_t flag, bool set)
{
	uint32_t reg;
	reg = SCRATCH_READ(SCRATCH0_0);
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	switch (flag) {
	case TEGRABL_PMC_SCRATCH0_FLAG_FORCED_RECOVERY:
	case TEGRABL_PMC_SCRATCH0_FLAG_BOOT_RECOVERY_KERNEL:
	case TEGRABL_PMC_SCRATCH0_FLAG_FASTBOOT:
		if (set) {
			reg |= (1UL << flag);
		} else {
			reg &= ~(1UL << flag);
		}
		SCRATCH_WRITE(SCRATCH0_0, reg);
		break;
	default:
		pr_error("Flag %u not handled\n", flag);
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		break;
	}
	return err;
}

tegrabl_error_t tegrabl_get_pmc_scratch0_flag(
	tegrabl_scratch0_flag_t flag, bool *is_set)
{
	uint32_t reg;

	if (is_set == NULL) {
		return TEGRABL_ERR_INVALID;
	}

	reg = SCRATCH_READ(SCRATCH0_0);
	switch (flag) {
	case TEGRABL_PMC_SCRATCH0_FLAG_FORCED_RECOVERY:
	case TEGRABL_PMC_SCRATCH0_FLAG_BOOT_RECOVERY_KERNEL:
	case TEGRABL_PMC_SCRATCH0_FLAG_FASTBOOT:
		*is_set = ((reg & (1UL << flag)) != 0UL) ? true : false;
		break;
	default:
		pr_critical("Flag %u not handled\n", flag);
		break;
	}
	return TEGRABL_NO_ERROR;
}
tegrabl_error_t tegrabl_set_soc_core_voltage(uint32_t soc_mv)
{
	TEGRABL_UNUSED(soc_mv);
	/* Dummy function */
	return TEGRABL_NO_ERROR;
}

tegrabl_boot_chain_type_t tegrabl_get_boot_chain_type(void)
{
	uint32_t reg = 0;

	reg = SCRATCH_READ(SCRATCH_99);

	if ((reg & RECOVERY_BOOT_CHAIN_MASK) == RECOVERY_BOOT_CHAIN_VALUE) {
		return TEGRABL_BOOT_CHAIN_RECOVERY;
	}

	return TEGRABL_BOOT_CHAIN_PRIMARY;
}

void tegrabl_set_boot_chain_type(tegrabl_boot_chain_type_t boot_chain)
{
	uint32_t reg = 0;

	reg = SCRATCH_READ(SCRATCH_99);
	reg = reg & ~RECOVERY_BOOT_CHAIN_MASK;

	if (boot_chain == TEGRABL_BOOT_CHAIN_RECOVERY) {
		reg = reg | RECOVERY_BOOT_CHAIN_VALUE;
	}

	SCRATCH_WRITE(SCRATCH_99, reg);
}

void tegrabl_reset_fallback_scratch(void)
{
	SCRATCH_WRITE(SCRATCH_99, 0);
}

void tegrabl_trigger_fallback_boot_chain(void)
{
	uint32_t reg = 0;
	uint32_t val = 0;

	reg = SCRATCH_READ(SCRATCH_99);

	if ((reg & RECOVERY_BOOT_CHAIN_MASK) != RECOVERY_BOOT_CHAIN_VALUE) {
		pr_critical("Resetting device for recovery boot chain\n");
		val = RECOVERY_BOOT_CHAIN_VALUE;
	} else {
		pr_critical("Resetting device for primary boot chain\n");
	}

	reg = reg & ~RECOVERY_BOOT_CHAIN_MASK;
	reg = reg | val;

	SCRATCH_WRITE(SCRATCH_99, reg);

	tegrabl_pmc_reset();

	pr_critical("Should not be reaching here.\n");
	while (true) {
		;
	}
}

void tegrabl_get_chip_info(struct tegrabl_chip_info *info)
{
	uint32_t reg, rev;

	if (info == NULL) {
		return;
	}

	reg = NV_READ32(NV_ADDRESS_MAP_MISC_BASE + MISCREG_HIDREV_0);
	rev = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_OPT_SUBREVISION_0);

	info->chip_id = NV_DRF_VAL(MISCREG, HIDREV, CHIPID, reg);
	info->major = NV_DRF_VAL(MISCREG, HIDREV, MAJORREV, reg);
	info->minor = NV_DRF_VAL(MISCREG, HIDREV, MINORREV, reg);
	info->revision = rev;
}

#if defined(CONFIG_PRINT_CHIP_INFO)
#define PARKER_CPU_SKU 0xF3
#define REILLY_CPU_SKU 0xF0
#define DENVER_CORES_MASK 0x03U

void mb1_print_cpucore_info(void)
{
	uint32_t cores_mask=0U, denver_cores=0U, tpc=0U;

	if (tegrabl_fuse_read(FUSE_ENABLED_CPU_CORES, &cores_mask, sizeof(uint32_t)) != TEGRABL_NO_ERROR) {
		pr_error("Failed to read CPU core info\n");
		goto fail;
	}

	/* Check denver cores status */
	pr_info("Enabled Cores: 0x%x\n", cores_mask);
	denver_cores = ((cores_mask & DENVER_CORES_MASK) != 0) ? 1U:0U;

	/* Get & publish TPC disable status */
	if(tegrabl_fuse_read(FUSE_TPC_DISABLE, &tpc, sizeof(tpc)))
		pr_error("Failed to read TPC fuse\n");
	else
		pr_info("TPC disable fuse status: %d\n", tpc);

	if((cores_mask == PARKER_CPU_SKU) && (tpc == 0))
		pr_info("Booting Parker Sku\n");
	else if(denver_cores == 0)
		pr_info("Booting %s Sku\n", (tpc != 0)? "Reilly" : "Ohara");
	else
		pr_error("Booting unknown SKU\n\n");

fail:
	return;
}

void mb1_print_chip_info(void)
{
	uint32_t reg = 0;
	struct tegrabl_chip_info info;
	/* print bootrom patch version */
	reg = tegrabl_fuse_get_bootrom_patch_version();
	pr_info("Bootrom patch version : %u (%s)\n", reg,
			(((reg > 1) &&  (tegrabl_fuserdata_read(0xBA) == 0)) ?
			"incorrectly patched" : "correctly patched"));
	pr_info("ATE fuse revision : 0x%x\n", tegrabl_fuserdata_read(0x14));
	if (!fuse_is_nv_production_mode()) {
		if (tegrabl_fuse_read(FUSE_OPT_PRIV_SEC_EN, &reg, sizeof(reg)) != TEGRABL_NO_ERROR) {
			pr_error("OPT fuse priv sec_en read failed\n");
			return;
		}
		pr_info("OPT fuse priv sec_en : 0x%x\n", reg);
	}

	tegrabl_get_chip_info(&info);
	/* print major rev info of the chip */
	pr_info("Chip revision : %c%02u%c\n", info.major + 64, info.minor,
			(info.revision == 0) ? ' ' : info.revision + 'O');
	mb1_print_cpucore_info();
	reg = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_RAM_REPAIR_INDICATOR_0);
	pr_info("Ram repair fuse : 0x%x\n", reg);
}
#endif

tegrabl_error_t tegrabl_dbell_trigger(tegrabl_dbell_client_t source,
									  tegrabl_dbell_target_t target)
{
	uint32_t offset = 0;
	uint32_t val = 0;

	if (target >= TEGRABL_DBELL_TARGET_MAX ||
		source >= TEGRABL_DBELL_CLIENT_MAX) {
		pr_trace("Doorbell target/source not supported\n");
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 2);
	}

	offset = ((HSP_DBELL_1_TRIGGER_0 - HSP_DBELL_0_TRIGGER_0) * target);

	/* The source client has the following format :
	Bit 0 : Reserved
	Bit 1 :CCPLEX
	Bit 2 : DPMU
	Bit 3 : BPMP
	Bit 4 : SPE
	Bit 5 : SCE
	Bit 6 : DMA
	Bit 7 : TSECA
	Bit 8 : TSECB
	Bit 9 : JTAGM
	Bit 10 : CSITE
	Bit 11 : APE
	Bit 12 : PEATRANS
	Bit 13 - Bit 15 : Reserved
	Bit 16 - Bit 31 : Same strucutre as the 16 LSB
	*/

	val = TOP0_DBELL_READ(HSP_DBELL_0_ENABLE_0 + offset);
	val |= ((1UL << source) | (1UL << (source + 16UL)));
	TOP0_DBELL_WRITE(HSP_DBELL_0_ENABLE_0 + offset, val);

	TOP0_DBELL_WRITE(HSP_DBELL_0_TRIGGER_0 + offset, 0x1); /* any value is ok */
	TOP0_DBELL_READ(HSP_DBELL_0_RAW_0 + offset);
	pr_debug("Triggered doorbell- source:%u, target: %u\n", source, target);

	return TEGRABL_NO_ERROR;
}

uint32_t tegrabl_dbell_ack(tegrabl_dbell_client_t recipient)
{
	uint32_t ack = 0;
	uint32_t mask = 0;
	uint32_t timeout = 0;

	mask = mask | ((1UL << recipient) | (1UL << (recipient + 16UL)));

	/* poll till pending is cleared */
	timeout = DBELL_ACK_TIMEOUT_US;
	while (timeout != 0U) {
		ack = TOP0_DBELL_READ(HSP_DBELL_0_PENDING_0);
		if ((ack & mask) == 0U) {
			break;
		}
		tegrabl_udelay(DBELL_ACK_POLL_INTERVAL_US);
		timeout -= DBELL_ACK_POLL_INTERVAL_US;
	}

	return ack;
}

bool tegrabl_is_wdt_enable(void)
{
	uint32_t reg_val = 0;
	uint32_t odmdata_wdt = 0;
	uint32_t halt_in_fiq = 0;

	/* Get tegra-ap-wdt bit of odmdata 0:Tegra AP watchdog disable
	and value of HALT_IN_FIQ */
	odmdata_wdt = tegrabl_odmdata_get();
	reg_val = PMC_READ(RAMDUMP_CTL_STATUS);
	halt_in_fiq = NV_DRF_VAL(PMC_IMPL, RAMDUMP_CTL_STATUS, HALT_IN_FIQ,
				reg_val);

	if (fuse_is_nv_production_mode()) {
		return ((odmdata_wdt & TEGRA_WDT_MASK) != 0U);
	}

	return ((odmdata_wdt & TEGRA_WDT_MASK) != 0U) && (halt_in_fiq == 0U);
}

tegrabl_binary_type_t tegrabl_get_kernel_type(void)
{
	bool boot_recovery_kernel = false;
	tegrabl_binary_type_t bin_type;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_get_pmc_scratch0_flag(TEGRABL_PMC_SCRATCH0_FLAG_BOOT_RECOVERY_KERNEL,
								&boot_recovery_kernel);
	if (err != TEGRABL_NO_ERROR) {
		pr_info("Failed to get scratch0_flag\n");
	}
	if (boot_recovery_kernel == false) {
		bin_type = TEGRABL_BINARY_KERNEL;
		pr_info("Kernel type = Normal\n");
	} else {
		bin_type = TEGRABL_BINARY_RECOVERY_KERNEL;
		pr_info("Kernel type = Recovery\n");
	}
	return bin_type;
}

tegrabl_error_t tegrabl_register_prod_settings(uint32_t *prod_settings,
		uint32_t num_settings)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t i;
	uint32_t controller_id;
	uint32_t controller_instance;
	uint32_t num_configs;
	static tegrabl_error_t (*register_function[])(uint32_t instance,
			uint32_t aux_data, uint32_t *ptr, uint32_t num_configs) = {
		NULL,
#if defined(CONFIG_ENABLE_I2C)
		tegrabl_i2c_register_prod_settings,
#else
		NULL,
#endif

#if defined(CONFIG_ENABLE_SPI)
	tegrabl_spi_register_prod_settings,
#else
		NULL,
#endif
	};

	if (num_settings == 0U) {
		goto done;
	}

	if (prod_settings == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto done;
	}

	for (i = 0; i < num_settings; i++) {
		controller_instance = BITFIELD_GET(prod_settings[0],
				CONFIG_PROD_CONTROLLER_SHIFT, 0);
		controller_id = BITFIELD_GET(prod_settings[0],
				CONFIG_PROD_CONTROLLER_SHIFT, CONFIG_PROD_CONTROLLER_SHIFT);
		num_configs = prod_settings[2];

		if (controller_id >= ARRAY_SIZE(register_function) ||
			(register_function[controller_id] == NULL)) {
			prod_settings += (num_configs + 1U) * 3U;
			continue;
		}

		err = register_function[controller_id](
				controller_instance, prod_settings[1], prod_settings + 3,
				num_configs);
		if (err != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(err);
			return err;
		}
		prod_settings += (num_configs + 1) * 3;
	}

done:
	return err;
}

void tegrabl_set_boot_error_scratch(uint32_t val)
{
	SCRATCH_WRITE(SCRATCH_8, val);
	pr_debug("Boot error scratch register set to 0x%08x\n", val);
}

uint32_t tegrabl_get_boot_error(void)
{
	return SCRATCH_READ(SCRATCH_8);
}

bool tegrabl_is_ufs_enable(void)
{
	uint32_t odmdata = 0;

	odmdata = tegrabl_odmdata_get();
	if ((odmdata & 0x10000000U) != 0U) {
		return true;
	} else {
		return false;
	}
}

uint32_t tegrabl_get_hsm_reset_reason(void)
{
	return SCRATCH_READ(SCRATCH_6);
}

uint32_t tegrabl_get_bad_page_number(void)
{
	return SCRATCH_READ(SCRATCH_7);
}

#define FUSE_ECID_MAX_SIZE 4U /* in Bytes */
tegrabl_error_t tegrabl_get_ecid_str(char *ecid_str, uint32_t size)
{
	tegrabl_error_t err;
	uint32_t ecid[FUSE_ECID_MAX_SIZE];
	uint32_t ecid_size;
	uint32_t *ptr;

	if (ecid_str == NULL) {
		pr_error("Invalid ECID addr\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto done;
	}

	err = tegrabl_fuse_query_size(FUSE_UID, &ecid_size);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to query size of ECID\n");
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}
	if ((ecid_size > (FUSE_ECID_MAX_SIZE * sizeof(uint32_t))) || (size < (ecid_size * 2U))) {
		pr_error("Not enough buffer for ECID\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto done;
	}

	err = tegrabl_fuse_read(FUSE_UID, ecid, ecid_size);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to read ECID\n");
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	/* Transfer ECID to string */
	for (ptr = ecid + ecid_size / sizeof(uint32_t) - 1; ptr >= ecid; ptr--) {
		tegrabl_snprintf(ecid_str, size, "%s%08x", ecid_str, *ptr);
	}

done:
	return err;
}
