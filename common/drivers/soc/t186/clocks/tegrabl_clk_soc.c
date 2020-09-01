/*
 * Copyright (c) 2015-2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE	TEGRABL_ERR_CLK_RST

#include <tegrabl_debug.h>
#include <tegrabl_module.h>
#include <tegrabl_error.h>
#include <tegrabl_timer.h>
#include <tegrabl_clock.h>
#include <tegrabl_clk_rst_soc.h>
#include <tegrabl_clk_rst_private.h>
#include <arusec_cntr.h>
#include <arpmc_impl.h>
#include <arpadctl_DEBUG.h>
#include <tegrabl_io.h>

#define PLLM_OVERRIDE_ENABLE 11
#define CONF_CLK_M_12_8_MHZ  0

#define PADCTL_A5_WRITE(reg, val) \
	NV_WRITE32(NV_ADDRESS_MAP_PADCTL_A5_BASE + (PADCTL_##reg##_0), val)

#define PADCTL_GEN1_I2C_SDA_REG 0x02434060
#define PADCTL_GEN1_I2C_SCL_REG 0x02434068
#define PADCTL_PWR_I2C_SDA_REG 0x0C301060
#define PADCTL_PWR_I2C_SCL_REG 0x0C301068

/* For most oscillator frequencies, the PLL reference divider is 1.
 * Frequencies that require a different reference divider will set
 * it below. */
uint32_t g_pllrefdiv = CLK_RST_CONTROLLER_OSC_CTRL_0_PLL_REF_DIV_DIV1;

/* Dummy fields added to maintain enum tegrabl_clk_osc_freq
 * enum values in a sequence
 */
static const uint32_t s_tsc_cfg_table[(int) TEGRABL_CLK_OSC_FREQ_MAX_VAL] =
{
	/* 13MHz */

	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_ENB, 1) |
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_DIVIDEND, (125-1)) |
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_DIVISOR, (156-1)),

	/* 16.8MHz */
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_ENB, 1) |
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_DIVIDEND, (625-1)) |
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_DIVISOR, (672-1)),

	/* dummy field */
	0,

	/* dummy field */
	0,

	/* 19.2MHz */
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_ENB, 1) |
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_DIVIDEND, (625-1)) |
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_DIVISOR, (768-1)),

	/* 38.4MHz */
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_ENB, 1) |
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_DIVIDEND, (625-1)) |
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_DIVISOR, (768-1)),

	/* dummy field */
	0,

	/* dummy field */
	0,

	/* 12MHz */
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_ENB, 1) |
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_DIVIDEND, (125-1)) |
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_DIVISOR, (144-1)),

	/* 48MHz */
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_ENB, 1) |
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_DIVIDEND, (125-1)) |
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_DIVISOR, (192-1)),

	/* dummy field */
	0,

	/* dummy field */
	0,

	/* 26MHz */
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_ENB, 1) |
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_DIVIDEND, (125-1)) |
	NV_DRF_NUM(CLK_RST_CONTROLLER, TSC_OSC_SUPER_CLK_DIVIDER,
			   SUPER_TSC_OSC_DIV_DIVISOR, (208-1)),
};

void tegrabl_car_clock_init(void)
{
	tegrabl_clk_osc_freq_t osc_freq;
	uint32_t regval;

	osc_freq = tegrabl_get_osc_freq();

	/* Try to set clk_m in 12MHz - 20MHz range. For 38.2MHz, set to 19.2 */
	switch (osc_freq) {
	case TEGRABL_CLK_OSC_FREQ_26:
	case TEGRABL_CLK_OSC_FREQ_38_4:
	case TEGRABL_CLK_OSC_FREQ_48:
			regval = NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_M_DIVIDE,
													  CLK_M_DIVISOR,
													  1);
			NV_CLK_RST_WRITE_REG(CLK_M_DIVIDE, regval);
			break;
	default:
		break;
	}

