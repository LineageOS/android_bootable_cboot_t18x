/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE	TEGRABL_ERR_CLK_RST

/* Remove when tegrabl sdram param header is ready */
#define NvU32 uint32_t

#include <tegrabl_debug.h>
#include <tegrabl_module.h>
#include <tegrabl_error.h>
#include <tegrabl_timer.h>
#include <tegrabl_clock.h>
#include <tegrabl_clk_rst_soc.h>
#include <tegrabl_clk_rst_private.h>
#if defined(CONFIG_ENABLE_CLOCK_PLLAON)
#include <tegrabl_mb1bct_lib.h>
#endif
#include <arusec_cntr.h>
#include <arpmc_impl.h>
#include <arpadctl_DEBUG.h>
#include <nvboot_sdram_param.h>
#include <armc.h>
#include <inttypes.h>
#if defined(CONFIG_ENABLE_QSPI)
#include <tegrabl_qspi.h>
#endif
#include <tegrabl_global_defs.h>

extern struct module_car_info g_module_carinfo[TEGRABL_MODULE_NUM];

#define PLLM_VCO_MIN_KHZ    600000
#define CONF_CLK_M_12_8_MHZ  0
#define CLK_RST_CONTROLLER_PLLM_BASE_0_PLLM_BYPASS_DISABLE	\
	CLK_RST_CONTROLLER_PLLM_BASE_0_PLLM_BYPASSPLL_DISABLE
#define CLK_RST_CONTROLLER_PLLM_BASE_0_PLLM_BYPASS_RANGE	\
		CLK_RST_CONTROLLER_PLLM_BASE_0_PLLM_BYPASSPLL_RANGE


