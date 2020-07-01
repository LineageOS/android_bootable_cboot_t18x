/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_FUSE

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_drf.h>
#include <tegrabl_debug.h>
#include <tegrabl_io.h>
#include <tegrabl_clock.h>
#include <tegrabl_fuse.h>
#include <arfuse.h>
#include <tegrabl_timer.h>
#include <tegrabl_fuse_bitmap.h>
#include <tegrabl_malloc.h>
#include <tegrabl_soc_misc.h>

/* Stores the base address of the fuse module */
static uintptr_t fuse_base_address = NV_ADDRESS_MAP_FUSE_BASE;

#define NV_FUSE_READ(reg) NV_READ32((fuse_base_address + reg))
#define NV_FUSE_WRITE(reg, data) NV_WRITE32((fuse_base_address + reg), data)
#define FUSE_DISABLEREGPROGRAM_0_VAL_MASK 0x1
#define FUSE_STROBE_PROGRAMMING_PULSE 5

static uint32_t fuse_word;

#define write_fuse_word_0(name, data)								\
{																	\
	fuse_word =	(name##_ADDR_0_MASK & data) <<						\
		(name##_ADDR_0_SHIFT);										\
}																	\

#define write_fuse_word_1(name, data)								\
{																	\
	fuse_word = (name##_ADDR_1_MASK & data) >>						\
		(name##_ADDR_1_SHIFT);										\
}																	\

static bool is_fuse_write_disabled(void)
{
	uint32_t val;

	val = NV_FUSE_READ(FUSE_DISABLEREGPROGRAM_0);

	if ((val & FUSE_DISABLEREGPROGRAM_0_VAL_MASK) != 0)
		return true;
	else
		return false;
}

static tegrabl_error_t program_fuse_strobe(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t oscillator_frequency_khz;
	uint32_t oscillator_frequency;
	uint32_t strobe_pulse = FUSE_STROBE_PROGRAMMING_PULSE;
	uint32_t strobe_width;
	uint32_t data;

	err = tegrabl_car_get_osc_freq_khz(&oscillator_frequency_khz);
	if (err)
		goto fail;

	oscillator_frequency = oscillator_frequency_khz * 1000;

	strobe_width = (oscillator_frequency * strobe_pulse) / (1000 * 1000);

	/* Program FUSE_FUSETIME_PGM2_0 with strobe_width */
	data = NV_FUSE_READ(FUSE_FUSETIME_PGM2_0);
	data = NV_FLD_SET_DRF_NUM(FUSE, FUSETIME_PGM2, FUSETIME_PGM2_TWIDTH_PGM,
		strobe_width, data);
	NV_FUSE_WRITE(FUSE_FUSETIME_PGM2_0, data);

fail:
	if (err)
		pr_error("error = 0x%x in program_fuse_strobe\n", err);
	return err;
}

static void fuse_assert_pd(bool is_assert)
{
	uint32_t data;
	bool pd_ctrl = false;

	data = NV_FUSE_READ(FUSE_FUSECTRL_0);
	pd_ctrl = NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_PD_CTRL, data);

	if (is_assert) {
		if (pd_ctrl) {
			return;
		} else {
			tegrabl_udelay(1);
			data = NV_FLD_SET_DRF_NUM(FUSE, FUSECTRL,
				FUSECTRL_PD_CTRL, 0x1, data);
			NV_FUSE_WRITE(FUSE_FUSECTRL_0, data);
		}
	} else {
		if (!pd_ctrl) {
			return;
		} else {
			data = NV_FLD_SET_DRF_NUM(FUSE, FUSECTRL,
				FUSECTRL_PD_CTRL, 0x0, data);
			NV_FUSE_WRITE(FUSE_FUSECTRL_0, data);
			data = NV_FUSE_READ(FUSE_FUSECTRL_0);
			tegrabl_udelay(1);
		}
	}
}

static tegrabl_error_t fuse_write_pre_process(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t data;

	/* Confirm fuse option write access hasn't already
	 * been permanently disabled
	 */
	if (is_fuse_write_disabled()) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	/* Enable software writes to fuse registers.*/
	data = NV_FUSE_READ(FUSE_WRITE_ACCESS_SW_0);
	data = NV_FLD_SET_DRF_NUM(FUSE, WRITE_ACCESS_SW,
		WRITE_ACCESS_SW_CTRL, 0x1, data);
	NV_FUSE_WRITE(FUSE_WRITE_ACCESS_SW_0, data);


	/* Set the fuse strobe programming width */
	err = program_fuse_strobe();
	if (err)
		goto fail;

	/* Increase SOC core voltage to at least 0.85V and wait it to be stable */
	err = tegrabl_set_soc_core_voltage(850);
	if (err)
		goto fail;

	/* Disable fuse mirroring and set PD to 0, wait for the required setup time
	 * This insures that the fuse macro is not power gated
	 */
	tegrabl_fuse_program_mirroring(false);
	fuse_assert_pd(false);

	/* Make sure the fuse burning voltage is present and stable
	 * (Assuming this is already set)
	 */

	/* Confirm the fuse wrapper's state machine is idle */
	data = NV_FUSE_READ(FUSE_FUSECTRL_0);
	data = NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_STATE, data);
	if (data != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto fail;
	}

fail:
	if (err)
		pr_error("error = 0x%x in fuse_write_pre_process\n", err);
	return err;
}

static void fuse_write_post_process(void)
{
	uint32_t data;

	/* Enable back fuse mirroring and set PD to 1,
	 * wait for the required setup time
	 */
	tegrabl_fuse_program_mirroring(true);
	fuse_assert_pd(true);

	/* If desired the newly burned raw fuse values can take effect without
	 * a reset, cold boot, or SC7LP0 resume
	 */
	data = NV_FUSE_READ(FUSE_FUSECTRL_0);
	data = NV_FLD_SET_DRF_DEF(FUSE, FUSECTRL, FUSECTRL_CMD, SENSE_CTRL, data);
	NV_FUSE_WRITE(FUSE_FUSECTRL_0, data);

	/* Wait at least 400ns as per IAS. Waiting 50us here to make sure h/w is
	 * stable and eliminate any issue with our timer driver. Since fuse burning
	 * is invoked rarely, KPIs doesn't matter here.
	 */
	tegrabl_udelay(50);

	/* Poll FUSE_FUSECTRL_0_FUSECTRL_STATE until it reads back STATE_IDLE */
	do {
		data = NV_FUSE_READ(FUSE_FUSECTRL_0);
		data = NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_STATE, data);
	} while (data != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);

	/* Simultaneously set FUSE_PRIV2INTFC_START_0_PRIV2INTFC_START_DATA &
	 * _PRIV2INTFC_SKIP_RECORDS
	 */
	data = NV_FUSE_READ(FUSE_PRIV2INTFC_START_0);
	data = NV_FLD_SET_DRF_NUM(FUSE, PRIV2INTFC_START,
								PRIV2INTFC_START_DATA, 1, data);
	data = NV_FLD_SET_DRF_NUM(FUSE, PRIV2INTFC_START,
								PRIV2INTFC_SKIP_RECORDS, 1, data);
	NV_FUSE_WRITE(FUSE_PRIV2INTFC_START_0, data);

	/* Wait at least 400ns as per IAS. Waiting 50us here to make sure h/w is
	 * stable and eliminate any issue with our timer driver. Since fuse burning
	 * is invoked rarely, KPIs doesn't matter here.
	 */
	tegrabl_udelay(50);

	/* Poll FUSE_FUSECTRL_0 until both FUSECTRL_FUSE_SENSE_DONE is set,
	 * and FUSECTRL_STATE is STATE_IDLE
	 */
	do {
		data = NV_FUSE_READ(FUSE_FUSECTRL_0);
		data = NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_FUSE_SENSE_DONE, data);
	} while (!data);

	do {
		data = NV_FUSE_READ(FUSE_FUSECTRL_0);
		data = NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_STATE, data);
	} while (data != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);

}