#if defined(CONFIG_ENABLE_FPGA)
	/* Increase the reset delay time to maximum for FPGAs to avoid
	 * race conditions between WFI and reset propagation due to
	 * delays introduced by serializers.
	 */
	reg = NV_CLK_RST_READ_REG(CPU_SOFTRST_CTRL);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CPU_SOFTRST_CTRL,
			CPU_SOFTRST_LEGACY_WIDTH, 0xF0, reg);
	NV_CLK_RST_WRITE_REG(CPU_SOFTRST_CTRL, reg);
#endif

	return;
}

tegrabl_clk_osc_freq_t tegrabl_get_osc_freq(void)
{
	uint32_t reg;

	reg = NV_CLK_RST_READ_REG(OSC_CTRL);

	return (tegrabl_clk_osc_freq_t)NV_DRF_VAL(CLK_RST_CONTROLLER,
			OSC_CTRL, OSC_FREQ, reg);
}

tegrabl_error_t tegrabl_car_get_osc_freq_khz(uint32_t *freq_khz)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (freq_khz == NULL) {
		return TEGRABL_ERR_BAD_PARAMETER;
	}

	switch (tegrabl_get_osc_freq()) {
	case TEGRABL_CLK_OSC_FREQ_13:
		*freq_khz = 13000;
		break;

	case TEGRABL_CLK_OSC_FREQ_19_2:
		*freq_khz = 19200;
		break;

	case TEGRABL_CLK_OSC_FREQ_12:
		*freq_khz = 12000;
		break;

	case TEGRABL_CLK_OSC_FREQ_26:
		*freq_khz = 26000;
		break;

	case TEGRABL_CLK_OSC_FREQ_16_8:
		*freq_khz = 16800;
		break;

	case TEGRABL_CLK_OSC_FREQ_38_4:
		*freq_khz = 38400;
		break;

	case TEGRABL_CLK_OSC_FREQ_48:
		*freq_khz = 48000;
		break;

	default:
		*freq_khz = 0;
		err = TEGRABL_ERR_NOT_SUPPORTED;
		pr_debug("Invalid Oscillator freq\n");
		break;
	}
	return err;
}

tegrabl_error_t tegrabl_car_init_pll_with_rate(
		tegrabl_clk_pll_id_t pll_id,
		uint32_t rate_khz,
		void *priv_data)
{
	TEGRABL_UNUSED(priv_data);
	TEGRABL_UNUSED(rate_khz);

	switch (pll_id) {
	case TEGRABL_CLK_PLL_ID_PLLP:
		/* PLLP is already inited by BR */
		return TEGRABL_NO_ERROR;
		/* Rate is unnecessary for PLLE, Sata PLL and PLLC4 */
	case TEGRABL_CLK_PLL_ID_PLLE:
		return tegrabl_init_plle();
	case TEGRABL_CLK_PLL_ID_SATA_PLL:
		return tegrabl_sata_pll_cfg();
	case TEGRABL_CLK_PLL_ID_PLLC4:
		return tegrabl_init_pllc4();
#if defined(CONFIG_ENABLE_CLOCK_PLLAON)
	case TEGRABL_CLK_PLL_ID_AON_PLL:
		return tegrabl_init_pllaon();
#endif
	case TEGRABL_CLK_PLL_ID_UTMI_PLL:
		return tegrabl_init_utmipll();
	default:
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 12);
	}
	return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 13);
}

