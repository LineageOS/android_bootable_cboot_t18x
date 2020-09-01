/*
 * Copyright (c) 2015-2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE	TEGRABL_ERR_CLK_RST

#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_module.h>
#include <tegrabl_clock.h>
#include <tegrabl_clk_rst_private.h>
#include <tegrabl_clk_rst_soc.h>
#include <tegrabl_compiler.h>
#include <tegrabl_clock.h>
#include <stdbool.h>
#include <tegrabl_io.h>

/* Global module car info table */
extern struct module_car_info g_module_carinfo[TEGRABL_MODULE_NUM];

tegrabl_error_t tegrabl_car_clk_enable(
		tegrabl_module_t module,
		uint8_t instance,
		void *priv_data)
{
	return tegrabl_clk_enb_set(module, instance, true, priv_data);
}

bool tegrabl_car_clk_is_enabled(tegrabl_module_t module, uint8_t instance)
{
	return tegrabl_clk_is_enb_set(module, instance);
}

tegrabl_error_t tegrabl_car_clk_disable(
		tegrabl_module_t module,
		uint8_t instance)
{
	return tegrabl_clk_enb_set(module, instance, false, NULL);
}

tegrabl_error_t tegrabl_car_rst_set(
		tegrabl_module_t module,
		uint8_t instance)
{
	return tegrabl_rst_set(module, instance, true);
}

tegrabl_error_t tegrabl_car_rst_clear(
		tegrabl_module_t module,
		uint8_t instance)
{
	return tegrabl_rst_set(module, instance, false);
}

tegrabl_clk_src_id_t tegrabl_car_get_clk_src(
		tegrabl_module_t module,
		uint8_t instance)
{
	struct clk_info *pclk_info;
	struct car_info *pcar_info;
	struct module_car_info *pmodule_car_info;

	if (!module_support(module, instance)) {
		return TEGRABL_CLK_SRC_INVALID;
	}

	/* Handle exceptions */
	if (module == TEGRABL_MODULE_MEM) {
		CLOCK_BUG(module,
				  instance,
				  "%s not supported for this module\n",
				  __func__);
		return TEGRABL_CLK_SRC_INVALID;
	}

	pmodule_car_info = &g_module_carinfo[module];
	pcar_info = &pmodule_car_info->pcar_info[instance];
	pclk_info = &pcar_info->clock_info;

	if (pclk_info == NULL) {
		return TEGRABL_CLK_SRC_INVALID;
	}

	return pclk_info->clk_src;
}