static void fuse_initiate_burn(void)
{
	uint32_t data;

	/* Initiate the fuse burn */
	data = NV_FUSE_READ(FUSE_FUSECTRL_0);
	data = NV_FLD_SET_DRF_DEF(FUSE, FUSECTRL, FUSECTRL_CMD, WRITE, data);
	NV_FUSE_WRITE(FUSE_FUSECTRL_0, data);

	/* Wait at least 400ns as per IAS. Waiting 50us here to make sure h/w is
	 * stable and eliminate any issue with our timer driver. Since fuse burning
	 * is invoked rarely, KPIs doesn't matter here.
	 */
	tegrabl_udelay(50);

	/* Wait for the fuse burn to complete */
	do {
		data = NV_FUSE_READ(FUSE_FUSECTRL_0);
		data = NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_STATE, data);
	} while (data != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);


	/* check that the correct data has been burned correctly
	 * by reading back the data
	 */
	do {
		data = NV_FUSE_READ(FUSE_FUSECTRL_0);
		data = NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_STATE, data);
	} while (data != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);

	data = NV_FUSE_READ(FUSE_FUSECTRL_0);
	data = NV_FLD_SET_DRF_DEF(FUSE, FUSECTRL, FUSECTRL_CMD, READ, data);
	NV_FUSE_WRITE(FUSE_FUSECTRL_0, data);

	/* Wait at least 400ns as per IAS. Waiting 50us here to make sure h/w is
	 * stable and eliminate any issue with our timer driver. Since fuse burning
	 * is invoked rarely, KPIs doesn't matter here.
	 */
	tegrabl_udelay(50);

	do {
		data = NV_FUSE_READ(FUSE_FUSECTRL_0);
		data = NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_STATE, data);
	} while (data != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);

	data = NV_FUSE_READ(FUSE_FUSERDATA_0);
}