#define PADCTL_A5_WRITE(reg, val)							\
	NV_WRITE32(NV_ADDRESS_MAP_PADCTL_A5_BASE + (PADCTL_##reg##_0), val)

#define GET_DIVM(PLL_ID)									\
	NV_DRF_VAL(CLK_RST_CONTROLLER,							\
			PLL_ID##_BASE,									\
			PLL_ID##_DIVM,									\
			NV_CLK_RST_READ_REG(PLL_ID##_BASE))

#define GET_DIVN(PLL_ID)									\
	NV_DRF_VAL(CLK_RST_CONTROLLER,							\
			PLL_ID##_BASE,									\
			PLL_ID##_DIVN,									\
			NV_CLK_RST_READ_REG(PLL_ID##_BASE))

#define GET_DIVP(PLL_ID)									\
	NV_DRF_VAL(CLK_RST_CONTROLLER,							\
			PLL_ID##_BASE,									\
			PLL_ID##_DIVP,									\
			NV_CLK_RST_READ_REG(PLL_ID##_BASE))

#define FILL_MNP(PLL_ID, m, n, p)							\
	do {													\
		m = GET_DIVM(PLL_ID);								\
		n = GET_DIVN(PLL_ID);								\
		p = GET_DIVP(PLL_ID);								\
	} while (0)

#define CHECK_PLL_LOCK(PLLID, FIELD)								\
	NV_DRF_VAL(CLK_RST_CONTROLLER, PLLID##_BASE,			\
			FIELD##_LOCK,									\
			NV_CLK_RST_READ_REG(PLLID##_BASE))

#define ENABLE_PLL_LOCK(PLLID, MISC_REG)					\
	do  { \
		uint32_t val = NV_CLK_RST_READ_REG(MISC_REG);		\
		val = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, MISC_REG,	\
				PLLID##_EN_LCKDET, ENABLE, val);			\
		NV_CLK_RST_WRITE_REG(MISC_REG, val);				\
	} while (0)

#define WAIT_PLL_LOCK(PLLID, FIELD)								\
	do {													\
	} while (!CHECK_PLL_LOCK(PLLID, FIELD))

#define DISABLE_IDDQ(PLLID, MISC_REG)						\
	do {													\
		uint32_t val = 0x0;									\
		val = NV_CLK_RST_READ_REG(MISC_REG);				\
		val = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, MISC_REG,	\
				PLLID##_IDDQ, 0x0, val);					\
		NV_CLK_RST_WRITE_REG(MISC_REG, val);				\
	} while (0)

#define ENABLE_PLL(PLLID)									\
	do {													\
		uint32_t val = 0x0;									\
		val = NV_CLK_RST_READ_REG(PLLID##_BASE);			\
		val = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLID##_BASE,	\
				PLLID##_ENABLE, ENABLE, val);				\
		NV_CLK_RST_WRITE_REG(PLLID##_BASE, val);			\
	} while (0)

#define DISABLE_PLL(PLLID)									\
	do {													\
		uint32_t val = 0x0;									\
		val = NV_CLK_RST_READ_REG(PLLID##_BASE);			\
		val = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLID##_BASE, \
				PLLID##_ENABLE, DISABLE, val);				\
		NV_CLK_RST_WRITE_REG(PLLID##_BASE, val);			\
	} while (0)

#define UPDATE_PLL_BASE(PLLID, bypass, ref_dis, divm, divn, divp)	\
	do	{ \
		uint32_t base = NV_DRF_DEF(CLK_RST_CONTROLLER, PLLID##_BASE, PLLID##_BYPASSPLL, bypass)	\
			| NV_DRF_DEF(CLK_RST_CONTROLLER, PLLID##_BASE, PLLID##_ENABLE, DISABLE)	\
			| NV_DRF_DEF(CLK_RST_CONTROLLER, PLLID##_BASE, PLLID##_REF_DIS, REF_##ref_dis)	\
			| NV_DRF_NUM(CLK_RST_CONTROLLER, PLLID##_BASE, PLLID##_DIVP, divp)	\
			| NV_DRF_NUM(CLK_RST_CONTROLLER, PLLID##_BASE, PLLID##_DIVN, divn)	\
			| NV_DRF_NUM(CLK_RST_CONTROLLER, PLLID##_BASE, PLLID##_DIVM, divm);	\
		NV_CLK_RST_WRITE_REG(PLLID##_BASE, base);			\
	} while (0)

/* AON PLL params */
#define AON_EXT_FRU_VAL 0x8000
#define AON_PLL_FRUG_VAL 0x72
#define AON_FLL_LD_MEM_VAL 0xF
#define AON_KP_STEP_TIMER_VAL 0x4
#define AON_FRAC_STEP_TIMER_VAL 3
#define AON_FRAC_STEP_VAL 0xFFF

/* Max wait time for period for PLLAON lock.
	Value has taken from  bug 200049029 comment #84. */
#define AON_PLL_LOCK_MAX_TIMEOUT 300

uint32_t tegrabl_get_pllref_khz(void)
{
	uint32_t freq_khz;

	tegrabl_car_get_osc_freq_khz(&freq_khz);
	return freq_khz >> g_pllrefdiv;
}

tegrabl_error_t tegrabl_enable_mem_clk(bool enable, void *priv_data)
{
	uint32_t reg = 0;
	NvBootSdramParams *pdata = (NvBootSdramParams *)priv_data;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (enable) {
#if !defined(CONFIG_ENABLE_FPGA)
		if (priv_data != NULL) {
			/* Get clock source */
			reg = NV_DRF_VAL(CLK_RST_CONTROLLER, CLK_SOURCE_EMC,
					EMC_2X_CLK_SRC, pdata->EmcClockSource);
			if (reg == NV_DRF_DEF(CLK_RST_CONTROLLER, CLK_SOURCE_EMC,
					EMC_2X_CLK_SRC, PLLM_OUT0)) {
				err = tegrabl_init_pllm(pdata);
				if (err != TEGRABL_NO_ERROR)
					return err;
			}
		}
#endif
	} else {
		/* Configure EMCSB */
		reg = NV_CLK_RST_READ_REG(CLK_OUT_ENB_EMCSB_CLR);
		reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
								 CLK_OUT_ENB_EMCSB_CLR,
								 CLR_CLK_ENB_MCHUBSB, ENABLE, reg);
		reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
								 CLK_OUT_ENB_EMCSB_CLR,
								 CLR_CLK_ENB_MC3, ENABLE, reg);
		reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
								 CLK_OUT_ENB_EMCSB_CLR,
								 CLR_CLK_ENB_EMCSB_LATENCY, ENABLE, reg);
		reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
								 CLK_OUT_ENB_EMCSB_CLR,
								 CLR_CLK_ENB_EMC, ENABLE, reg);
		NV_CLK_RST_WRITE_REG(CLK_OUT_ENB_EMCSB_CLR, reg);

		/* Configure EMC */
		reg = NV_CLK_RST_READ_REG(CLK_OUT_ENB_EMC_CLR);
		reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
								 CLK_OUT_ENB_EMC_CLR,
								 CLR_CLK_ENB_MC_CEPA, ENABLE, reg);
		reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
								 CLK_OUT_ENB_EMC_CLR,
								 CLR_CLK_ENB_MC_CDPA, ENABLE, reg);
		reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
								 CLK_OUT_ENB_EMC_CLR,
								 CLR_CLK_ENB_MC_CCPA, ENABLE, reg);
		reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
								 CLK_OUT_ENB_EMC_CLR,
								 CLR_CLK_ENB_MCHUBSA, ENABLE, reg);
		reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
								 CLK_OUT_ENB_EMC_CLR,
								 CLR_CLK_ENB_MC1, 0x1, reg);
		reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
								 CLK_OUT_ENB_EMC_CLR,
								 CLR_CLK_ENB_EMC_LATENCY, 0x1, reg);
		reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
								 CLK_OUT_ENB_EMC_CLR,
								 CLR_CLK_ENB_MC_CBPA, 0x1, reg);
		reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
								 CLK_OUT_ENB_EMC_CLR,
								 CLR_CLK_ENB_MC_CAPA, 0x1, reg);
		reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
								 CLK_OUT_ENB_EMC_CLR,
								 CLR_CLK_ENB_EMC, 0x1, reg);
		NV_CLK_RST_WRITE_REG(CLK_OUT_ENB_EMC_CLR, reg);
	}
	return TEGRABL_NO_ERROR;
}

#if defined(CONFIG_ENABLE_QSPI)
tegrabl_error_t tegrabl_enable_qspi_clk(void *priv_data)
{
	uint8_t instance = 0;
	char *qspi_src = NULL;

	struct qspi_clk_data *clk_data;
	struct clk_info *pclk_info;
	struct car_info *pcar_info;
	struct module_car_info *pmodule_car_info;
	uint32_t src_idx = 0;
	uint32_t temp_clk_div = 0;

	if (!priv_data)
		return TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 0);

	pmodule_car_info = &g_module_carinfo[TEGRABL_MODULE_QSPI];
	pcar_info = &pmodule_car_info->pcar_info[instance];
	pclk_info = &pcar_info->clock_info;
	clk_data = (struct qspi_clk_data *)priv_data;

	switch(clk_data->clk_src) {
	case TEGRABL_CLK_SRC_PLLP_OUT0:
		src_idx = QSPI_CLK_SRC_PLLP_OUT0;
		qspi_src = "pllp";
		break;
	case TEGRABL_CLK_SRC_PLLC4_MUXED:
		src_idx = QSPI_CLK_SRC_PLLC4_MUXED;

		/* Enable PLLC if it is not enabled already */
		tegrabl_car_init_pll_with_rate(TEGRABL_CLK_PLL_ID_PLLC4, 800000, NULL);
		/* Setting PLLC4_MUXED to 160MHz */
		tegrabl_car_set_clk_src_rate(TEGRABL_CLK_SRC_PLLC4_MUXED,
									 160000, NULL);
		qspi_src = "pllc4_muxed";
		break;
	case TEGRABL_CLK_SRC_CLK_M:
		src_idx = QSPI_CLK_SRC_CLK_M;
		qspi_src = "clk_m";
		break;
	default:
		pr_error("Invalid clock source for QSPI\n");
		return TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 1);
	}

	pr_info("Qspi clock source : %s\n", qspi_src);
	if (pclk_info->clk_enb_set_reg != 0U)
		NV_CLK_RST_WRITE_OFFSET(pclk_info->clk_enb_set_reg,
								0x1 << pcar_info->bit_offset);

	/* Update clk_src register with new divider */
	temp_clk_div = clk_data->clk_divisor;
	if ((temp_clk_div  % 2) != 0U) {
		pr_warn("Odd-N(%d) not supported. Decreasing, to round-up frequency\n",
				temp_clk_div);
		--temp_clk_div;
	}
	update_clk_src_reg(TEGRABL_MODULE_QSPI, instance, pclk_info,
					   temp_clk_div, src_idx);

	return tegrabl_qspi_clk_div_mode(0);
}
#endif

tegrabl_error_t tegrabl_assert_mem_rst(bool assert)
{
	uint32_t emc_reg = 0x0;
	uint32_t emcsb_reg = 0x0;

	if (assert) {
		emcsb_reg = NV_DRF_NUM(CLK_RST_CONTROLLER,
							   RST_DEV_EMCSB_SET, SET_SWR_MEM_RST, 0x1) |
					NV_DRF_NUM(CLK_RST_CONTROLLER,
							   RST_DEV_EMCSB_SET, SET_SWR_EMC_RST, 0x1);

		emc_reg = NV_DRF_NUM(CLK_RST_CONTROLLER,
							 RST_DEV_EMC_SET, SET_SWR_MEM_RST, 0x1) |
				  NV_DRF_NUM(CLK_RST_CONTROLLER,
							 RST_DEV_EMC_SET, SET_SWR_EMC_RST, 0x1);
		NV_CLK_RST_WRITE_REG(RST_DEV_EMC_SET, emc_reg);
		NV_CLK_RST_WRITE_REG(RST_DEV_EMCSB_SET, emcsb_reg);

	} else {
		emcsb_reg = NV_DRF_NUM(CLK_RST_CONTROLLER,
							   RST_DEV_EMCSB_CLR, CLR_SWR_MEM_RST, 0x1) |
					NV_DRF_NUM(CLK_RST_CONTROLLER,
							   RST_DEV_EMCSB_CLR, CLR_SWR_EMC_RST, 0x1);

		emc_reg = NV_DRF_NUM(CLK_RST_CONTROLLER,
							 RST_DEV_EMC_CLR, CLR_SWR_MEM_RST, 0x1) |
				  NV_DRF_NUM(CLK_RST_CONTROLLER,
							 RST_DEV_EMC_CLR, CLR_SWR_EMC_RST, 0x1);
		NV_CLK_RST_WRITE_REG(RST_DEV_EMC_CLR, emc_reg);
		NV_CLK_RST_WRITE_REG(RST_DEV_EMCSB_CLR, emcsb_reg);
	}

	return TEGRABL_NO_ERROR;
}

bool check_clk_src_enable(enum tegrabl_clk_src_id_t clk_src)
{
	switch (clk_src) {
	case TEGRABL_CLK_SRC_CLK_M:
		/* CLK_M is always enabled */
		return true;
	case TEGRABL_CLK_SRC_PLLP_OUT0:
		/* CHECK_PLL_ENABLE returns int type */
		return CHECK_PLL_ENABLE(PLLP) ? true : false;
	case TEGRABL_CLK_SRC_PLLM_OUT0:
		return CHECK_PLL_ENABLE(PLLM) ? true : false;
	case TEGRABL_CLK_SRC_PLLC4_OUT0_LJ:
		return CHECK_PLL_ENABLE(PLLC4) ? true : false;
	case TEGRABL_CLK_SRC_PLLE:
		return CHECK_PLL_ENABLE(PLLE) ? true : false;
	default:
		return false;
	}
}

tegrabl_error_t tegrabl_init_pllc4(void)
{
	uint32_t val = 0;

	if (CHECK_PLL_ENABLE(PLLC4) != 0U)
		return TEGRABL_NO_ERROR;

	pr_debug("PLLC4 not enabeld yet \n");

	/* Set SETUP4 bit */
	val = NV_CLK_RST_READ_REG(PLLC4_MISC);
	val = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLC4_MISC,
								PLLC4_SETUP, (1 << 4), val);

	NV_CLK_RST_WRITE_REG(PLLC4_MISC, val);

	/* Disable IDDQ */
	DISABLE_IDDQ(PLLC4, PLLC4_BASE);
	tegrabl_udelay(5ULL);
	/* Set SSC_STEP to 0x5 */
	val = NV_CLK_RST_READ_REG(PLLC4_SS_CTRL2);
	val = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLC4_SS_CTRL2,
							 PLLC4_SDM_SSC_STEP, 0x5, val);
	NV_CLK_RST_WRITE_REG(PLLC4_SS_CTRL2, val);

	/* Set PLL_EN_SDM to 0 */
	val = NV_CLK_RST_READ_REG(PLLC4_SS_CFG);
	val = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLC4_SS_CFG,
							 PLLC4_EN_SDM, 1, val);
	val = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLC4_SS_CFG,
							 PLLC4_EN_SSC, 1, val);
	NV_CLK_RST_WRITE_REG(PLLC4_SS_CFG, val);

	/* Set SSC_MAX and MIN */
	val = NV_CLK_RST_READ_REG(PLLC4_SS_CTRL1);
	val = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLC4_SS_CTRL1,
							 PLLC4_SDM_SSC_MAX, 0xC55, val);

	val = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLC4_SS_CTRL1,
							 PLLC4_SDM_SSC_MIN, 0x5CB, val);
	NV_CLK_RST_WRITE_REG(PLLC4_SS_CTRL1, val);

	/* program MN P values */
	val = NV_CLK_RST_READ_REG(PLLC4_BASE);
	val = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLC4_BASE,
							 PLLC4_DIVM, 2, val);

	val = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLC4_BASE,
							 PLLC4_DIVN, 0x28, val);
	NV_CLK_RST_WRITE_REG(PLLC4_BASE, val);

	ENABLE_PLL_LOCK(PLLC4, PLLC4_MISC);
	ENABLE_PLL(PLLC4);
	WAIT_PLL_LOCK(PLLC4, PLLC4);

	pr_debug("----PLLC4 ENABLED \n");
	return TEGRABL_NO_ERROR;
}