tegrabl_error_t tegrabl_car_get_clk_src_rate(
		tegrabl_clk_src_id_t src_id,
		uint32_t *rate_khz)
{
	uint32_t clkm_div;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

#if defined(CONFIG_ENABLE_FPGA)
	/* On FPGA, this is the default src freq for all modules
	 * that have divider logic
	 */
	*rate_khz = T18X_FPGA_DEFAULT_SRC_FREQ_KHZ;
	return TEGRABL_NO_ERROR;
#endif

	switch (src_id) {
	case TEGRABL_CLK_SRC_CLK_M:
		clkm_div = NV_DRF_VAL(CLK_RST_CONTROLLER, CLK_M_DIVIDE,
							  CLK_M_DIVISOR,
							  NV_CLK_RST_READ_REG(CLK_M_DIVIDE));
		err = tegrabl_car_get_osc_freq_khz(rate_khz);
		if (err == TEGRABL_NO_ERROR) {
			*rate_khz = *rate_khz / (clkm_div + 1);
		}
		break;

	case TEGRABL_CLK_SRC_PLLP_OUT0:
		*rate_khz = PLLP_FIXED_FREQ_KHZ_408000;
		break;

	case TEGRABL_CLK_SRC_PLLM_OUT0:
		*rate_khz = tegrabl_get_pll_freq_khz(TEGRABL_CLK_PLL_ID_PLLM);
		break;

	case TEGRABL_CLK_SRC_PLLC4_OUT0_LJ:
		*rate_khz = tegrabl_get_pll_freq_khz(TEGRABL_CLK_PLL_ID_PLLC4);
		break;

	default:
		pr_debug("Clk source %u not supported\n", src_id);
		err = TEGRABL_ERR_NOT_SUPPORTED;
		break;
	}

	return err;
}

static tegrabl_error_t set_pllc4_muxed_clk(uint32_t rate_set_khz)
{
	uint32_t freq_khz, val;
	uint8_t pll_div;
	/* check if PLLC4 is enabled already or not */
	if (!CHECK_PLL_ENABLE(PLLC4))
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);

	/* Get pllc4 freq */
	freq_khz = tegrabl_get_pll_freq_khz(TEGRABL_CLK_PLL_ID_PLLC4);

	/* Get the divisor */
	pll_div = (rate_set_khz + (freq_khz / 2)) / freq_khz;

	/* Program the divisor */
	val = NV_CLK_RST_READ_REG(PLLC4_MISC1);
	switch (pll_div) {
	case 2:
		val = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLC4_MISC1,
		PLLC4_CLK_SEL, PLLC4_VCO_DIV2, val);
		break;
	case 3:
		val = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLC4_MISC1,
		PLLC4_CLK_SEL, PLLC4_OUT1, val);
		break;
	case 5:
		val = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLC4_MISC1,
		PLLC4_CLK_SEL, PLLC4_OUT2, val);
		break;
	default:
		val = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLC4_MISC1,
		PLLC4_CLK_SEL, PLLC4_OUT2, val);
		break;
	}
	NV_CLK_RST_WRITE_REG(PLLC4_MISC1, val);

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_car_set_clk_src_rate(tegrabl_clk_src_id_t src_id,
											 uint32_t rate_khz,
											 uint32_t *rate_set_khz)
{
	if (src_id == TEGRABL_CLK_SRC_PLLC4_MUXED) {
		set_pllc4_muxed_clk(rate_khz);
	}

	if (rate_set_khz != NULL) {
		*rate_set_khz = rate_khz;
	}
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_car_pllm_wb0_override(void)
{
	uint32_t reg;

	/* PMC PLLM OVERRIDE DISABLE */
	reg = NV_READ32(NV_ADDRESS_MAP_PMC_BASE + PMC_IMPL_PLLP_WB0_OVERRIDE_0);
	reg &= ~(1 << PLLM_OVERRIDE_ENABLE);
	NV_WRITE32(NV_ADDRESS_MAP_PMC_BASE + PMC_IMPL_PLLP_WB0_OVERRIDE_0, reg);

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_car_setup_tsc_dividers(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	tegrabl_clk_osc_freq_t osc_freq_enum = tegrabl_get_osc_freq();
	uint32_t tsc_src_reg_val;
	uint32_t osc_freq_khz;
	uint32_t osc_super_clk_div = s_tsc_cfg_table[osc_freq_enum];
	uint32_t hs_super_clk_div = NV_CLK_RST_READ_REG(TSC_HS_SUPER_CLK_DIVIDER);

	hs_super_clk_div = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
					   TSC_HS_SUPER_CLK_DIVIDER,
					   SUPER_TSC_HS_DIV_ENB,
					   ENABLE, hs_super_clk_div);

	err = tegrabl_car_get_osc_freq_khz(&osc_freq_khz);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s: Unsupported osc freq=%d\n", __func__, osc_freq_khz);
		return err;
	}

#if !defined(CONFIG_ENABLE_FPGA)
	if (osc_freq_khz >= 31250) {
		tsc_src_reg_val = NV_CLK_RST_READ_REG(CLK_SOURCE_TSC);
		tsc_src_reg_val = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
						     CLK_SOURCE_TSC,
						     TSC_CLK_SRC,
						     CLK_M,
						     tsc_src_reg_val);
		NV_CLK_RST_WRITE_REG(CLK_SOURCE_TSC, tsc_src_reg_val);

	} else {

		hs_super_clk_div = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
					       TSC_HS_SUPER_CLK_DIVIDER,
					       SUPER_TSC_HS_DIV_DIVIDEND,
					       (125 - 1),
							hs_super_clk_div);

		hs_super_clk_div = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
					       TSC_HS_SUPER_CLK_DIVIDER,
					       SUPER_TSC_HS_DIV_DIVISOR,
					       (816 - 1), /* Values for PLLP_OUT0 @ 408 Mhz */
							hs_super_clk_div);

		tsc_src_reg_val = NV_CLK_RST_READ_REG(CLK_SOURCE_TSC);
		tsc_src_reg_val = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
						     CLK_SOURCE_TSC,
						     TSC_CLK_SRC,
						     PLLP_OUT0,
						     tsc_src_reg_val);
		tsc_src_reg_val = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
							CLK_SOURCE_TSC, TSC_CLK_DIVISOR, 2,
							tsc_src_reg_val);

		NV_CLK_RST_WRITE_REG(CLK_SOURCE_TSC, tsc_src_reg_val);
	}
