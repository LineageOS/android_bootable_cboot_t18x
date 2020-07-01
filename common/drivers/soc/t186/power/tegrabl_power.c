/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
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
#if defined(CONFIG_ENABLE_XUSBH)
#include <bpmp_abi.h>
#include <powergate-t186.h>
#include <tegrabl_bpmp_fw_interface.h>
#endif

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
		pr_debug("%s: Unpowergating APE\n", __func__);
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
	if (err != TEGRABL_NO_ERROR)
		goto fail;

	pr_debug("%s: Unpowergate done\n", __func__);

fail:
	if (err != TEGRABL_NO_ERROR)
		pr_error("%s: exit error = %x\n", __func__, err);
	return err;
}

static tegrabl_error_t tegrabl_unpowergate_xusbh(bool enable)
{
#if defined(CONFIG_ENABLE_XUSBH)
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct mrq_pg_update_state_request pg_write_req;
	uint32_t partition_id[2] = {
		TEGRA186_POWER_DOMAIN_XUSBA,
		TEGRA186_POWER_DOMAIN_XUSBC
	};
	uint8_t i;

	for (i = 0; i < 2; i++) {
		pg_write_req.partition_id = partition_id[i];
		if (enable == true) {
			pg_write_req.sram_state = 0x1;	/* power ON */
			pg_write_req.logic_state = 0x3; /* power ON */
			pg_write_req.clock_state = 0x3; /* enable clocks */
		} else {
			pg_write_req.sram_state = 0x3;	/* power OFF */
			pg_write_req.logic_state = 0x1; /* power OFF */
			pg_write_req.clock_state = 0x1; /* disable clocks */
		}

		err = tegrabl_ccplex_bpmp_xfer(&pg_write_req, NULL,
			sizeof(pg_write_req), 0, MRQ_PG_UPDATE_STATE);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Unable to power on - "
				"TEGRA186_POWER_DOMAIN_XUSB%c\n",
				(i == 0) ? 'A' : 'C');
			return err;
		}
	}
	return err;
#else
	TEGRABL_UNUSED(enable);
	return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
#endif
}

tegrabl_error_t tegrabl_unpowergate(tegrabl_module_t module)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	bool flag = ((module & 0x80000000) == 0x80000000) ? false : true;

	module &= ~0x80000000;
	switch (module)
	{
	case TEGRABL_MODULE_APE:
		err = tegrabl_powergate_ape();
		if (err != TEGRABL_NO_ERROR)
			goto fail;
		break;
	case TEGRABL_MODULE_XUSBH:
		err = tegrabl_unpowergate_xusbh(flag);
		if (err != TEGRABL_NO_ERROR)
			goto fail;
		break;
	default:
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		break;
	}

fail:
	if (err != TEGRABL_NO_ERROR)
		pr_error("%s: exit error = %x\n", __func__, err);
	return err;
}