#if defined(CONFIG_ENABLE_CLOCK_PLLAON)
static void pllaon_toggle_reset(void)
{
	uint32_t val = NV_CLK_RST_READ_REG(PLLAON_MISC_0);

	val = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLAON_MISC_0,
			PLLAON_RESET, ENABLE, val);
	NV_CLK_RST_WRITE_REG(PLLAON_MISC_0, val);
	val = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLAON_MISC_0,
			PLLAON_RESET, DISABLE, val);
	NV_CLK_RST_WRITE_REG(PLLAON_MISC_0, val);
}

static int32_t aon_pll_lock(uint32_t timeout, uint32_t mask)
{
	uint32_t val = NV_CLK_RST_READ_REG(PLLAON_BASE);
	while (((val & mask) != mask) && timeout) {
		tegrabl_udelay(2);
		val = NV_CLK_RST_READ_REG(PLLAON_BASE);
		timeout--;
	}

	if (!timeout) {
		pr_debug("AO-PLL locking failed, mask %x\n", mask);
		return TEGRABL_ERR_LOCK_FAILED;
	}

	return TEGRABL_NO_ERROR;
}

/* Note: Referenece to below sequence is from bug #200049029 comment #84
 * WAR for HPLL fractional NDIV frequency issue is from bug #1713332
 * comment #106
 */