tegrabl_error_t tegrabl_car_set_clk_src(
		tegrabl_module_t module,
		uint8_t instance,
		tegrabl_clk_src_id_t clk_src)
{
	struct clk_info *pclk_info;
	struct car_info *pcar_info;
	struct module_car_info *pmodule_car_info;
	uint32_t src_rate_khz;
	uint32_t div;
	uint32_t src_idx;
	uint32_t clk_src_reg_val;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (!module_support(module, instance)) {
		return TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 2);
	}

	/* Handle exceptions */
	if (module == TEGRABL_MODULE_MEM) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 3);
	}

	pmodule_car_info = &g_module_carinfo[module];
	pcar_info = &pmodule_car_info->pcar_info[instance];
	pclk_info = &pcar_info->clock_info;

	if (pclk_info == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 4);
	}

	if (TEGRABL_CLK_CHECK_SRC_ID(clk_src) == TEGRABL_CLK_SRC_INVALID) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 5);
	}

	/* The source has to be running before configuring CLK_SRC register */
	if (check_clk_src_enable(clk_src) == false) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_STARTED, 1);
	}

	/* Check whether we're already on the requested source */
	if (clk_src != pclk_info->clk_src) {
		/* Check if default source can be overridden for this module
		 * Return ERROR if it cannot
		 */
		if (!pclk_info->allow_src_switch) {
			CLOCK_BUG(
					module,
					instance,
					"Changing source not supported for this module\n"
					);
			return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 6);
		}

		/* Get frequency of the source to be set */
		err = tegrabl_car_get_clk_src_rate(clk_src, &src_rate_khz);
		if (err != TEGRABL_NO_ERROR) {
			return err;
		}

		/* Read clock source register */
		clk_src_reg_val = NV_CLK_RST_READ_OFFSET(pclk_info->clk_src_reg);

		/* Get current divider (divisor shift is always 0) */
		div = clk_src_reg_val & pclk_info->clk_div_mask;

		/* Get index of source to be set */
		src_idx = get_src_idx(pclk_info->src_list, clk_src);

		/* If the clock is currently disabled, store the rate in
		 * resume_rate so that when it is enabled again, it starts running
		 * at that rate
		 */
		if (pclk_info->clk_rate != 0U) {
			/* Update clk_src register with new src */
			update_clk_src_reg(module, instance, pclk_info, div, src_idx);
			pclk_info->clk_rate = get_clk_rate_khz(src_rate_khz, div,
												   pclk_info->div_type);
		} else {
			pclk_info->resume_rate = get_clk_rate_khz(src_rate_khz, div,
													  pclk_info->div_type);
		}

		/* Update clock state */
		pclk_info->clk_src_idx = src_idx;
		pclk_info->clk_src = clk_src;
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_car_set_clk_rate(
		tegrabl_module_t module,
		uint8_t instance,
		uint32_t rate_khz,
		uint32_t *rate_set_khz)
{
	struct clk_info *pclk_info;
	struct car_info *pcar_info;
	struct module_car_info *pmodule_car_info;
	uint32_t src_rate_khz;
	uint32_t new_div;
	uint32_t curr_src_idx;
	tegrabl_clk_src_id_t curr_clk_src;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (!module_support(module, instance) || (rate_set_khz == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 1);
	}

	/* Handle exceptions */
	if (module == TEGRABL_MODULE_MEM) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 1);
	}

	pmodule_car_info = &g_module_carinfo[module];
	pcar_info = &pmodule_car_info->pcar_info[instance];
	pclk_info = &pcar_info->clock_info;

	if (pclk_info == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 2);
	}

	/* Get current clock source and its source index */
	curr_clk_src = pclk_info->clk_src;
	curr_src_idx = pclk_info->clk_src_idx;

	/* Get the source rate */
	err = tegrabl_car_get_clk_src_rate(curr_clk_src, &src_rate_khz);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	/* Get new divider based on src rate, desired rate and divtype */
	new_div = get_divider(src_rate_khz, rate_khz, pclk_info->div_type);

	/* If the clock is currently disabled, enable it */
	if (pclk_info->clk_rate == 0) {
		NV_CLK_RST_WRITE_OFFSET(pclk_info->clk_enb_set_reg, 0x1);
	}

	/* Update clk_src register with new divider */
	update_clk_src_reg(module, instance, pclk_info, new_div, curr_src_idx);

	/* Update clock state and the actual rate set */
	*rate_set_khz = pclk_info->clk_rate = get_clk_rate_khz(
										src_rate_khz, new_div,
										pclk_info->div_type);

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_car_get_clk_rate(
		tegrabl_module_t module,
		uint8_t instance,
		uint32_t *rate_khz)
{
	struct clk_info *pclk_info;
	struct car_info *pcar_info;
	struct module_car_info *pmodule_car_info;

	if (!module_support(module, instance) || (rate_khz == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 3);
	}

	/* Handle exceptions */
	if (module == TEGRABL_MODULE_MEM) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 7);
	}

	pmodule_car_info = &g_module_carinfo[module];
	pcar_info = &pmodule_car_info->pcar_info[instance];
	pclk_info = &pcar_info->clock_info;

	if (pclk_info == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 8);
	}

	if (!pclk_info->ON)
		*rate_khz = 0;
	else
		*rate_khz = pclk_info->clk_rate;

	return TEGRABL_NO_ERROR;
}
