/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDE_TEGRABL_CLK_RST_SOC_H
#define INCLUDE_TEGRABL_CLK_RST_SOC_H

/* Remove when tegrabl sdram param header is ready */
#define NvU32 uint32_t

#include <tegrabl_debug.h>
#include <arclk_rst.h>
#include <tegrabl_clock.h>
#include <nvboot_sdram_param.h>

/* PLL reference divider */
extern uint32_t g_pllrefdiv;

#define PLL_BYPASS                  0
#define PLLP_FREQ_KHZ               PLLP_FIXED_FREQ_KHZ_408000

#define MISC1_CLK_RST_CONTROLLER_PLLM_MISC1_0_PLLM_SETUP_RANGE	(23) : (0)
#define MISC2_CLK_RST_CONTROLLER_PLLM_MISC2_0_PLLM_KVCO_RANGE	(0) : (0)
#define MISC2_CLK_RST_CONTROLLER_PLLM_MISC2_0_PLLM_KCP_RANGE	(2) : (1)

/* This is the default source frequency for all
 * LSIO modules on the FPGA. Might be not true for
 * a few of the modules listed in the table which are
 * not LSIO modules. In such cases, simply enabling
 * the clock should be sufficient. This is because
 * clock divider logic is only present for LSIO modules.
 */
#define T18X_FPGA_DEFAULT_SRC_FREQ_KHZ	20000

/*
 * Set of valid count ranges per frequency.
 * Measuring 13 gives exactly 406 e.g.
 * The chosen range parameter is:
 * - more than the expected combined frequency deviation
 * - less than half the  relative distance between 12 and 13
 * - expressed as a ratio against a power of two to avoid floating point
 * - so that intermediate overflow is not possible
 *
 * The chosen factor is 1/64 or slightly less than 1.6% = 2^-6
 * Rounding is performed in such way as to guarantee at least the range
 * that is down for min and up for max
 * the range macros receive the frequency in kHz as argument
 * division by 32 kHz then becomes a shift by 5 to the right
 *
 * The macros are defined for a frequency of 32768 Hz (not 32000 Hz).
 * They use 2^-5 ranges, or about 3.2% and dispense with the rounding.
 * Also need to use the full value in Hz in the macro
 */

#define TEGRABL_CLK_MIN_RANGE(F) (((F) - ((F)>>5) - (1<<15) + 1) >> 15)
#define TEGRABL_CLK_MAX_RANGE(F) (((F) + ((F)>>5) + (1<<15) - 1) >> 15)

/* For an easier ECO (keeping same number of instructions), we need a
 * special case for 12 min range
 */
#define TEGRABL_CLK_MIN_CNT_12 (TEGRABL_CLK_MIN_RANGE(12000000) - 1)
#define TEGRABL_CLK_MAX_CNT_12 TEGRABL_CLK_MAX_RANGE(12000000)

#define TEGRABL_CLK_MIN_CNT_13 TEGRABL_CLK_MIN_RANGE(13000000)
#define TEGRABL_CLK_MAX_CNT_13 TEGRABL_CLK_MAX_RANGE(13000000)

#define TEGRABL_CLK_MIN_CNT_16_8 TEGRABL_CLK_MIN_RANGE(16800000)
#define TEGRABL_CLK_MAX_CNT_16_8 TEGRABL_CLK_MAX_RANGE(16800000)

#define TEGRABL_CLK_MIN_CNT_19_2 TEGRABL_CLK_MIN_RANGE(19200000)
#define TEGRABL_CLK_MAX_CNT_19_2 TEGRABL_CLK_MAX_RANGE(19200000)

#define TEGRABL_CLK_MIN_CNT_26 TEGRABL_CLK_MIN_RANGE(26000000)
#define TEGRABL_CLK_MAX_CNT_26 TEGRABL_CLK_MAX_RANGE(26000000)

#define TEGRABL_CLK_MIN_CNT_38_4 TEGRABL_CLK_MIN_RANGE(38400000)
#define TEGRABL_CLK_MAX_CNT_38_4 TEGRABL_CLK_MAX_RANGE(38400000)

#define TEGRABL_CLK_MIN_CNT_48 TEGRABL_CLK_MIN_RANGE(48000000)
#define TEGRABL_CLK_MAX_CNT_48 TEGRABL_CLK_MAX_RANGE(48000000)

uint32_t tegrabl_get_pllref_khz(void);

tegrabl_error_t tegrabl_enable_mem_clk(
		bool enable, void *priv_data);

#if defined(CONFIG_ENABLE_QSPI)
/**
 * @brief enables the qspi clk
 *
 * @param a pointer to qspi related private data
 *
 * @retval TEGRABL_NO_ERROR initialization if successful
 */
tegrabl_error_t tegrabl_enable_qspi_clk(void *priv_data);
#endif

tegrabl_error_t tegrabl_assert_mem_rst(bool assert);

tegrabl_error_t tegrabl_init_pllm(NvBootSdramParams *pdata);

tegrabl_error_t tegrabl_init_pllc4(void);

tegrabl_error_t tegrabl_init_pllaon(void);

tegrabl_error_t tegrabl_init_plle(void);

tegrabl_error_t tegrabl_init_utmipll(void);

tegrabl_error_t tegrabl_clk_start_pll(
		tegrabl_clk_pll_id_t pll_id,
		uint32_t m,
		uint32_t n,
		uint32_t p,
		uint32_t misc1,
		uint32_t misc2,
		uint64_t *stable_time);

tegrabl_error_t tegrabl_sata_pll_cfg(void);

uint32_t tegrabl_get_pll_freq_khz(tegrabl_clk_pll_id_t pll_id);

bool check_clk_src_enable(tegrabl_clk_src_id_t clk_src);

#endif /* INCLUDE_TEGRABL_CLK_RST_SOC_H */