tegrabl_error_t tegrabl_init_pllaon(void)
{
	uint32_t val;
	uint32_t val2;
	struct tegrabl_mb1_bct *mb1bct;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (CHECK_PLL_ENABLE(PLLAON) != 0U)
		return TEGRABL_NO_ERROR;

	mb1bct = tegrabl_mb1bct_get();
	TEGRABL_ASSERT(mb1bct);

	/* Default settings incase if it is not mentioned in cfg file */
	if (mb1bct->clock.pllaon_divn == 0) {
		mb1bct->clock.pllaon_divn = 30;
		mb1bct->clock.pllaon_divm = 1;
		mb1bct->clock.pllaon_divp = 2;
	}

	/* Program MISC values */
	val = (NV_DRF_DEF(CLK_RST_CONTROLLER,
				PLLAON_MISC_0, PLLAON_RESET, DISABLE) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_0, PLLAON_EXT_FRU,
													AON_EXT_FRU_VAL) |
		NV_DRF_DEF(CLK_RST_CONTROLLER, PLLAON_MISC_0, PLLAON_PTS, DISABLE) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_0, PLLAON_LOOP_CTRL, 1));
	NV_CLK_RST_WRITE_REG(PLLAON_MISC_0, val);
	/* Dummy read to ensure write is successful */
	NV_CLK_RST_READ_REG(PLLAON_MISC_0);

	val = (NV_DRF_DEF(CLK_RST_CONTROLLER, PLLAON_MISC_1, PLLAON_IDDQ, ON) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_1, PLLAON_EXT_SUBINT, 0) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_1, PLLAON_DIVN_FRAC,
				   mb1bct->clock.pllaon_divn_frac));
	NV_CLK_RST_WRITE_REG(PLLAON_MISC_1, val);
	/* Dummy read to ensure write is successful */
	NV_CLK_RST_READ_REG(PLLAON_MISC_1);

	val =
		(NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_2, PLLAON_PLL_LD_MEM, 0x1F) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_2, PLLAON_PLL_FRUG,
													AON_PLL_FRUG_VAL) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_2, PLLAON_FLL_LD_MEM,
													AON_FLL_LD_MEM_VAL) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_2, PLLAON_FLL_DIV, 0) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_2, PLLAON_FLL_FRUG, 0x5));
	NV_CLK_RST_WRITE_REG(PLLAON_MISC_2, val);
	/* Dummy read to ensure write is successful */
	NV_CLK_RST_READ_REG(PLLAON_MISC_2);

	val =
		(NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_3, PLLAON_VREG10V_CTRL, 0) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_3, PLLAON_SETUP, (1<<14)) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_3, PLLAON_LDIV, 0) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_3, PLLAON_PLL_LD_TOL, 0x4));
	NV_CLK_RST_WRITE_REG(PLLAON_MISC_3, val);
	/* Dummy read to ensure write is successful */
	NV_CLK_RST_READ_REG(PLLAON_MISC_3);

	val = (NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_4, PLLAON_SEL_IREF, 1) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_4, PLLAON_KP_LO, 0x0) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_4, PLLAON_KP_HI, 0x7) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_4, PLLAON_KP_STEP_TIMER,
										AON_KP_STEP_TIMER_VAL) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_4, PLLAON_FRAC_STEP_TIMER,
										AON_FRAC_STEP_TIMER_VAL) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_4, PLLAON_FRAC_STEP,
										AON_FRAC_STEP_VAL));
	NV_CLK_RST_WRITE_REG(PLLAON_MISC_4, val);
	/* Dummy read to ensure write is successful */
	NV_CLK_RST_READ_REG(PLLAON_MISC_4);

	/* Disable Bypass, Enable Ref */
	val = NV_CLK_RST_READ_REG(PLLAON_BASE);
	val = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
								PLLAON_BASE, PLLAON_BYPASS, DISABLE, val);
	val = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
			PLLAON_BASE, PLLAON_REF_DIS, REF_ENABLE, val);
	NV_CLK_RST_WRITE_REG(PLLAON_BASE, val);
	/* Dummy read to ensure write is successful */
	NV_CLK_RST_READ_REG(PLLAON_BASE);

	/* Program M/N/P */
	val = (NV_DRF_DEF(CLK_RST_CONTROLLER, PLLAON_BASE, PLLAON_BYPASS, DISABLE) |
		NV_DRF_DEF(CLK_RST_CONTROLLER, PLLAON_BASE, PLLAON_ENABLE, DISABLE) |
		NV_DRF_DEF(CLK_RST_CONTROLLER,
					PLLAON_BASE, PLLAON_REF_DIS, REF_ENABLE) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_BASE, PLLAON_OVERRIDE, 1) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_BASE, PLLAON_DIVP,
											mb1bct->clock.pllaon_divp) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_BASE, PLLAON_DIVN,
												mb1bct->clock.pllaon_divn) |
		NV_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_BASE, PLLAON_DIVM,
												mb1bct->clock.pllaon_divm));
	NV_CLK_RST_WRITE_REG(PLLAON_BASE, val);
	/* Dummy read to ensure write is successful */
	NV_CLK_RST_READ_REG(PLLAON_BASE);

	/* Disable IDDQ */
	val = NV_CLK_RST_READ_REG(PLLAON_MISC_1);
	val = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
			PLLAON_MISC_1, PLLAON_IDDQ, OFF, val);
	NV_CLK_RST_WRITE_REG(PLLAON_MISC_1, val);
	val = NV_CLK_RST_READ_REG(PLLAON_MISC_1);
	tegrabl_udelay(1);

	pllaon_toggle_reset();

	/* Enable Clock */
	val = NV_CLK_RST_READ_REG(PLLAON_BASE);
	val = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
			PLLAON_BASE, PLLAON_ENABLE, ENABLE, val);
	NV_CLK_RST_WRITE_REG(PLLAON_BASE, val);

	/* Wait for PLL_FREQ lock */
	val = NV_CLK_RST_READ_REG(PLLAON_BASE);
	error = aon_pll_lock(AON_PLL_LOCK_MAX_TIMEOUT,
					CLK_RST_CONTROLLER_PLLAON_BASE_0_PLLAON_FREQ_LOCK_FIELD);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	tegrabl_udelay(2);

	/* Reset 14th bit of PLLAON_SETUP field.
	 * This is purely hradware known thing, but info on each bit of SETUP field
     * can be referred from HPLL12G_DYN_F1.doc and its available in perforce.
     */
	val = NV_CLK_RST_READ_REG(PLLAON_MISC_3);
	val2 = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLAON_MISC_3, PLLAON_SETUP, val);
	CLEAR_BIT(val2, 14);
	val = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLAON_MISC_3, PLLAON_SETUP,
							 val2, val);
	NV_CLK_RST_WRITE_REG(PLLAON_MISC_3, val);

	/* Wait for PLL lock */
	error = aon_pll_lock(AON_PLL_LOCK_MAX_TIMEOUT,
						 CLK_RST_CONTROLLER_PLLAON_BASE_0_PLLAON_LOCK_FIELD);

