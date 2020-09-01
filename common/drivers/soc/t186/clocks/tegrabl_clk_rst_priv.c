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
#include <tegrabl_clk_rst_table.h>
#include <tegrabl_compiler.h>
#include <tegrabl_timer.h>
#include <armc.h>
#include <stdbool.h>
#include <tegrabl_io.h>

#define ABS(x) (((x) < 0) ? -(x) : (x))

/* The following table should have the entries in same order as
 * tegrabl_module_t definition in include/drivers/tegrabl_module.h
 * Use CLOCKINFO_DATA for modules that have proper clock/reset enable,
 * and/or clock source registers. For the rest, use
 * CLOCKINFO_DATA_DUMMY.
 */
struct module_car_info g_module_carinfo[TEGRABL_MODULE_NUM] = {
	[TEGRABL_MODULE_CLKRST] = CLOCKINFO_DATA_DUMMY(clk_rst),
	[TEGRABL_MODULE_UART] = CLOCKINFO_DATA(uart),
	[TEGRABL_MODULE_SDMMC] = CLOCKINFO_DATA(sdmmc),
	[TEGRABL_MODULE_QSPI] = CLOCKINFO_DATA(qspi),
	[TEGRABL_MODULE_SE] = CLOCKINFO_DATA(se),
	[TEGRABL_MODULE_XUSB_HOST] = CLOCKINFO_DATA(xusb_host),
	[TEGRABL_MODULE_XUSB_DEV] = CLOCKINFO_DATA(xusb_dev),
	[TEGRABL_MODULE_XUSB_PADCTL] = CLOCKINFO_DATA(xusb_padctl),
	[TEGRABL_MODULE_XUSB_SS] = CLOCKINFO_DATA(xusb_ss),
	[TEGRABL_MODULE_XUSBF] = CLOCKINFO_DATA(xusbf),
	[TEGRABL_MODULE_DPAUX1] = CLOCKINFO_DATA_DUMMY(dpaux1),
	[TEGRABL_MODULE_HOST1X] = CLOCKINFO_DATA(host1x),
	[TEGRABL_MODULE_CLDVFS] = CLOCKINFO_DATA(cldvfs),
	[TEGRABL_MODULE_I2C] = CLOCKINFO_DATA(i2c),
	[TEGRABL_MODULE_SOR_SAFE] = CLOCKINFO_DATA_DUMMY(sor_safe),
	[TEGRABL_MODULE_MEM] = CLOCKINFO_DATA_DUMMY(mem),
	[TEGRABL_MODULE_KFUSE] = CLOCKINFO_DATA(kfuse),
	[TEGRABL_MODULE_NVDEC] = CLOCKINFO_DATA(nvdec),
	[TEGRABL_MODULE_GPCDMA] = CLOCKINFO_DATA(gpcdma),
	[TEGRABL_MODULE_BPMPDMA] = CLOCKINFO_DATA(bpmpdma),
	[TEGRABL_MODULE_SPEDMA] = CLOCKINFO_DATA(spedma),
	[TEGRABL_MODULE_SOC_THERM] = CLOCKINFO_DATA(soc_therm),
	[TEGRABL_MODULE_APE] = CLOCKINFO_DATA(ape),
	[TEGRABL_MODULE_ADSP] = CLOCKINFO_DATA(adsp),
	[TEGRABL_MODULE_APB2APE] = CLOCKINFO_DATA(apb2ape),
	[TEGRABL_MODULE_SATA] = CLOCKINFO_DATA(sata),
	[TEGRABL_MODULE_PWM] = CLOCKINFO_DATA(pwm),
	[TEGRABL_MODULE_SATACOLD] = CLOCKINFO_DATA(sata_cold),
	[TEGRABL_MODULE_SATA_OOB] = CLOCKINFO_DATA(sata_oob),
};

