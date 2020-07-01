/*
 * Copyright (c) 2017-2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define PLLP_FIXED_FREQ_KHZ_13000            13000
#define PLLP_FIXED_FREQ_KHZ_216000          216000
#define PLLP_FIXED_FREQ_KHZ_408000          408000
#define PLLP_FIXED_FREQ_KHZ_432000          432000

/**
 * @brief enum for plls that might be used. Not all of them might be
 * supported. Add new plls to this list and update the clock driver add
 * support for the new pll.
 */
/* macro tegrabl clk pll id */
typedef uint32_t tegrabl_clk_pll_id_t;
#define TEGRABL_CLK_PLL_ID_PLLP 0					/* 0x0 */
#define TEGRABL_CLK_PLL_ID_PLLC4 1					/* 0x1 */
#define TEGRABL_CLK_PLL_ID_PLLD 2					/* 0x2 */
#define TEGRABL_CLK_PLL_ID_PLLD2 3					/* 0x3 */
	/* TEGRABL_CLK_PLL_ID_PLLD3 at 14 */
#define TEGRABL_CLK_PLL_ID_PLLDP 4					/* 0x4 */
#define TEGRABL_CLK_PLL_ID_PLLE 5					/* 0x5 */
#define TEGRABL_CLK_PLL_ID_PLLM 6					/* 0x6 */
#define TEGRABL_CLK_PLL_ID_SATA_PLL 7				/* 0x7 */
#define TEGRABL_CLK_PLL_ID_UTMI_PLL 8				/* 0x8 */
#define TEGRABL_CLK_PLL_ID_XUSB_PLL 9				/* 0x9 */
#define TEGRABL_CLK_PLL_ID_AON_PLL 10				/* 0xA */
#define TEGRABL_CLK_PLL_ID_PLLDISPHUB 11				/* 0xB */
#define TEGRABL_CLK_PLL_ID_PLL_NUM 12				/* 0xC */
#define TEGRABL_CLK_PLL_ID_PLLMSB 13					/* 0xD */
#define TEGRABL_CLK_PLL_ID_PLLD3 14					/* 0xE */
#define TEGRABL_CLK_PLL_ID_PLLC 15					/* 0xF */
#define TEGRABL_CLK_PLL_ID_PLLC2 16					/* 0x10 */
#define TEGRABL_CLK_PLL_ID_PLLC3 17					/* 0x11 */
#define TEGRABL_CLK_PLL_ID_MAX 18					/* 0x12 */
#define TEGRABL_CLK_PLL_ID_PLL_FORCE32 2147483647ULL	/* 0x7FFFFFFF */

/**
 * @brief - enum for possible module clock divisors
 * @TEGRABL_CLK_DIV_TYPE_REGULAR - Divide by (N + 1)
 * @TEGRABL_CLK_DIV_TYPE_FRACTIONAL - Divide by (N/2 + 1)
 * where N is the divisor value written to the clock source register
 * one of the PLLs). Not all of them are supported.
 */
/* macro tegrabl clk div type */
typedef uint32_t tegrabl_clk_div_type_t;
#define TEGRABL_CLK_DIV_TYPE_INVALID 0x0U
#define TEGRABL_CLK_DIV_TYPE_REGULAR 0x1U
#define TEGRABL_CLK_DIV_TYPE_FRACTIONAL 0x2U
#define TEGRABL_CLK_DIV_TYPE_FORCE32 0x7fffffffU

/**
 * @brief - enum for possible clock sources
 * Add new sources to this list and update tegrabl_clk_get_src_freq()
 * to take care of the newly added source (usually a derivative of one of
 * one of the PLLs). Not all of them are supported.
 */