static tegrabl_error_t fuse_burn(uint32_t addr)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (fuse_word == 0)
		goto fail;

	err = fuse_write_pre_process();
	if (err)
		goto fail;

	/* Set the desired fuse dword address */
	NV_FUSE_WRITE(FUSE_FUSEADDR_0, addr);

	/* Set the desired fuses to burn */
	NV_FUSE_WRITE(FUSE_FUSEWDATA_0, fuse_word);

	fuse_initiate_burn();

	fuse_write_post_process();
fail:
	if (err)
		pr_error("error = 0x%x in fuse_burn\n", err);
	return err;
}

static tegrabl_error_t fuse_set_macro_and_burn(
	uint32_t fuse_type, uint32_t *buffer, uint32_t size)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t temp_size = 0;
	uint32_t *temp_buffer = NULL;
	uint32_t i = 0;

	if (!buffer) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
		goto fail;
	}

	err = tegrabl_fuse_query_size(fuse_type, &temp_size);
	if (err)
		goto fail;

	if (temp_size > size) {
		err = TEGRABL_ERROR(TEGRABL_ERR_TOO_SMALL, 0);
		goto fail;
	}

	temp_buffer = tegrabl_malloc(temp_size);
	if (temp_buffer == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}
	memset(temp_buffer, 0, temp_size);

	err = tegrabl_fuse_read(fuse_type, temp_buffer, size);
	if (err)
		goto fail;

	while (i < (size >> 2)) {
		buffer[i] = buffer[i] ^ temp_buffer[i];
		if (buffer[i] & temp_buffer[i]) {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 4);
			pr_warn("read fuse value = 0x%0x\n", temp_buffer[i]);
			pr_error("Error = %d in fuse_write: tried to set 1 to 0\n", err);
			goto fail;
		}
		i++;
	}

	switch (fuse_type) {
	case FUSE_HYPERVOLTAGING:
		write_fuse_word_0(FUSE_HYPERVOLTAGING, buffer[0])
		err = fuse_burn(FUSE_HYPERVOLTAGING_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_HYPERVOLTAGING, buffer[0])
		err = fuse_burn(FUSE_HYPERVOLTAGING_ADDR_1);
		if (err)
			goto fail;
		break;
	case FUSE_RESERVED_CALIB0:
		write_fuse_word_0(FUSE_RESERVED_CALIB0, buffer[0])
		err = fuse_burn(FUSE_RESERVED_CALIB0_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_RESERVED_CALIB0, buffer[0])
		err = fuse_burn(FUSE_RESERVED_CALIB0_ADDR_1);
		if (err)
			goto fail;
		break;
#if defined(CONFIG_ENABLE_FSKP) || defined(CONFIG_ENABLE_FUSING)
	case FUSE_SKU_INFO:
		write_fuse_word_0(FUSE_SKU_INFO, buffer[0])
		err = fuse_burn(FUSE_SKU_INFO_ADDR_0);
		if (err)
			goto fail;
		break;
	case FUSE_PRODUCTION_MODE:
		write_fuse_word_0(FUSE_PRODUCTION_MODE, buffer[0])
		err = fuse_burn(FUSE_PRODUCTION_MODE_ADDR_0);
		if (err)
			goto fail;
		break;
	case FUSE_SECURITY_MODE:
		write_fuse_word_0(FUSE_SECURITY_MODE, buffer[0])
		err = fuse_burn(FUSE_SECURITY_MODE_ADDR_0);
		if (err)
			goto fail;
		write_fuse_word_0(FUSE_SECURITY_MODE_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_SECURITY_MODE_REDUNDANT_ADDR_0);
		if (err)
			goto fail;
		break;
	case FUSE_ODM_LOCK:
		write_fuse_word_0(FUSE_ODM_LOCK, buffer[0])
		err = fuse_burn(FUSE_ODM_LOCK_ADDR_0);
		if (err)
			goto fail;
		write_fuse_word_0(FUSE_ODM_LOCK_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_ODM_LOCK_REDUNDANT_ADDR_0);
		if (err)
			goto fail;
		break;
	case FUSE_ARM_JTAG_DIS:
		write_fuse_word_0(FUSE_ARM_JTAG_DIS, buffer[0])
		err = fuse_burn(FUSE_ARM_JTAG_DIS_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_ARM_JTAG_DIS_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_ARM_JTAG_DIS_REDUNDANT_ADDR_0);
		if (err)
			goto fail;
		break;
	case FUSE_BOOT_SECURITY_INFO:
		write_fuse_word_0(FUSE_BOOT_SECURITY_INFO, buffer[0])
		err = fuse_burn(FUSE_BOOT_SECURITY_INFO_ADDR_0);
		if (err)
			goto fail;
		write_fuse_word_0(FUSE_BOOT_SECURITY_INFO_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_BOOT_SECURITY_INFO_REDUNDANT_ADDR_0);
		if (err)
			goto fail;
		break;
	case FUSE_RESERVED_ODM0:
		write_fuse_word_0(FUSE_RESERVED_ODM0, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM0_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_RESERVED_ODM0, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM0_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_RESERVED_ODM0_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM0_REDUNDANT_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_RESERVED_ODM0_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM0_REDUNDANT_ADDR_1);
		if (err)
			goto fail;
		break;
	case FUSE_RESERVED_ODM1:
		write_fuse_word_0(FUSE_RESERVED_ODM1, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM1_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_RESERVED_ODM1, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM1_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_RESERVED_ODM1_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM1_REDUNDANT_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_RESERVED_ODM1_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM1_REDUNDANT_ADDR_1);
		if (err)
			goto fail;
		break;
	case FUSE_RESERVED_ODM2:
		write_fuse_word_0(FUSE_RESERVED_ODM2, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM2_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_RESERVED_ODM2, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM2_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_RESERVED_ODM2_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM2_REDUNDANT_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_RESERVED_ODM2_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM2_REDUNDANT_ADDR_1);
		if (err)
			goto fail;
		break;
#endif
	case FUSE_RESERVED_ODM3:
		write_fuse_word_0(FUSE_RESERVED_ODM3, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM3_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_RESERVED_ODM3, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM3_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_RESERVED_ODM3_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM3_REDUNDANT_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_RESERVED_ODM3_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM3_REDUNDANT_ADDR_1);
		if (err)
			goto fail;
		break;
#if defined(CONFIG_ENABLE_FSKP) || defined(CONFIG_ENABLE_FUSING)
	case FUSE_RESERVED_ODM4:
		write_fuse_word_0(FUSE_RESERVED_ODM4, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM4_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_RESERVED_ODM4, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM4_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_RESERVED_ODM4_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM4_REDUNDANT_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_RESERVED_ODM4_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM4_REDUNDANT_ADDR_1);
		if (err)
			goto fail;
		break;
	case FUSE_RESERVED_ODM5:
		write_fuse_word_0(FUSE_RESERVED_ODM5, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM5_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_RESERVED_ODM5, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM5_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_RESERVED_ODM5_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM5_REDUNDANT_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_RESERVED_ODM5_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM5_REDUNDANT_ADDR_1);
		if (err)
			goto fail;
		break;
	case FUSE_RESERVED_ODM6:
		write_fuse_word_0(FUSE_RESERVED_ODM6, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM6_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_RESERVED_ODM6, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM6_ADDR_1);
		if (err)
			goto fail;
		write_fuse_word_0(FUSE_RESERVED_ODM6_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM6_REDUNDANT_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_RESERVED_ODM6_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM6_REDUNDANT_ADDR_1);
		if (err)
			goto fail;
		break;
	case FUSE_RESERVED_ODM7:
		write_fuse_word_0(FUSE_RESERVED_ODM7, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM7_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_RESERVED_ODM7, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM7_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_RESERVED_ODM7_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM7_REDUNDANT_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_RESERVED_ODM7_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM7_REDUNDANT_ADDR_1);
		if (err)
			goto fail;
		break;
	case FUSE_PKC_PUBKEY_HASH:
		write_fuse_word_0(FUSE_PUBLIC_KEY0, buffer[0])
		err = fuse_burn(FUSE_PUBLIC_KEY0_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_PUBLIC_KEY0, buffer[0])
		err = fuse_burn(FUSE_PUBLIC_KEY0_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_PUBLIC_KEY1, buffer[1])
		err = fuse_burn(FUSE_PUBLIC_KEY1_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_PUBLIC_KEY1, buffer[1])
		err = fuse_burn(FUSE_PUBLIC_KEY1_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_PUBLIC_KEY2, buffer[2])
		err = fuse_burn(FUSE_PUBLIC_KEY2_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_PUBLIC_KEY2, buffer[2])
		err = fuse_burn(FUSE_PUBLIC_KEY2_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_PUBLIC_KEY3, buffer[3])
		err = fuse_burn(FUSE_PUBLIC_KEY3_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_PUBLIC_KEY3, buffer[3])
		err = fuse_burn(FUSE_PUBLIC_KEY3_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_PUBLIC_KEY4, buffer[4])
		err = fuse_burn(FUSE_PUBLIC_KEY4_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_PUBLIC_KEY4, buffer[4])
		err = fuse_burn(FUSE_PUBLIC_KEY4_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_PUBLIC_KEY5, buffer[5])
		err = fuse_burn(FUSE_PUBLIC_KEY5_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_PUBLIC_KEY5, buffer[5])
		err = fuse_burn(FUSE_PUBLIC_KEY5_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_PUBLIC_KEY6, buffer[6])
		err = fuse_burn(FUSE_PUBLIC_KEY6_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_PUBLIC_KEY6, buffer[6])
		err = fuse_burn(FUSE_PUBLIC_KEY6_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_PUBLIC_KEY7, buffer[7])
		err = fuse_burn(FUSE_PUBLIC_KEY7_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_PUBLIC_KEY7, buffer[7])
		err = fuse_burn(FUSE_PUBLIC_KEY7_ADDR_1);
		if (err)
			goto fail;
		break;
	case FUSE_SECURE_BOOT_KEY:
		write_fuse_word_0(FUSE_PRIVATE_KEY0, buffer[0])
		err = fuse_burn(FUSE_PRIVATE_KEY0_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_PRIVATE_KEY0, buffer[0])
		err = fuse_burn(FUSE_PRIVATE_KEY0_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_PRIVATE_KEY1, buffer[1])
		err = fuse_burn(FUSE_PRIVATE_KEY1_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_PRIVATE_KEY1, buffer[1])
		err = fuse_burn(FUSE_PRIVATE_KEY1_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_PRIVATE_KEY2, buffer[2])
		err = fuse_burn(FUSE_PRIVATE_KEY2_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_PRIVATE_KEY2, buffer[2])
		err = fuse_burn(FUSE_PRIVATE_KEY2_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_PRIVATE_KEY3, buffer[3])
		err = fuse_burn(FUSE_PRIVATE_KEY3_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_PRIVATE_KEY3, buffer[3])
		err = fuse_burn(FUSE_PRIVATE_KEY3_ADDR_1);
		if (err)
			goto fail;
		break;
	case FUSE_KEK256:
		write_fuse_word_0(FUSE_KEK00, buffer[0])
		err = fuse_burn(FUSE_KEK00_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK00, buffer[0])
		err = fuse_burn(FUSE_KEK00_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_KEK01, buffer[1])
		err = fuse_burn(FUSE_KEK01_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK01, buffer[1])
		err = fuse_burn(FUSE_KEK01_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_KEK02, buffer[2])
		err = fuse_burn(FUSE_KEK02_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK02, buffer[2])
		err = fuse_burn(FUSE_KEK02_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_KEK03, buffer[3])
		err = fuse_burn(FUSE_KEK03_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK03, buffer[3])
		err = fuse_burn(FUSE_KEK03_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_KEK10, buffer[4])
		err = fuse_burn(FUSE_KEK10_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK10, buffer[4])
		err = fuse_burn(FUSE_KEK10_ADDR_1);
		if (err)
			goto fail;
		write_fuse_word_0(FUSE_KEK11, buffer[5])
		err = fuse_burn(FUSE_KEK11_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK11, buffer[5])
		err = fuse_burn(FUSE_KEK11_ADDR_1);
		if (err)
			goto fail;
		write_fuse_word_0(FUSE_KEK12, buffer[6])
		err = fuse_burn(FUSE_KEK12_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK12, buffer[6])
		err = fuse_burn(FUSE_KEK12_ADDR_1);
		if (err)
			goto fail;
		write_fuse_word_0(FUSE_KEK13, buffer[7])
		err = fuse_burn(FUSE_KEK13_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK13, buffer[7])
		err = fuse_burn(FUSE_KEK13_ADDR_1);
		if (err)
			goto fail;
		break;
	case FUSE_KEK0:
		write_fuse_word_0(FUSE_KEK00, buffer[0])
		err = fuse_burn(FUSE_KEK00_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK00, buffer[0])
		err = fuse_burn(FUSE_KEK00_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_KEK01, buffer[1])
		err = fuse_burn(FUSE_KEK01_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK01, buffer[1])
		err = fuse_burn(FUSE_KEK01_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_KEK02, buffer[2])
		err = fuse_burn(FUSE_KEK02_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK02, buffer[2])
		err = fuse_burn(FUSE_KEK02_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_KEK03, buffer[3])
		err = fuse_burn(FUSE_KEK03_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK03, buffer[3])
		err = fuse_burn(FUSE_KEK03_ADDR_1);
		if (err)
			goto fail;
		break;
	case FUSE_KEK1:
		write_fuse_word_0(FUSE_KEK10, buffer[0])
		err = fuse_burn(FUSE_KEK10_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK10, buffer[0])
		err = fuse_burn(FUSE_KEK10_ADDR_1);
		if (err)
			goto fail;
		write_fuse_word_0(FUSE_KEK11, buffer[1])
		err = fuse_burn(FUSE_KEK11_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK11, buffer[1])
		err = fuse_burn(FUSE_KEK11_ADDR_1);
		if (err)
			goto fail;
		write_fuse_word_0(FUSE_KEK12, buffer[2])
		err = fuse_burn(FUSE_KEK12_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK12, buffer[2])
		err = fuse_burn(FUSE_KEK12_ADDR_1);
		if (err)
			goto fail;
		write_fuse_word_0(FUSE_KEK13, buffer[3])
		err = fuse_burn(FUSE_KEK13_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK13, buffer[3])
		err = fuse_burn(FUSE_KEK13_ADDR_1);
		if (err)
			goto fail;
		break;
	case FUSE_KEK2:
		write_fuse_word_0(FUSE_KEK20, buffer[0])
		err = fuse_burn(FUSE_KEK20_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK20, buffer[0])
		err = fuse_burn(FUSE_KEK20_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_KEK21, buffer[1])
		err = fuse_burn(FUSE_KEK21_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK21, buffer[1])
		err = fuse_burn(FUSE_KEK21_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_KEK22, buffer[2])
		err = fuse_burn(FUSE_KEK22_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK22, buffer[2])
		err = fuse_burn(FUSE_KEK22_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_KEK23, buffer[3])
		err = fuse_burn(FUSE_KEK23_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_KEK23, buffer[3])
		err = fuse_burn(FUSE_KEK23_ADDR_1);
		if (err)
			goto fail;
		break;
	case FUSE_RESERVED_SW:
		write_fuse_word_0(FUSE_RESERVED_SW, buffer[0])
		err = fuse_burn(FUSE_RESERVED_SW_ADDR_0);
		if (err)
			goto fail;
		break;
	case FUSE_BOOT_DEVICE_SELECT:
		write_fuse_word_0(FUSE_BOOT_DEVICE_SELECT, buffer[0])
		err = fuse_burn(FUSE_BOOT_DEVICE_SELECT_ADDR_0);
		if (err)
			goto fail;
		break;
	case FUSE_SKIP_DEV_SEL_STRAPS:
		write_fuse_word_0(FUSE_SKIP_DEV_SEL_STRAPS, buffer[0])
		err = fuse_burn(FUSE_SKIP_DEV_SEL_STRAPS_ADDR_0);
		if (err)
			goto fail;
		break;
	case FUSE_BOOT_DEVICE_INFO:
		write_fuse_word_0(FUSE_BOOT_DEVICE_INFO, buffer[0])
		err = fuse_burn(FUSE_BOOT_DEVICE_INFO_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_BOOT_DEVICE_INFO, buffer[0])
		err = fuse_burn(FUSE_BOOT_DEVICE_INFO_ADDR_1);
		if (err)
			goto fail;
		break;
	case FUSE_ENDORSEMENT_KEY:
		write_fuse_word_0(FUSE_EK0, buffer[0])
		err = fuse_burn(FUSE_EK0_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_EK0, buffer[0])
		err = fuse_burn(FUSE_EK0_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_EK1, buffer[1])
		err = fuse_burn(FUSE_EK1_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_EK1, buffer[1])
		err = fuse_burn(FUSE_EK1_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_EK2, buffer[2])
		err = fuse_burn(FUSE_EK2_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_EK2, buffer[2])
		err = fuse_burn(FUSE_EK2_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_EK3, buffer[3])
		err = fuse_burn(FUSE_EK3_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_EK3, buffer[3])
		err = fuse_burn(FUSE_EK3_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_EK4, buffer[4])
		err = fuse_burn(FUSE_EK4_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_EK4, buffer[4])
		err = fuse_burn(FUSE_EK4_ADDR_1);
		if (err)
			goto fail;
		write_fuse_word_0(FUSE_EK5, buffer[5])
		err = fuse_burn(FUSE_EK5_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_EK5, buffer[5])
		err = fuse_burn(FUSE_EK5_ADDR_1);
		if (err)
			goto fail;
		write_fuse_word_0(FUSE_EK6, buffer[6])
		err = fuse_burn(FUSE_EK6_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_EK6, buffer[6])
		err = fuse_burn(FUSE_EK6_ADDR_1);
		if (err)
			goto fail;
		write_fuse_word_0(FUSE_EK7, buffer[7])
		err = fuse_burn(FUSE_EK7_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_EK7, buffer[7])
		err = fuse_burn(FUSE_EK7_ADDR_1);
		if (err)
			goto fail;
		break;
	case FUSE_ODMID:
		write_fuse_word_0(FUSE_ODMID0, buffer[0])
		err = fuse_burn(FUSE_ODMID0_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_ODMID0, buffer[0])
		err = fuse_burn(FUSE_ODMID0_ADDR_1);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_ODMID1, buffer[1])
		err = fuse_burn(FUSE_ODMID1_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_ODMID1, buffer[1])
		err = fuse_burn(FUSE_ODMID1_ADDR_1);
		if (err)
			goto fail;
		break;
	case FUSE_H2:
		write_fuse_word_0(FUSE_H2, buffer[0])
		err = fuse_burn(FUSE_H2_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_H2, buffer[0])
		err = fuse_burn(FUSE_H2_ADDR_1);
		if (err)
			goto fail;
		break;
	case FUSE_ODM_INFO:
		write_fuse_word_0(FUSE_ODM_INFO, buffer[0])
		err = fuse_burn(FUSE_ODM_INFO_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_1(FUSE_ODM_INFO, buffer[0])
		err = fuse_burn(FUSE_ODM_INFO_ADDR_1);
		if (err)
			goto fail;
		break;
	case FUSE_DEBUG_AUTHENTICATION:
		write_fuse_word_0(FUSE_DEBUG_AUTHENTICATION, buffer[0])
		err = fuse_burn(FUSE_DEBUG_AUTHENTICATION_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_DEBUG_AUTHENTICATION_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_DEBUG_AUTHENTICATION_REDUNDANT_ADDR_0);
		if (err)
			goto fail;
		break;
	case FUSE_SATA_MPHY_ODM_CALIB:
		write_fuse_word_0(FUSE_SATA_MPHY_ODM_CALIB, buffer[0])
		err = fuse_burn(FUSE_SATA_MPHY_ODM_CALIB_ADDR_0);
		if (err)
			goto fail;
		break;
	case FUSE_CCPLEX_DFD_ACCESS_DISABLE:
		write_fuse_word_0(FUSE_CCPLEX_DFD_ACCESS_DISABLE, buffer[0])
		err = fuse_burn(FUSE_CCPLEX_DFD_ACCESS_DISABLE_ADDR_0);
		if (err)
			goto fail;

		write_fuse_word_0(FUSE_CCPLEX_DFD_ACCESS_DISABLE_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_CCPLEX_DFD_ACCESS_DISABLE_REDUNDANT_ADDR_0);
		if (err)
			goto fail;
		break;
#endif
	default:
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 5);
		goto fail;
	}