bool module_support(tegrabl_module_t module, uint8_t instance)
{
	struct module_car_info *pmodule_car_info;

	if (module >= ARRAY_SIZE(g_module_carinfo)) {
		CLOCK_BUG(module, instance, "module not supported\n");
		return false;
	}
	pmodule_car_info = &g_module_carinfo[module];

	if (instance >= pmodule_car_info->instance_count) {
		CLOCK_BUG(module, instance, "Invalid instance Number");
		return false;
	}
	return true;
}

uint32_t get_divider(uint32_t src_rate, uint32_t module_rate,
		tegrabl_clk_div_type_t divtype)
{
	uint32_t sr = src_rate;
	uint32_t mr = module_rate;
	uint32_t tmp = 0;

	if (divtype == TEGRABL_CLK_DIV_TYPE_FRACTIONAL) {
		/* If src rate is lower than required rate, return 0
		 * so that src is fed undivided. That's the best we
		 * can do in this scenario
		 */
		if (sr < mr) {
			return 0;
		}

		uint32_t div1 = 2UL * (sr/mr - 1UL);
		uint32_t div2 = div1;
		while (((sr << 1)/(div1 + 2U)) > mr) {
			div2 = div1;
			div1++;
		}
		tmp = ((sr << 1)/(div1 + 2U)) - mr;
		uint32_t diff1 = (uint32_t)ABS((int)tmp);
		tmp = ((sr << 1)/(div2 + 2U)) - mr;
		uint32_t diff2 = (uint32_t)ABS((int)tmp);
		return (diff1 < diff2) ? div1 : div2;
	} else {
		/* Use ceil divison to make sure that final clock frequency is
		 * always less than or equal to asked frequency.
		 */
		return DIV_CEIL(sr, mr) - 1UL;
	}
}

uint32_t get_clk_rate_khz(
		uint32_t src_rate_khz,
		uint32_t div,
		tegrabl_clk_div_type_t div_type)
{
	if (div_type == TEGRABL_CLK_DIV_TYPE_FRACTIONAL) {
		return (src_rate_khz << 1) / (div + 2UL);
	} else {
		return src_rate_khz / (div + 1UL);
	}
}

int get_src_idx(
		tegrabl_clk_src_id_t *src_list,
		tegrabl_clk_src_id_t clk_src)
{
	int i = 0;

	if (src_list == NULL) {
		return -1;
	}

	for (i = 0; i <= MAX_SRC_ID; i++) {
		if (src_list[i] == clk_src) {
			return i;
		}
	}
	return -1;
}

void update_clk_src_reg(
		tegrabl_module_t module,
		uint32_t instance,
		struct clk_info *pclk_info,
		uint32_t div, uint32_t src_idx)
{
	uint32_t regdata;
	uint32_t clk_src_reg_val = NV_CLK_RST_READ_OFFSET(pclk_info->clk_src_reg);

	TEGRABL_UNUSED(instance);

	/**  To use PLLP/OSC branch for SDMMC4 clocking, PLLC4 should be out of
	 * IDDQ mode. IDDQ = 0. Refer bug 200056675.
	 * SYNCMODE = 1 in PLLC4_MISC1.
	 */
	if (module == TEGRABL_MODULE_SDMMC) {
		regdata = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
				CLK_RST_CONTROLLER_PLLC4_BASE_0);
		if (0UL == NV_DRF_VAL(CLK_RST_CONTROLLER, PLLC4_BASE,
							PLLC4_ENABLE, regdata)) {
			regdata = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
					PLLC4_BASE, PLLC4_IDDQ, OFF, regdata);
			NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
					CLK_RST_CONTROLLER_PLLC4_BASE_0, regdata);
			/* wait for 5 usec */
			tegrabl_udelay(5ULL);
		}

		regdata = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
				CLK_RST_CONTROLLER_PLLC4_MISC1_0);
		regdata = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLC4_MISC1,
				PLLC4_SYNCMODE, 1, regdata);
		NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_PLLC4_MISC1_0,
				regdata);
		/* wait for 5 usec */
		tegrabl_udelay(5ULL);
	}

	UPDATE_CLK_DIV(clk_src_reg_val, pclk_info->clk_div_mask, div);
	CLOCK_DEBUG(module, instance,
				"Updating CLK_SOURCE register [0x%X] with 0x%X\n",
				pclk_info->clk_src_reg, clk_src_reg_val);
	NV_CLK_RST_WRITE_OFFSET(pclk_info->clk_src_reg, clk_src_reg_val);

	tegrabl_udelay(2ULL);

	UPDATE_CLK_SRC(clk_src_reg_val, pclk_info->clk_src_mask, src_idx);
	CLOCK_DEBUG(module, instance,
				"Updating CLK_SOURCE register [0x%X] with 0x%X\n",
				pclk_info->clk_src_reg, clk_src_reg_val);
	NV_CLK_RST_WRITE_OFFSET(pclk_info->clk_src_reg, clk_src_reg_val);

	tegrabl_udelay(2ULL);
}