/* macro tegrabl clk src id */
typedef uint32_t tegrabl_clk_src_id_t;
#define TEGRABL_CLK_SRC_INVALID 0x0
#define TEGRABL_CLK_SRC_CLK_M 0x1
#define TEGRABL_CLK_SRC_CLK_S 0x2 /* 0x2 */
#define TEGRABL_CLK_SRC_PLLP_OUT0 0x3
#define TEGRABL_CLK_SRC_PLLM_OUT0 0x4 /* 0x4 */
#define TEGRABL_CLK_SRC_PLLC_OUT0 0x5
#define TEGRABL_CLK_SRC_PLLC4_MUXED 0x6 /* 0x6 */
#define TEGRABL_CLK_SRC_PLLC4_VCO 0x7
#define TEGRABL_CLK_SRC_PLLC4_OUT0_LJ 0x8 /* 0x8 */
#define TEGRABL_CLK_SRC_PLLC4_OUT1 0x9
#define TEGRABL_CLK_SRC_PLLC4_OUT1_LJ 0xa /* 0xA */
#define TEGRABL_CLK_SRC_PLLC4_OUT2 0xb
#define TEGRABL_CLK_SRC_PLLC4_OUT2_LJ 0xc /* 0xC */
#define TEGRABL_CLK_SRC_PLLE 0xd
#define TEGRABL_CLK_SRC_PLLAON_OUT 0xe /* 0xE */
#define TEGRABL_CLK_SRC_PLLD_OUT1 0xf
#define TEGRABL_CLK_SRC_PLLD2_OUT0 0x10 /* 0x10 */
#define TEGRABL_CLK_SRC_PLLD3_OUT0 0x11
#define TEGRABL_CLK_SRC_PLLDP 0x12 /* 0x12 */
#define TEGRABL_CLK_SRC_NVDISPLAY_P0_CLK 0x13
#define TEGRABL_CLK_SRC_NVDISPLAY_P1_CLK 0x14 /* 0x14 */
#define TEGRABL_CLK_SRC_NVDISPLAY_P2_CLK 0x15
#define TEGRABL_CLK_SRC_SOR0 0x16 /* 0x16*/
#define TEGRABL_CLK_SRC_SOR1 0x17
#define TEGRABL_CLK_SRC_SOR_SAFE_CLK 0x18 /* 0x18 */
#define TEGRABL_CLK_SRC_SOR0_PAD_CLKOUT 0x19
#define TEGRABL_CLK_SRC_SOR1_PAD_CLKOUT 0x1a /* 0x1A */
#define TEGRABL_CLK_SRC_DFLLDISP_DIV 0x1b
#define TEGRABL_CLK_SRC_PLLDISPHUB_DIV 0x1c /* 0x1C */
#define TEGRABL_CLK_SRC_PLLDISPHUB 0x1d
#define TEGRABL_CLK_SRC_PLLC2_OUT0 0x1e /* 0x1E */
#define TEGRABL_CLK_SRC_PLLC3_OUT0 0x1f
#define TEGRABL_CLK_SRC_DUMMY 0x20 /* 0x21 */
#define TEGRABL_CLK_SRC_NUM 0x7fffffff


/*
 * @brief - enum for possible set of oscillator frequencies
 * supported in the internal API + invalid (measured but not in any valid band)
 * + unknown (not measured at all)
 * Define tegrabl_clk_osc_freq here to have the correct collection of
 * oscillator frequencies.
 */
/* macro tegrabl clk osc freq */
typedef uint32_t tegrabl_clk_osc_freq_t;
	/* Specifies an oscillator frequency of 13MHz.*/
#define TEGRABL_CLK_OSC_FREQ_13 CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC13

	/* Specifies an oscillator frequency of 19.2MHz. */
#define TEGRABL_CLK_OSC_FREQ_19_2 CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC19P2

	/* Specifies an oscillator frequency of 12MHz. */
#define TEGRABL_CLK_OSC_FREQ_12 CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC12

	/* Specifies an oscillator frequency of 26MHz. */
#define TEGRABL_CLK_OSC_FREQ_26 CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC26

	/* Specifies an oscillator frequency of 16.8MHz. */
#define TEGRABL_CLK_OSC_FREQ_16_8 CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC16P8

	/* Specifies an oscillator frequency of 38.4MHz. */
#define TEGRABL_CLK_OSC_FREQ_38_4 CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC38P4

	/* Specifies an oscillator frequency of 48MHz. */
#define TEGRABL_CLK_OSC_FREQ_48 CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC48

#define TEGRABL_CLK_OSC_FREQ_NUM 7 /* dummy to get number of frequencies */
#define TEGRABL_CLK_OSC_FREQ_MAX_VAL 13 /* dummy to get the max enum value */
#define TEGRABL_CLK_OSC_FREQ_UNKNOWN 15 /* illegal/undefined frequency */
#define TEGRABL_CLK_OSC_FREQ_FORCE32 0x7fffffff

/* FIXME: Add relevant code and remove below stub functions */
/**
 * @brief Set Qspi_clk ddr divisor mode for read operation
 *
 * @param instance qspi instance
 *
 * @retval TEGRABL_NO_ERROR No err
 */
static inline tegrabl_error_t tegrabl_qspi_ddr_enable(uint8_t instance)
{
	TEGRABL_UNUSED(instance);
	return TEGRABL_NO_ERROR;
}

/**
 * @brief Set Qspi_clk sdr divisor mode for read operation
 *
 * @param instance qspi instance
 *
 * @retval TEGRABL_NO_ERROR No err
 */
static inline tegrabl_error_t tegrabl_qspi_sdr_enable(uint8_t instance)
{
	TEGRABL_UNUSED(instance);
	return TEGRABL_NO_ERROR;
}