#else
	tsc_src_reg_val = NV_CLK_RST_READ_REG(CLK_SOURCE_TSC);
	tsc_src_reg_val = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
						 CLK_SOURCE_TSC,
						 TSC_CLK_SRC,
						 CLK_M,
						 tsc_src_reg_val);
	NV_CLK_RST_WRITE_REG(CLK_SOURCE_TSC, tsc_src_reg_val);
#endif

	/* Set up osc_super_clk_divider for OSC path */
	NV_CLK_RST_WRITE_REG(TSC_OSC_SUPER_CLK_DIVIDER, osc_super_clk_div);

	/* Set up hs_super_clk_divider for PLLP (408 Mhz) */
	NV_CLK_RST_WRITE_REG(TSC_HS_SUPER_CLK_DIVIDER, hs_super_clk_div);

	return TEGRABL_NO_ERROR;
}

bool tegrabl_set_fuse_reg_visibility(bool visibility)
{
	uint32_t reg_data;
	bool original_visibility;

	reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
				CLK_RST_CONTROLLER_MISC_CLK_ENB_0);

	/* store the original visibility to return it for clients
	 * It is the responsibility of clients to restore it back */
	original_visibility = NV_DRF_VAL(CLK_RST_CONTROLLER, MISC_CLK_ENB,
		CFG_ALL_VISIBLE, reg_data);

	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, MISC_CLK_ENB,
		CFG_ALL_VISIBLE, visibility, reg_data);

	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_MISC_CLK_ENB_0,
		reg_data);

	return original_visibility;
}

