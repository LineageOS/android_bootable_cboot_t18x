/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All Rights Reserved.
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
#include <tegrabl_i2c.h>
#include <tegrabl_spi.h>
#include <string.h>
#include <tegrabl_goldenreg.h>

#define CONFIG_PROD_CONTROLLER_SHIFT 16

#if defined(CONFIG_GR_SUPPORT)
static uint32_t golden_regs_mb1[] = {
#include <golden_reg_mb1.h>
};

static uint32_t golden_regs_mb2[] = {
#include <golden_reg_mb2.h>
};
#endif

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

#define DBELL_ACK_TIMEOUT_US 1000000 /* us */
#define DBELL_ACK_POLL_INTERVAL_US 5 /* us */

#define RECOVERY_BOOT_CHAIN_VALUE 0xDEADBEEF
#define RECOVERY_BOOT_CHAIN_MASK  0xFFFFFFFF

uint32_t read_miscreg_strap(enum tegrabl_strap_field fld)
{
	static uint32_t boot_dev_val;
	static uint32_t ram_code_val;
	static bool is_strap_read;

	/* Read strap boot sel from SECURE_RSV3_SCRATCH_0 */
	if (!is_strap_read) {
		uint32_t strap_val = NV_READ32(NV_ADDRESS_MAP_SCRATCH_BASE +
				SCRATCH_SECURE_RSV3_SCRATCH_0);

		/* store the values and keep it future references */
		boot_dev_val = (enum tegrabl_fuse_boot_dev)(NV_DRF_VAL(MISCREG_STRAP,
						STRAPPING_OPT_A,
						BOOT_SELECT,
						strap_val));

		/* use lower 2 bits for RAM_CODE */
		ram_code_val = (NV_DRF_VAL(MISCREG_STRAP,
						STRAPPING_OPT_A,
						RAM_CODE,
						strap_val)) & 0x3;

		is_strap_read = true;
	}

	switch (fld) {
	case BOOT_SELECT_FIELD:
		return boot_dev_val;
	case RAM_CODE_FIELD:
		return ram_code_val;
	default:
		break;
	}

	return 0;
}

