/*
 * Copyright (c) 2015-2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_POWER

#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_module.h>
#include <tegrabl_compiler.h>
#include <stdbool.h>
#include <tegrabl_io.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_clock.h>
#include <arpmc_impl.h>
#include <armc.h>
#include <tegrabl_drf.h>
#include <tegrabl_timer.h>
#include <tegrabl_power.h>

#define pmc_impl_writel(reg, value) \
	NV_WRITE32(NV_ADDRESS_MAP_PMC_BASE + PMC_IMPL_##reg##_0, value)

#define pmc_impl_readl(reg) \
	NV_READ32(NV_ADDRESS_MAP_PMC_BASE + PMC_IMPL_##reg##_0);

#define mc_writel(instance, reg, value) \
	NV_WRITE32(NV_ADDRESS_MAP_##instance##_BASE + MC_##reg##_0, value)

#define mc_readl(instance, reg) \
	NV_READ32(NV_ADDRESS_MAP_##instance##_BASE + MC_##reg##_0)

static tegrabl_error_t tegrabl_powergate_ape(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t val;

	pr_debug("%s: entry", __func__);

	val = pmc_impl_readl(PART_AUD_POWER_GATE_CONTROL);
	val = NV_DRF_VAL(PMC_IMPL, PART_AUD_POWER_GATE_CONTROL, START, val);

	if (val ==
			NV_DRF_DEF(PMC_IMPL, PART_AUD_POWER_GATE_CONTROL, START, PENDING)) {

		/* Unpower gate APE  with single write with start */
		pr_trace("%s: Unpowergating APE\n", __func__);
		val = NV_DRF_DEF(PMC_IMPL, PART_AUD_POWER_GATE_CONTROL,
			LOGIC_SLEEP, OFF);
		val |= NV_DRF_DEF(PMC_IMPL, PART_AUD_POWER_GATE_CONTROL,
			SRAM_SD, OFF);
		val |= NV_DRF_DEF(PMC_IMPL, PART_AUD_POWER_GATE_CONTROL,
			SRAM_SLP, OFF);
		val |= NV_DRF_DEF(PMC_IMPL, PART_AUD_POWER_GATE_CONTROL,
			SRAM_DSLP, OFF);
		val |= NV_DRF_DEF(PMC_IMPL, PART_AUD_POWER_GATE_CONTROL,
			START, PENDING);
		pmc_impl_writel(PART_AUD_POWER_GATE_CONTROL, val);

		/* Poll until START bit is DONE */
		pr_debug("%s: Poll until START bit is DONE\n", __func__);
		do {
			val = pmc_impl_readl(PART_AUD_POWER_GATE_CONTROL);
		} while (NV_DRF_VAL(PMC_IMPL, PART_AUD_POWER_GATE_CONTROL,
				START, val) == 1);
	}

	/* Read STATUS register, check if any mismatch */
	pr_debug("%s: Checking if any mismatch\n", __func__);
	val = pmc_impl_readl(PART_AUD_POWER_GATE_STATUS);
	if ((NV_DRF_VAL(PMC_IMPL, PART_AUD_POWER_GATE_STATUS,
			LOGIC_SLEEP_STS, val) != 0) ||
		(NV_DRF_VAL(PMC_IMPL, PART_AUD_POWER_GATE_STATUS,
			SRAM_SD_STS, val) != 0) ||
		(NV_DRF_VAL(PMC_IMPL, PART_AUD_POWER_GATE_STATUS,
			SRAM_SLP_STS, val) != 0) ||
		(NV_DRF_VAL(PMC_IMPL, PART_AUD_POWER_GATE_STATUS,
			SRAM_DSLP_STS, val) != 0)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	/* Enable clocks */
	pr_debug("%s: Enabling clocks\n", __func__);

	tegrabl_car_clk_enable(TEGRABL_MODULE_ADSP, 0, NULL);
	tegrabl_car_clk_enable(TEGRABL_MODULE_APE, 0, NULL);
	tegrabl_car_clk_enable(TEGRABL_MODULE_APB2APE, 0, NULL);

	/* Deassert rst to APE and ADSPs */
	pr_debug("%s: Deassert reset to APE and ADSPs\n", __func__);
	err = tegrabl_car_rst_clear(TEGRABL_MODULE_APE, 0);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	pr_trace("%s: Unpowergate done\n", __func__);

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: exit error = %x\n", __func__, err);
	}
	return err;
}

tegrabl_error_t tegrabl_unpowergate(tegrabl_module_t module)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	switch (module)
	{
	case TEGRABL_MODULE_APE:
		err = tegrabl_powergate_ape();
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
		break;
	default:
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		break;
	}

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: exit error = %x\n", __func__, err);
	}
	return err;
}