fail:
	return error;
}
#endif

void tegrabl_car_disable_plle(void)
{
	uint32_t reg = 0;

	reg = NV_CLK_RST_READ_REG(PLLE_BASE);
	reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLE_BASE,
							 PLLE_ENABLE, DISABLE, reg);
	NV_CLK_RST_WRITE_REG(PLLE_BASE, reg);

	reg = NV_CLK_RST_READ_REG(PLLE_MISC);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_MISC,
							 PLLE_IDDQ_OVERRIDE_VALUE, 1, reg);
	NV_CLK_RST_WRITE_REG(PLLE_MISC, reg);
}

tegrabl_error_t tegrabl_init_plle(void)
{
	uint32_t reg = 0;
	uint32_t val = 0;

	/* If PLLE is already enabled then just return */
	val = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLE_BASE,
							 PLLE_ENABLE, ENABLE, val);

	reg = NV_CLK_RST_READ_REG(PLLE_BASE);

	if ((val & reg) != 0U) {
		/* Check lock bit */
		reg = NV_CLK_RST_READ_REG(PLLE_MISC);
		if ((reg & CLK_RST_CONTROLLER_PLLE_MISC_0_PLLE_LOCK_FIELD) != 0U)
			return TEGRABL_NO_ERROR;
	}

	/* Configure and enable PLLE */
	reg = NV_CLK_RST_READ_REG(PLLE_BASE);
	reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLE_BASE,
							 PLLE_ENABLE, DISABLE, reg);
	NV_CLK_RST_WRITE_REG(PLLE_BASE, reg);

	reg = NV_CLK_RST_READ_REG(PLLE_MISC);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_MISC,
							 PLLE_IDDQ_SWCTL, 1, reg);
	NV_CLK_RST_WRITE_REG(PLLE_MISC, reg);

	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_MISC,
							 PLLE_IDDQ_OVERRIDE_VALUE, 1, reg);

	NV_CLK_RST_WRITE_REG(PLLE_MISC, reg);

	tegrabl_udelay(10);

	/* Set PLLREFE prod value */
	reg = NV_CLK_RST_READ_REG(PLLREFE_BASE);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLREFE_BASE,
							 PLLREFE_DIVM, 12, reg);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLREFE_BASE,
							 PLLREFE_DIVN, 125, reg);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLREFE_BASE,
							 PLLREFE_DIVP, 1, reg);
	NV_CLK_RST_WRITE_REG(PLLREFE_BASE, reg);

	/* Select XTAL as a SOURCE */
	reg = NV_CLK_RST_READ_REG(PLLE_AUX);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_AUX,
							 PLLE_REF_SEL_PLLREFE, 0, reg);
	NV_CLK_RST_WRITE_REG(PLLE_AUX, reg);

	reg = NV_CLK_RST_READ_REG(PLLE_MISC);

	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_MISC,
							 PLLE_IDDQ_OVERRIDE_VALUE, 0, reg);
	NV_CLK_RST_WRITE_REG(PLLE_MISC, reg);

	tegrabl_udelay(5);

	reg = NV_CLK_RST_READ_REG(PLLE_MISC1);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_MISC1,
							 PLLE_SDM_RESET, 0, reg);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_MISC1,
							 PLLE_EN_DITHER, 1, reg);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_MISC1,
							 PLLE_EN_SSC, 0, reg);
	NV_CLK_RST_WRITE_REG(PLLE_MISC1, reg);

	reg = NV_CLK_RST_READ_REG(PLLE_SS_CNTL);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_SS_CNTL,
							 PLLE_SSCINCINTRV, 0x20, reg);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_SS_CNTL,
							 PLLE_SSCINC, 1, reg);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_SS_CNTL,
							 PLLE_SSCINVERT, 0, reg);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_SS_CNTL,
							 PLLE_SSCCENTER, 0, reg);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_SS_CNTL,
							 PLLE_SSCBYP, 0, reg);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_SS_CNTL,
							 PLLE_INTERP_RESET, 0, reg);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_SS_CNTL,
							 PLLE_BYPASS_SS, 0, reg);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_SS_CNTL,
							 PLLE_SSCMAX, 0x25, reg);
	NV_CLK_RST_WRITE_REG(PLLE_SS_CNTL, reg);

	reg = NV_CLK_RST_READ_REG(PLLE_MISC1);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_MISC1,
							 PLLE_EN_SDM, 0, reg);
	NV_CLK_RST_WRITE_REG(PLLE_MISC1, reg);

	reg = NV_CLK_RST_READ_REG(PLLE_SS_CNTL1);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_SS_CNTL1,
							 PLLE_SDM_DIN, 0xF000, reg);
	NV_CLK_RST_WRITE_REG(PLLE_SS_CNTL1, reg);

	reg = NV_CLK_RST_READ_REG(PLLE_BASE);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_BASE,
							 PLLE_MDIV, 2, reg);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_BASE,
							 PLLE_NDIV, 125, reg);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_BASE,
							 PLLE_PLDIV_CML, 24, reg);
	NV_CLK_RST_WRITE_REG(PLLE_BASE, reg);

	reg = NV_CLK_RST_READ_REG(PLLE_MISC);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_MISC,
							 PLLE_LOCK_ENABLE, 0x1, reg);
	NV_CLK_RST_WRITE_REG(PLLE_MISC, reg);

	reg = NV_CLK_RST_READ_REG(PLLE_BASE);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLE_BASE,
							 PLLE_LOCK_OVERRIDE, 0, reg);
	NV_CLK_RST_WRITE_REG(PLLE_BASE, reg);

	reg = NV_CLK_RST_READ_REG(PLLE_BASE);
	reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, PLLE_BASE,
							 PLLE_ENABLE, ENABLE, reg);
	NV_CLK_RST_WRITE_REG(PLLE_BASE, reg);

	tegrabl_udelay(500);

	/* Poll to ensure PLLE is locked */
	do {
		reg = NV_CLK_RST_READ_REG(PLLE_MISC);
	} while (!(reg & CLK_RST_CONTROLLER_PLLE_MISC_0_PLLE_LOCK_FIELD));

	return TEGRABL_NO_ERROR;
}