fail:
	if (temp_buffer)
		tegrabl_free(temp_buffer);
	if (err)
		pr_error("error = 0x%x in fuse_set_macro_and_burn\n", err);
	return err;
}

static tegrabl_error_t tegrabl_fuse_confirm_burn(enum fuse_type type,
												 uint32_t val_written)
{
	uint32_t size;
	uint32_t *val = NULL;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_fuse_query_size(type, &size);
	if (err) {
		pr_error("Failed to query fuse size\n");
		goto fail;
	}

	val = tegrabl_malloc(size);
	if (val == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	memset(val, 0, size);

	if (type == FUSE_BOOT_SECURITY_INFO) {
		*val = tegrabl_fuse_get_security_info();
	} else {
		err = tegrabl_fuse_read(type, val, size);

		if (err) {
			pr_error("Failed to read fuse\n");
			goto cleanup;
		}
	}

	if (*val != val_written) {
		pr_error("Fuse (%u) is not burnt\n"
				 "val tried to write = 0x%08x\n"
				 "val after write    = 0x%08x\n", type, val_written, *val);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	} else {
		pr_info("Fuse (%u) burnt successfully with val 0x%08x\n", type, *val);
	}

cleanup:
	if (val) {
		tegrabl_free(val);
	}

fail:
	return err;
}

tegrabl_error_t tegrabl_fuse_write(
	uint32_t fuse_type, uint32_t *fuse_val, uint32_t size)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	bool original_visibility;
	uint32_t val_bef_burn;

	if (fuse_val == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 6);
		goto fail;
	}

	val_bef_burn = *fuse_val;

	/* Make all fuse registers visible */
	original_visibility = tegrabl_set_fuse_reg_visibility(true);
	tegrabl_pmc_fuse_control_ps18_latch_set();

	err = fuse_set_macro_and_burn(fuse_type, fuse_val, size);
	if (err) {
		goto fail;
	}

	/* Wait to make sure fuses are burnt */
	tegrabl_mdelay(2);

	tegrabl_pmc_fuse_control_ps18_latch_clear();

	/* Restore back the original visibility */
	tegrabl_set_fuse_reg_visibility(original_visibility);

	/* confirm fuses are burnt */
	err = tegrabl_fuse_confirm_burn(fuse_type, val_bef_burn);
	if (err) {
		goto fail;
	}

fail:
	if (err) {
		pr_error("error = 0x%x in tegrabl_fuse_write\n", err);
	}

	return err;
}