tegrabl_error_t tegrabl_soc_get_bootdev(
		tegrabl_storage_type_t *device, uint32_t *instance)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	enum tegrabl_fuse_boot_dev fdev;

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
		fdev == TEGRABL_FUSE_BOOT_DEV_RESVD_4) {
		/* Read boot dev value from fuse */
		err = tegrabl_fuse_read(FUSE_SEC_BOOTDEV, (uint32_t *)&fdev,
			sizeof(enum tegrabl_fuse_boot_dev));
		if (TEGRABL_NO_ERROR != err) {
			TEGRABL_SET_HIGHEST_MODULE(err);
			pr_error("Failed to read sec boot device from fuse\n");
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
		goto fail;
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

tegrabl_error_t tegrabl_get_rst_status(enum tegrabl_rst_source *rst_source,
									   enum tegrabl_rst_level *rst_level)
{
	uint32_t reg_val = 0;

	if (!rst_source && !rst_level)
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);

	reg_val = PMC_READ(RST_STATUS);

	if (rst_source)
		*rst_source = NV_DRF_VAL(PMC_IMPL, RST_STATUS, RST_SOURCE, reg_val);

	if (rst_level)
		*rst_level = NV_DRF_VAL(PMC_IMPL, RST_STATUS, RST_LEVEL, reg_val);

	return TEGRABL_NO_ERROR;
}

bool tegrabl_rst_is_sc8_exit(void)
{
	uint32_t rst_source;

	if (tegrabl_get_rst_status(&rst_source, NULL) != TEGRABL_NO_ERROR)
		return false;

	if (rst_source == RST_SOURCE_SC7)
		return ((PMC_READ(SC7_CONFIG) & 0x1) == 1) ? true : false;

	return false;
}

tegrabl_error_t tegrabl_enable_soc_therm(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_car_clk_enable(TEGRABL_MODULE_SOC_THERM, 0, NULL);
	if (err != TEGRABL_NO_ERROR)
		return err;

	err = tegrabl_car_rst_clear(TEGRABL_MODULE_SOC_THERM, 0);
	if (err != TEGRABL_NO_ERROR)
		return err;

	return err;
}

void tegrabl_clear_sec_scratch_bl(void)
{
	uint32_t rst_source, rst_level;
	uint32_t reg_val = 0;

#define RECOVERY_MODE		(1 << 31)
#define BOOTLOADER_MODE		(1 << 30)
#define FORCED_RECOVERY_MODE	(1 << 1)

	if (tegrabl_get_rst_status(&rst_source, &rst_level) != TEGRABL_NO_ERROR)
		return;

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
	pr_debug("Setting recovery flag\n");
	tegrabl_set_pmc_scratch0_flag(
			TEGRABL_PMC_SCRATCH0_FLAG_FORCED_RECOVERY, true);

	pr_debug("Clearing RSV9 scratch register\n");
	SCRATCH_WRITE(SECURE_RSV9_SCRATCH_0, 0);

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
	if (tegrabl_rst_is_sc8_exit())
		return;

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
		enum tegrabl_scratch0_flag flag, bool set)
{
	uint32_t reg;

	reg = SCRATCH_READ(SCRATCH0_0);
	switch (flag) {
	case TEGRABL_PMC_SCRATCH0_FLAG_FORCED_RECOVERY:
	case TEGRABL_PMC_SCRATCH0_FLAG_BOOT_RECOVERY_KERNEL:
	case TEGRABL_PMC_SCRATCH0_FLAG_FASTBOOT:
		if (set)
			reg |= (1 << flag);
		else
			reg &= ~(1 << flag);

		SCRATCH_WRITE(SCRATCH0_0, reg);
		break;
	default:
		pr_error("flag %u not handled\n", flag);
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_get_pmc_scratch0_flag(
	enum tegrabl_scratch0_flag flag, bool *is_set)
{
	uint32_t reg;
	if (!is_set)
		return TEGRABL_ERR_INVALID;

	reg = SCRATCH_READ(SCRATCH0_0);
	switch (flag) {
	case TEGRABL_PMC_SCRATCH0_FLAG_FORCED_RECOVERY:
	case TEGRABL_PMC_SCRATCH0_FLAG_BOOT_RECOVERY_KERNEL:
	case TEGRABL_PMC_SCRATCH0_FLAG_FASTBOOT:
		*is_set = (reg & (1 << flag)) ? true : false;
		break;
	default:
		pr_critical("flag %u not handled\n", flag);
	}
	return TEGRABL_NO_ERROR;
}
tegrabl_error_t tegrabl_set_soc_core_voltage(uint32_t soc_mv)
{
	TEGRABL_UNUSED(soc_mv);
	/* Dummy function */
	return TEGRABL_NO_ERROR;
}

enum tegrabl_boot_chain_type tegrabl_get_boot_chain_type(void)
{
	uint32_t reg = 0;

	reg = SCRATCH_READ(SCRATCH_99);

	if ((reg & RECOVERY_BOOT_CHAIN_MASK) == RECOVERY_BOOT_CHAIN_VALUE)
		return TEGRABL_BOOT_CHAIN_RECOVERY;

	return TEGRABL_BOOT_CHAIN_PRIMARY;
}

void tegrabl_set_boot_chain_type(enum tegrabl_boot_chain_type boot_chain)
{
	uint32_t reg = 0;

	reg = SCRATCH_READ(SCRATCH_99);
	reg = reg & ~RECOVERY_BOOT_CHAIN_MASK;

	if (boot_chain == TEGRABL_BOOT_CHAIN_RECOVERY)
		reg = reg | RECOVERY_BOOT_CHAIN_VALUE;

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
	while (1)
		;
}

void tegrabl_get_chip_info(struct tegrabl_chip_info *info)
{
	uint32_t reg, rev;

	if (!info)
		return;

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

	if (tegrabl_fuse_read(FUSE_ENABLED_CPU_CORES,
		&cores_mask, sizeof(uint32_t))) {
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
		if (tegrabl_fuse_read(FUSE_OPT_PRIV_SEC_EN, &reg, sizeof(reg))) {
			pr_error("OPT fuse priv sec_en read failed\n");
			return;
		}
		pr_info("OPT fuse priv sec_en : 0x%x\n", reg);
	}

	tegrabl_get_chip_info(&info);
	/* print major rev info of the chip */
	pr_info("chip revision : %c%02u%c\n", info.major + 64, info.minor,
			(info.revision == 0) ? ' ' : info.revision + 'O');
	mb1_print_cpucore_info();
	reg = NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_RAM_REPAIR_INDICATOR_0);
	pr_info("Ram repair fuse : 0x%x\n", reg);
}
#endif

tegrabl_error_t tegrabl_dbell_trigger(enum tegrabl_dbell_client source,
									  enum tegrabl_dbell_target target)
{
	uint32_t offset = 0;
	uint32_t val = 0;

	if (target >= TEGRABL_DBELL_TARGET_MAX ||
		source >= TEGRABL_DBELL_CLIENT_MAX) {
		pr_error("Doorbell target/source not supported\n");
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
	val |= ((1 << source) | (1 << (source + 16)));
	TOP0_DBELL_WRITE(HSP_DBELL_0_ENABLE_0 + offset, val);

	TOP0_DBELL_WRITE(HSP_DBELL_0_TRIGGER_0 + offset, 0x1); /* any value is ok */
	TOP0_DBELL_READ(HSP_DBELL_0_RAW_0 + offset);
	pr_debug("Triggered doorbell- source:%u, target: %u\n", source, target);

	return TEGRABL_NO_ERROR;
}

uint32_t tegrabl_dbell_ack(enum tegrabl_dbell_client recipient)
{
	uint32_t ack = 0;
	uint32_t mask = 0;
	uint32_t timeout = 0;

	mask = mask | ((1 << recipient) | (1 << (recipient + 16)));

	/* poll till pending is cleared */
	timeout = DBELL_ACK_TIMEOUT_US;
	while (timeout) {
		ack = TOP0_DBELL_READ(HSP_DBELL_0_PENDING_0);
		if ((ack & mask) == 0)
			break;
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

	if (fuse_is_nv_production_mode())
		return odmdata_wdt & TEGRA_WDT_MASK;

	return (odmdata_wdt & TEGRA_WDT_MASK) && !halt_in_fiq;
}

enum tegrabl_binary_type tegrabl_get_kernel_type(void)
{
	bool boot_recovery_kernel = false;
	enum tegrabl_binary_type bin_type;

	tegrabl_get_pmc_scratch0_flag(
								 TEGRABL_PMC_SCRATCH0_FLAG_BOOT_RECOVERY_KERNEL,
								 &boot_recovery_kernel);

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

	if (!num_settings)
		goto done;

	if (!prod_settings) {
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
			!register_function[controller_id]) {
			prod_settings += (num_configs + 1) * 3;
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

#if defined(CONFIG_GR_SUPPORT)
void tegrabl_dump_golden_regs(enum tegrabl_gr_state state,
						uint64_t start)
{
	uint32_t i = 0;
	uint32_t *golden_regs;
	uint32_t golden_reg_array_size;
	struct tegrabl_gr_hdr *tegrabl_gr =
		(struct tegrabl_gr_hdr *)(uintptr_t)(start);
	uint32_t start_addr_relative;
	struct tegrabl_gr_value *gr_state_start;
	static bool first_time = true;

	if (first_time) {
		memset((void *)(uintptr_t)start, 0,
				sizeof(struct tegrabl_gr_hdr));
		first_time = false;
	}
	if (state == TEGRABL_GR_MB1) {
		golden_regs = golden_regs_mb1;
		golden_reg_array_size = ARRAY_SIZE(golden_regs_mb1);
	} else {
		golden_regs = golden_regs_mb2;
		golden_reg_array_size = ARRAY_SIZE(golden_regs_mb2);
	}
	/* Get the start location in the carveout by adding all the sizes.  For
	 * simplicity not added if condition. As all size will be zero by
	 * default
	 */
	start_addr_relative = tegrabl_gr->mb1_size + tegrabl_gr->mb2_size +
					tegrabl_gr->cpu_bl_size;

	/* no golden regs are added or carveout over-flow*/
	if ((golden_reg_array_size <= 1) || ((start_addr_relative + golden_reg_array_size *
				sizeof(struct tegrabl_gr_value) +
				sizeof(struct tegrabl_gr_hdr)) >
				TEGRABL_GR_CARVEOUT_SIZE)) {
		pr_info("No golden regs to dump");
		if (state == TEGRABL_GR_MB1) {
			tegrabl_gr->mb1_offset = 0;
			tegrabl_gr->mb1_size = 0;
		} else {
			tegrabl_gr->mb2_offset = 0;
			tegrabl_gr->mb2_size = 0;
		}
		return;
	}

	gr_state_start = (struct tegrabl_gr_value *)(uintptr_t)
		(start + sizeof(struct tegrabl_gr_hdr) + start_addr_relative);
	pr_info("Dumping GR Value for stage %d\n", state);

	for (i = 0; i < golden_reg_array_size; i++) {
		gr_state_start[i].gr_address = golden_regs[i];
		gr_state_start[i].gr_value = NV_READ32(golden_regs[i]);
	}
	if (state == TEGRABL_GR_MB1) {
		tegrabl_gr->mb1_offset = start_addr_relative;
		tegrabl_gr->mb1_size = golden_reg_array_size *
			sizeof(struct tegrabl_gr_value);
	} else {
		tegrabl_gr->mb2_offset = start_addr_relative;
		tegrabl_gr->mb2_size = golden_reg_array_size *
			sizeof(struct tegrabl_gr_value);
	}
}
#endif

void tegrabl_set_boot_error_scratch(uint32_t val)
{
	SCRATCH_WRITE(SCRATCH_8, val);
	pr_debug("Boot error Scratch register set to %u\n", val);
}

uint32_t tegrabl_get_boot_error(void)
{
	return SCRATCH_READ(SCRATCH_8);
}

bool tegrabl_is_ufs_enable(void)
{
	uint32_t odmdata = 0;

	odmdata = tegrabl_odmdata_get();
	if ((odmdata & 0x10000000) != 0)
		return true;
	else
		return false;
}

uint32_t tegrabl_get_hsm_reset_reason(void)
{
	return SCRATCH_READ(SCRATCH_6);
}

uint32_t tegrabl_get_bad_page_number(void)
{
	return SCRATCH_READ(SCRATCH_7);
}