struct utmipll_clock_params {
	uint32_t n;
	uint32_t m;
};

/* UTMI PLL needs output = 960 Mhz with osc input
 * Bug 1398118
 * Note: CLKIN/M ratio should between 12 and 38.4 Mhz
 */
static const struct utmipll_clock_params
	s_utmipll_base_info[TEGRABL_CLK_OSC_FREQ_MAX_VAL] = {
	/* DivN, DivM */
	{0x04A, 0x01}, /* For OscFreq_13 */
	{0x039,  0x1}, /* For OscFreq_16_8 */
	{    0,    0}, /* dummy field */
	{    0,    0}, /* dummy field */
	{0x032, 0x01}, /* For OscFreq_19_2 */
	{0x019, 0x01}, /* For OscFreq_38_4 */
	{    0,    0}, /* dummy field */
	{    0,    0}, /* dummy field */
	{0x050, 0x01}, /* For OscFreq_12 */
	{0x028, 0x02}, /* For OscFreq_48 */
	{    0,    0}, /* dummy field */
	{    0,    0}, /* dummy field */
	{0x04A, 0x02}  /* For OscFreq_26 */
};

struct usbpll_delay_params {
	uint32_t enable_dly;
	uint32_t stable_cnt;
	uint32_t active_dly;
	uint32_t xtal_freq_cnt;
};

static const struct usbpll_delay_params
	s_usbpll_delay_params[TEGRABL_CLK_OSC_FREQ_MAX_VAL] = {
	/* ENABLE_DLY, STABLE_CNT, ACTIVE_DLY, XTAL_FREQ_CNT */
	{0x02, 0x33, 0x09, 0x7F}, /* For OscFreq_13 8 */
	{0x03, 0x42, 0x0B, 0xA5}, /* For OscFreq_16_8 */
	{   0,    0,    0,    0}, /* dummy field */
	{   0,    0,    0,    0}, /* dummy field */
	{0x03, 0x4B, 0x0C, 0xBC}, /* For OscFreq_19_2 */
	{0x05, 0x96, 0x18, 0x177},  /* For OscFreq_38_4 */
	{   0,    0,    0,    0}, /* dummy field */
	{   0,    0,    0,    0}, /* dummy field */
	{0x02, 0x2F, 0x08, 0x76}, /* For OscFreq_12 */
	{0x06, 0xBC, 0X1F, 0x1D5}, /* For OscFreq_48 */
	{   0,    0,    0,    0}, /* dummy field */
	{   0,    0,    0,    0}, /* dummy field */
	{0x04, 0x66, 0x11, 0xFE}  /* For OscFreq_26 */
};