static tegrabl_error_t tegrabl_clk_enb_set_handle_exceptions(
	tegrabl_module_t module, bool enable, void *priv_data)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	switch (module) {
	case TEGRABL_MODULE_MEM:
		err = tegrabl_enable_mem_clk(enable, priv_data);
		break;
#if defined(CONFIG_ENABLE_QSPI)
	case TEGRABL_MODULE_QSPI:
		err = tegrabl_enable_qspi_clk(priv_data);
		break;
#endif
	case TEGRABL_MODULE_XUSB_HOST:
		NV_CLK_RST_UPDATE_OFFSET((uint32_t)CLK_RST_CONTROLLER_CLK_OUT_ENB_XUSB_0,
			(enable ? (1UL << 2) : 0UL));
		break;
	case TEGRABL_MODULE_XUSB_DEV:
		NV_CLK_RST_UPDATE_OFFSET((uint32_t)CLK_RST_CONTROLLER_CLK_OUT_ENB_XUSB_0,
			(enable ? (1UL << 1) : 0UL));
		break;
	case TEGRABL_MODULE_XUSB_PADCTL:
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 12);
		break;
	case TEGRABL_MODULE_XUSB_SS:
		NV_CLK_RST_UPDATE_OFFSET((uint32_t)CLK_RST_CONTROLLER_CLK_OUT_ENB_XUSB_0,
			(enable ? (1UL << 3) : 0UL));
		break;
	case TEGRABL_MODULE_XUSBF:
		NV_CLK_RST_UPDATE_OFFSET((uint32_t)CLK_RST_CONTROLLER_CLK_OUT_ENB_XUSB_0,
			(enable ? 1UL : 0UL));
		break;
	default:
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		break;
	}

	return err;
}

bool tegrabl_clk_is_enb_set(
		tegrabl_module_t module,
		uint8_t instance)
{
	struct clk_info *pclk_info;
	struct car_info *pcar_info;
	struct module_car_info *pmodule_car_info;
	uint32_t reg = 0;

	if (!module_support(module, instance)) {
		return false;
	}

	pmodule_car_info = &g_module_carinfo[module];

	pcar_info = &pmodule_car_info->pcar_info[instance];

	pclk_info = &pcar_info->clock_info;
	if (pclk_info == NULL) {
		return false;
	}

	reg = NV_CLK_RST_READ_OFFSET(pclk_info->clk_enb_set_reg);
	return ((reg & (0x1UL << pcar_info->bit_offset)) != 0UL);
}