tegrabl_error_t tegrabl_usbf_clock_init(void)
{
	uint32_t reg_data;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	/* Set clk source of XUSB dev to PLLP_OUT0 divided
	 * down to 102 Mhz. (408/4)
	 */
	reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
		CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_CORE_DEV_0);
	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
		CLK_SOURCE_XUSB_CORE_DEV,
		XUSB_CORE_DEV_CLK_SRC,
		PLLP_OUT0,
		reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
		CLK_SOURCE_XUSB_CORE_DEV,
		XUSB_CORE_DEV_CLK_DIVISOR,
		6,
		reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
		CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_CORE_DEV_0, reg_data);

	tegrabl_udelay(2);

	/* Set XUSB_SS Clk (@120 Mhz) */
	reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
		CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_SS_0);
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_SOURCE_XUSB_SS,
		XUSB_SS_CLK_DIVISOR, 0x6, reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_XUSB_SS,
		XUSB_SS_CLK_SRC, HSIC_480, reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
		CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_SS_0, reg_data);

	/* Enable clock to FS. Needed for pad macro */
	reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
		CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_FS_0);
	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
		CLK_SOURCE_XUSB_FS,
		XUSB_FS_CLK_SRC,
		FO_48M,
		reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
		CLK_RST_CONTROLLER_CLK_SOURCE_XUSB_FS_0, reg_data);

	/* Take Dev, SS out of reset */
	err = tegrabl_car_rst_clear(TEGRABL_MODULE_XUSB_DEV, 0);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	err = tegrabl_car_rst_clear(TEGRABL_MODULE_XUSB_SS, 0);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	return err;
}

const uint32_t s_usbhsic_trkdiv[TEGRABL_CLK_OSC_FREQ_MAX_VAL] = {
	/* The frequency for the tracking circuit should be between 1 to 10 MHz.
	 * osc_clk is between 10 to 20 MHz, the clock divisor should be set to 0x2;
	 * osc_clk is between 20 to 30 MHz, the clock divisor should be set to 0x4;
	 * osc_clk is between 30 to 40 MHz, the clock divisor should be set to 0x6;
	 */
	2, /* OscFreq_13 */
	2, /* OscFreq_16_8 */
	0, /* dummy field */
	0, /* dummy field */
	2, /* OscFreq_19_2 */
	6, /* OscFreq_38_4, */
	0, /* dummy field */
	0, /* dummy field */
	2, /* OscFreq_12 */
	6, /* OscFreq_48, */
	0, /* dummy field */
	0, /* dummy field */
	4  /* OscFreq_26 */
};

void tegrabl_usbf_program_tracking_clock(bool is_enable)
{
	uint32_t reg_data;
	tegrabl_clk_osc_freq_t osc_freq = tegrabl_get_osc_freq();

	if (is_enable) {
		reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_OUT_ENB_USB2_HSIC_TRK_0);
		reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
			CLK_OUT_ENB_USB2_HSIC_TRK,
			CLK_ENB_USB2_TRK,
			0x1,
			reg_data);
		NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_OUT_ENB_USB2_HSIC_TRK_0, reg_data);

		/* The frequency for the tracking circuit should be between 1 to 10 MHz.
		 * osc_clk is between 10 to 20 MHz, set clock divisor to 0x2;
		 * osc_clk is between 20 to 30 MHz, set clock divisor to 0x4;
		 * osc_clk is between 30 to 40 MHz, set clock divisor to 0x6.
		 */

		reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_SOURCE_USB2_HSIC_TRK_0);
		reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
			CLK_SOURCE_USB2_HSIC_TRK,
			USB2_HSIC_TRK_CLK_DIVISOR,
			s_usbhsic_trkdiv[osc_freq],
			reg_data);
		NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_SOURCE_USB2_HSIC_TRK_0, reg_data);
	} else {
		reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_OUT_ENB_USB2_HSIC_TRK_0);
		reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
			CLK_OUT_ENB_USB2_HSIC_TRK,
			CLK_ENB_USB2_TRK,
			0x0,
			reg_data);
		NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_OUT_ENB_USB2_HSIC_TRK_0, reg_data);
	}
}