tegrabl_error_t tegrabl_init_utmipll(void)
{
	uint32_t reg_data;
	uint32_t lock_time;
	enum tegrabl_clk_osc_freq osc_freq = tegrabl_get_osc_freq();

	/* Check if xusb boot brought up UTMI PLL */
	reg_data = NV_CLK_RST_READ_REG(UTMIPLL_HW_PWRDN_CFG0);
	if ((NV_DRF_VAL(CLK_RST_CONTROLLER, UTMIPLL_HW_PWRDN_CFG0, UTMIPLL_LOCK,
		reg_data)) != 0U) {
		return TEGRABL_NO_ERROR;
	}

	reg_data = NV_CLK_RST_READ_REG(UTMIPLL_HW_PWRDN_CFG0);
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
					UTMIPLL_HW_PWRDN_CFG0,
					UTMIPLL_IDDQ_SWCTL,
					0x1, reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
					UTMIPLL_HW_PWRDN_CFG0,
					UTMIPLL_IDDQ_OVERRIDE_VALUE,
					0x0, reg_data);
	NV_CLK_RST_WRITE_REG(UTMIPLL_HW_PWRDN_CFG0, reg_data);

	tegrabl_udelay(10);

	/* Configure UTMI PLL dividers based on oscillator frequency. */
	reg_data = NV_CLK_RST_READ_REG(UTMIP_PLL_CFG0);
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP,
					PLL_CFG0,
					UTMIP_PLL_NDIV,
					s_utmipll_base_info[osc_freq].n,
					reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP,
					PLL_CFG0,
					UTMIP_PLL_MDIV,
					s_utmipll_base_info[osc_freq].m,
					reg_data);
	NV_CLK_RST_WRITE_REG(UTMIP_PLL_CFG0, reg_data);

	/* The following parameters control the bring up of the plls: */
	reg_data = NV_CLK_RST_READ_REG(UTMIP_PLL_CFG2);

	/** Set the Oscillator dependent automatic startup times etc **/
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP,
					PLL_CFG2,
					UTMIP_PLLU_STABLE_COUNT,
					0, /* Need not wait for PLLU to be stable if
						* using Osc_Clk as input */
					reg_data);

	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, UTMIP_PLL_CFG2,
					UTMIP_PLL_ACTIVE_DLY_COUNT, /* This is a dont care */
					s_usbpll_delay_params[osc_freq].active_dly,
					reg_data);

	NV_CLK_RST_WRITE_REG(UTMIP_PLL_CFG2, reg_data);

	/* Set PLL enable delay count and Crystal frequency count */
	reg_data = NV_CLK_RST_READ_REG(UTMIP_PLL_CFG1);

	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, UTMIP_PLL_CFG1,
					UTMIP_PLLU_ENABLE_DLY_COUNT,
					0, /* Need not wait for PLLU if input is OSC_CLK */
					reg_data);

	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, UTMIP_PLL_CFG1,
					UTMIP_XTAL_FREQ_COUNT,
					s_usbpll_delay_params[osc_freq].xtal_freq_cnt,
					reg_data);

	/************ End Automatic startup time programming ************/

	/********* Disable all force power ups and power downs ********/

	/* Power-up for PLLU_ENABLE */
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, UTMIP_PLL_CFG1,
					UTMIP_FORCE_PLL_ENABLE_POWERUP,
					0x1,
					reg_data);

	/* Remove power down for PLLU_ENABLE */
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, UTMIP_PLL_CFG1,
					UTMIP_FORCE_PLL_ENABLE_POWERDOWN,
					0x0,
					reg_data);

	/* Remove power down for PLL_ACTIVE */
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, UTMIP_PLL_CFG1,
					UTMIP_FORCE_PLL_ACTIVE_POWERDOWN,
					0x0,
					reg_data);

	NV_CLK_RST_WRITE_REG(UTMIP_PLL_CFG1, reg_data);

	/********* End Disabling all force power ups and power downs ********/

	lock_time = 100; /* 100 us */
	while (lock_time != 0U) {
		reg_data = NV_CLK_RST_READ_REG(UTMIPLL_HW_PWRDN_CFG0);
		if ((NV_DRF_VAL(CLK_RST_CONTROLLER, UTMIPLL_HW_PWRDN_CFG0,
			UTMIPLL_LOCK, reg_data)) != 0U) {
			break;
		}

		tegrabl_udelay(1);
		lock_time--;
	}

	/********** Remove power downs from UTMIP PLL Samplers bits ***********/

	reg_data = NV_CLK_RST_READ_REG(UTMIP_PLL_CFG2);

	/* Remove power down for PLLU_ENABLE */
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, UTMIP_PLL_CFG2,
					UTMIP_FORCE_PD_SAMP_A_POWERDOWN,
					0x0,
					reg_data);

	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, UTMIP_PLL_CFG2,
					UTMIP_FORCE_PD_SAMP_A_POWERUP,
					0x1,
					reg_data);

	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, UTMIP_PLL_CFG2,
					UTMIP_FORCE_PD_SAMP_B_POWERDOWN,
					0x0,
					reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, UTMIP_PLL_CFG2,
					UTMIP_FORCE_PD_SAMP_B_POWERUP,
					0x1,
					reg_data);

	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, UTMIP_PLL_CFG2,
					UTMIP_FORCE_PD_SAMP_C_POWERDOWN,
					0x0,
					reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, UTMIP_PLL_CFG2,
					UTMIP_FORCE_PD_SAMP_C_POWERUP,
					0x1,
					reg_data);

	NV_CLK_RST_WRITE_REG(UTMIP_PLL_CFG2, reg_data);
	/**** END Remove power downs from UTMIP PLL control bits ****/

	tegrabl_udelay(2);

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_clk_start_pll(
		enum tegrabl_clk_pll_id pll_id,
		uint32_t m,
		uint32_t n,
		uint32_t p,
		uint32_t misc1,
		uint32_t misc2,
		uint64_t *stable_time)
{
	uint32_t reg;
	uint32_t pllm_kvco;
	uint32_t pllm_kcp;

	TEGRABL_UNUSED(stable_time);
	TEGRABL_ASSERT(stable_time != NULL);

	/* We can now handle each PLL explicitly by making each PLL its own case
	 * construct. Unsupported PLL will fall into TEGRABL_ASSERT(0). misc1 and
	 * misc2 are flexible additional arguments for programming up to 2 32-bit
	 * registers. If more is required, one or both can be used as pointer to
	 * struct. */
	switch (pll_id) {
	case TEGRABL_CLK_PLL_ID_PLLM:
		NV_CLK_RST_WRITE_REG(PLLM_MISC1, misc1);

		pllm_kvco = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_MISC2,
						 PLLM_KVCO, misc2);

		pllm_kcp = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_MISC2,
						 PLLM_KCP, misc2);

		reg = NV_CLK_RST_READ_REG(PLLM_MISC2);
		reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLM_MISC2,
							 PLLM_KVCO, pllm_kvco, reg);
		reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLM_MISC2,
							 PLLM_KCP, pllm_kcp, reg);
		NV_CLK_RST_WRITE_REG(PLLM_MISC2, reg);

		/* Bypass disable, Ref enable, divm, divn, divp */
		UPDATE_PLL_BASE(PLLM, DISABLE, ENABLE, m, n, p);
		ENABLE_PLL(PLLM);

		WAIT_PLL_LOCK(PLLM, PLLM);
		break;

	case TEGRABL_CLK_PLL_ID_PLLMSB:
		NV_CLK_RST_WRITE_REG(PLLMSB_MISC1, misc1);

		pllm_kvco = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLMSB_MISC2,
						 PLLMSB_KVCO, misc2);

		pllm_kcp = NV_DRF_VAL(CLK_RST_CONTROLLER, PLLMSB_MISC2,
						 PLLMSB_KCP, misc2);

		reg = NV_CLK_RST_READ_REG(PLLMSB_MISC2);
		reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLMSB_MISC2,
							 PLLMSB_KVCO, pllm_kvco, reg);
		reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLMSB_MISC2,
							 PLLMSB_KCP, pllm_kcp, reg);
		NV_CLK_RST_WRITE_REG(PLLMSB_MISC2, reg);

		/* Bypass disable, Ref enable, divm, divn, divp */
		UPDATE_PLL_BASE(PLLMSB, DISABLE, ENABLE, m, n, p);
		ENABLE_PLL(PLLMSB);

		WAIT_PLL_LOCK(PLLMSB, PLLMSB);
		break;
	default:
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_init_pllm(NvBootSdramParams *pdata)
{
	uint32_t misc1;
	uint32_t misc2;
	uint64_t stable_time = 0;

	if (CHECK_PLL_ENABLE(PLLM) != 0U)
		return TEGRABL_NO_ERROR;

	if (!pdata)
		return TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 6);

	/*Pack PLLM params into misc1 and misc2 */
	misc1 = NV_DRF_NUM(MISC1, CLK_RST_CONTROLLER_PLLM_MISC1, PLLM_SETUP,
					   pdata->PllMSetupControl);
	misc2 = NV_DRF_NUM(MISC2, CLK_RST_CONTROLLER_PLLM_MISC2, PLLM_KVCO,
					   pdata->PllMKVCO) | \
			NV_DRF_NUM(MISC2, CLK_RST_CONTROLLER_PLLM_MISC2, PLLM_KCP,
					   pdata->PllMKCP);

	if (pdata->McEmemAdrCfgChannelEnable & 0x3) {
		DISABLE_IDDQ(PLLM, PLLM_MISC2);
		tegrabl_udelay(5);

		ENABLE_PLL_LOCK(PLLM, PLLM_MISC2);

		/* Start PLLM for EMC/MC */
		tegrabl_clk_start_pll(TEGRABL_CLK_PLL_ID_PLLM,
							  pdata->PllMInputDivider,
							  pdata->PllMFeedbackDivider,
							  pdata->PllMPostDivider,
							  misc1,
							  misc2,
							  &stable_time);
	}

	if (pdata->McEmemAdrCfgChannelEnable & 0xC) {
		DISABLE_IDDQ(PLLMSB, PLLMSB_MISC2);
		tegrabl_udelay(5);

		ENABLE_PLL_LOCK(PLLMSB, PLLMSB_MISC2);

		/* Start PLLM for EMC/MC */
		tegrabl_clk_start_pll(TEGRABL_CLK_PLL_ID_PLLMSB,
							  pdata->PllMInputDivider,
							  pdata->PllMFeedbackDivider,
							  pdata->PllMPostDivider,
							  misc1,
							  misc2,
							  &stable_time);
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_sata_pll_cfg(void)
{
	uint32_t reg = 0;

	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, SATA_PLL_CFG1,
							 SATA_LANE_IDDQ2_PADPLL_RESET_DLY, 0, reg);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, SATA_PLL_CFG1,
							 SATA_PADPLL_IDDQ2LANE_SLUMBER_DLY, 0, reg);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, SATA_PLL_CFG1,
							 SATA_PADPLL_PU_POST_DLY, 0x20, reg);
	reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, SATA_PLL_CFG1,
							 SATA_LANE_IDDQ2_PADPLL_IDDQ_DLY, 8, reg);
	NV_CLK_RST_WRITE_REG(SATA_PLL_CFG1, reg);

	return TEGRABL_NO_ERROR;
}

uint32_t tegrabl_get_pll_freq_khz(enum tegrabl_clk_pll_id pll_id)
{
	uint32_t m;
	uint32_t n;
	uint32_t p;
	uint32_t pllref_khz = tegrabl_get_pllref_khz();

	switch (pll_id) {
	case TEGRABL_CLK_PLL_ID_PLLM:
		FILL_MNP(PLLM, m, n, p);
		break;
	case TEGRABL_CLK_PLL_ID_PLLC4:
		FILL_MNP(PLLC4, m, n, p);
		break;
	default:
		CLOCK_BUG(TEGRABL_MODULE_CLKRST, 0,
				  "pll_id %u not supported (func = %s)\n", pll_id, __func__);
		return 0;
	}
	return ((pllref_khz / m) * n) / p;
}