tegrabl_error_t tegrabl_clk_enb_set(
		tegrabl_module_t module,
		uint8_t instance,
		bool enable,
		void *priv_data)
{
	struct clk_info *pclk_info;
	struct car_info *pcar_info;
	struct module_car_info *pmodule_car_info;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_clk_enb_set_handle_exceptions(module, enable, priv_data);
	if (TEGRABL_ERROR_REASON(err) != TEGRABL_ERR_NOT_FOUND) {
		return err;
	}

	err = TEGRABL_NO_ERROR;

	if (!module_support(module, instance)) {
		return TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 4);
	}

	pmodule_car_info = &g_module_carinfo[module];

	pcar_info = &pmodule_car_info->pcar_info[instance];
	if (pcar_info == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 9);
	}

	pclk_info = &pcar_info->clock_info;
	if (pclk_info == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 10);
	}

	if (enable) {
		if (pclk_info->clk_enb_set_reg != 0U) {
			CLOCK_DEBUG(module, instance,
						"Updating CLK_ENB_SET register [0x%X] with 0x%X\n",
						pclk_info->clk_enb_set_reg,
						(0x1U << pcar_info->bit_offset));
			NV_CLK_RST_WRITE_OFFSET(pclk_info->clk_enb_set_reg,
						0x1UL << pcar_info->bit_offset);
		}
		if (pclk_info->clk_src_reg != 0U) {
			err = tegrabl_car_set_clk_rate(module, instance,
									   pclk_info->resume_rate,
									   &pclk_info->clk_rate);

			/* Force enable divider for UART instances */
			if (module == TEGRABL_MODULE_UART) {
				uint32_t reg = NV_CLK_RST_READ_OFFSET(pclk_info->clk_src_reg);
				reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_UARTA,
										 UARTA_DIV_ENB, DISABLE, reg);
				NV_CLK_RST_WRITE_OFFSET(pclk_info->clk_src_reg, reg);
			}
		}
	} else {
		if (pclk_info->clk_enb_clr_reg != 0U) {
			CLOCK_DEBUG(module, instance,
						"Updating CLK_ENB_CLR register [0x%X] with 0x%X\n",
						pclk_info->clk_enb_clr_reg,
						(0x1U << pcar_info->bit_offset));
			NV_CLK_RST_WRITE_OFFSET(pclk_info->clk_enb_clr_reg,
						(0x1UL << pcar_info->bit_offset));

			/* Update clock state */
			if (pclk_info->clk_rate != 0U) {
				pclk_info->resume_rate = pclk_info->clk_rate;
				pclk_info->clk_rate = 0;
			}
		}
	}

	if ((pclk_info->clk_enb_set_reg == 0U) && (pclk_info->clk_src_reg == 0U)) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 11);
	}

	return err;
}

tegrabl_error_t tegrabl_rst_set(
		tegrabl_module_t module,
		uint8_t instance,
		bool enable)
{
	struct rst_info *prst_info;
	struct car_info *pcar_info;
	struct module_car_info *pmodule_car_info;

	if (!module_support(module, instance)) {
		return TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 5);
	}

	/* Handle exceptions */
	if (module == TEGRABL_MODULE_MEM) {
		return tegrabl_assert_mem_rst(enable);
	}

	pmodule_car_info = &g_module_carinfo[module];

	pcar_info = &pmodule_car_info->pcar_info[instance];
	if (pcar_info == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 12);
	}

	prst_info = &pcar_info->reset_info;
	if (prst_info == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 13);
	}

	/* Now handle generic cases */
	if (prst_info->rst_set_reg != 0U) {
		if (enable) {
			CLOCK_DEBUG(module, instance,
						"Updating CLK_RST_SET register [0x%X] with 0x%X\n",
						prst_info->rst_set_reg, 0x1);

			NV_CLK_RST_WRITE_OFFSET(prst_info->rst_set_reg,
						(0x1UL << pcar_info->bit_offset));

			pcar_info->clock_info.ON = false;
		} else {
			CLOCK_DEBUG(module, instance, "Updating CLK_RST_CLR register "
						"[0x%X] with 0x%X\n", prst_info->rst_clr_reg, 0x1);

			NV_CLK_RST_WRITE_OFFSET(prst_info->rst_clr_reg,
						(0x1UL << pcar_info->bit_offset));

			pcar_info->clock_info.ON = true;
		}
	} else {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 14);
	}

	return TEGRABL_NO_ERROR;
}
