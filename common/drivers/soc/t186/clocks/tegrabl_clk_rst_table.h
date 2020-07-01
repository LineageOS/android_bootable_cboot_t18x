/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDE_TEGRABL_CLK_RST_TABLE_H
#define INCLUDE_TEGRABL_CLK_RST_TABLE_H

#include "arclk_rst.h"
#include "tegrabl_clock.h"
#include "tegrabl_clk_rst_soc.h"

#define APE_CLOCK_FREQ			(300 * 1000 * 1000)

#define SRC_ID_LIST(s1, s2, s3, s4, s5, s6, s7, s8)	\
{													\
		TEGRABL_CLK_SRC_##s1,						\
		TEGRABL_CLK_SRC_##s2,						\
		TEGRABL_CLK_SRC_##s3,						\
		TEGRABL_CLK_SRC_##s4,						\
		TEGRABL_CLK_SRC_##s5,						\
		TEGRABL_CLK_SRC_##s6,						\
		TEGRABL_CLK_SRC_##s7,						\
}

/* Please specify all modules that need to be able to switch
 * sources dynamically below.
 *-----------------------------------------------------

 * Specify sources in the order of their bit-indexes
 * listed in arclk_rst.html. There should be a total of
 * eight sources for each module. For bit-indexes that
 * do not have a mapping, use DUMMY. See qspi below,
 * for example.
 */
enum tegrabl_clk_src_id_t g_sdmmc_src_list[] =
	SRC_ID_LIST(PLLP_OUT0, PLLC4_OUT2_LJ,				\
				PLLC4_OUT0_LJ, PLLC4_OUT2, PLLC4_OUT1,	\
				PLLC4_OUT1_LJ, CLK_M, PLLC4_VCO);

enum tegrabl_clk_src_id_t g_qspi_src_list[] =
	SRC_ID_LIST(PLLP_OUT0, DUMMY, DUMMY, DUMMY,			\
				PLLC4_MUXED, DUMMY, CLK_M, DUMMY);

enum tegrabl_clk_src_id_t g_sata_src_list[] =
	SRC_ID_LIST(PLLP_OUT0, DUMMY, DUMMY, DUMMY,			\
				DUMMY, DUMMY, CLK_M, DUMMY);

enum tegrabl_clk_src_id_t g_i2c_src_list[] =
	SRC_ID_LIST(PLLP_OUT0, DUMMY, DUMMY, DUMMY,			\
				DUMMY, DUMMY, CLK_S, CLK_M);

enum tegrabl_clk_src_id_t g_i2c2_src_list[] =
	SRC_ID_LIST(PLLP_OUT0, PLLC_OUT0, PLLAON_OUT,		\
				DUMMY, DUMMY, DUMMY, CLK_S, CLK_M);

/* -----------------------------------------------------
 * End of module source table list
 */

/* For some reason, UART runs properly only if the CAR divisor is
 * used instead of the internal DLL/DLM divider. Remove this macro
 * when the issue is fixed.
 */
#define UART_FPGA_FREQ_KHZ 1904

/* Module CAR tables begin here
 * ----------------------------------------------------------
 * CLK_RST_ARRAY should be used for modules that don't need
 * to be able to dynamically switch clock sources and stay
 * bound to a single clock source.
 *
 * CLK_RST_ARRAY_ALLOW_SWITCH should be used for modules
 * that dynamically switch sources.
 */

struct car_info g_uart_car_info[] = {
#if defined(CONFIG_ENABLE_FPGA)
	CLK_RST_ARRAY(UARTA, PLLP_OUT0, UART_FPGA_FREQ_KHZ, FRACTIONAL),
	CLK_RST_ARRAY(UARTB, PLLP_OUT0, UART_FPGA_FREQ_KHZ, FRACTIONAL),
	CLK_RST_ARRAY(UARTC, PLLP_OUT0, UART_FPGA_FREQ_KHZ, FRACTIONAL),
	CLK_RST_ARRAY(UARTD, PLLP_OUT0, UART_FPGA_FREQ_KHZ, FRACTIONAL),
	CLK_RST_ARRAY(UARTE, PLLP_OUT0, UART_FPGA_FREQ_KHZ, FRACTIONAL),
	CLK_RST_ARRAY(UARTF, PLLP_OUT0, UART_FPGA_FREQ_KHZ, FRACTIONAL),
	CLK_RST_ARRAY(UARTG, PLLP_OUT0, UART_FPGA_FREQ_KHZ, FRACTIONAL),
#else
	CLK_RST_ARRAY(UARTA, PLLP_OUT0, 408000, FRACTIONAL),
	CLK_RST_ARRAY(UARTB, PLLP_OUT0, 408000, FRACTIONAL),
	CLK_RST_ARRAY(UARTC, PLLP_OUT0, 408000, FRACTIONAL),
	CLK_RST_ARRAY(UARTD, PLLP_OUT0, 408000, FRACTIONAL),
	CLK_RST_ARRAY(UARTE, PLLP_OUT0, 408000, FRACTIONAL),
	CLK_RST_ARRAY(UARTF, PLLP_OUT0, 408000, FRACTIONAL),
	CLK_RST_ARRAY(UARTG, PLLP_OUT0, 408000, FRACTIONAL),
#endif
};

#define PWM_FREQ_KHZ	102000

struct car_info g_pwm_car_info[] = {
	CLK_RST_ARRAY(PWM1, PLLP_OUT0, PWM_FREQ_KHZ, FRACTIONAL),
	CLK_RST_ARRAY(PWM2, PLLP_OUT0, PWM_FREQ_KHZ, FRACTIONAL),
	CLK_RST_ARRAY(PWM3, PLLP_OUT0, PWM_FREQ_KHZ, FRACTIONAL),
	CLK_RST_ARRAY(PWM4, PLLP_OUT0, PWM_FREQ_KHZ, FRACTIONAL),
	CLK_RST_ARRAY(PWM5, PLLP_OUT0, PWM_FREQ_KHZ, FRACTIONAL),
	CLK_RST_ARRAY(PWM6, PLLP_OUT0, PWM_FREQ_KHZ, FRACTIONAL),
	CLK_RST_ARRAY(PWM7, PLLP_OUT0, PWM_FREQ_KHZ, FRACTIONAL),
	CLK_RST_ARRAY(PWM8, PLLP_OUT0, PWM_FREQ_KHZ, FRACTIONAL),
};

struct car_info g_sdmmc_car_info[] = {
#if defined(CONFIG_ENABLE_FPGA)
	CLK_RST_ARRAY_ALLOW_SWITCH(SDMMC1, sdmmc, CLK_M,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, FRACTIONAL),
	CLK_RST_ARRAY_ALLOW_SWITCH(SDMMC2, sdmmc, CLK_M,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, FRACTIONAL),
	CLK_RST_ARRAY_ALLOW_SWITCH(SDMMC3, sdmmc, CLK_M,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, FRACTIONAL),
	CLK_RST_ARRAY_ALLOW_SWITCH(SDMMC4, sdmmc, CLK_M,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, FRACTIONAL),
#else
	CLK_RST_ARRAY_ALLOW_SWITCH(SDMMC1, sdmmc, PLLP_OUT0,
							   102000, FRACTIONAL),
	CLK_RST_ARRAY_ALLOW_SWITCH(SDMMC2, sdmmc, PLLP_OUT0,
							   102000, FRACTIONAL),
	CLK_RST_ARRAY_ALLOW_SWITCH(SDMMC3, sdmmc, PLLP_OUT0,
							   102000, FRACTIONAL),
	CLK_RST_ARRAY_ALLOW_SWITCH(SDMMC4, sdmmc, PLLP_OUT0,
							   102000, FRACTIONAL),
#endif
};

struct car_info g_qspi_car_info[] = {
#if defined(CONFIG_ENABLE_FPGA)
	CLK_RST_ARRAY_ALLOW_SWITCH(QSPI, qspi, CLK_M,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, FRACTIONAL),
#else
	CLK_RST_ARRAY_ALLOW_SWITCH(QSPI, qspi, PLLP_OUT0, 166000, FRACTIONAL),
#endif
};

struct car_info g_sata_car_info[] = {
	CLK_RST_ARRAY_ALLOW_SWITCH(SATA, sata, PLLP_OUT0, 102000, FRACTIONAL),
};

struct car_info g_sata_oob_car_info[] = {
	CLK_RST_NO_RST_ARRAY_ALLOW_SWITCH(SATA, SATA_OOB, 1, sata, PLLP_OUT0, 204000, FRACTIONAL),
};

struct car_info g_sata_cold_car_info[] = {
	CLK_RST_NO_CLK_ARRAY(SATA, 1),
};

struct car_info g_se_car_info[] = {
#if defined(CONFIG_ENABLE_FPGA)
	CLK_RST_ARRAY(SE, CLK_M, 13000, FRACTIONAL),
#else
	CLK_RST_ARRAY(SE, PLLP_OUT0, 204000, FRACTIONAL),
#endif
};

struct car_info g_xusb_host_car_info[] = {
	CLK_RST_NO_SRC_ARRAY(XUSB, 0),
};

struct car_info g_xusb_dev_car_info[] = {
	CLK_RST_NO_SRC_ARRAY(XUSB, 1),
};

struct car_info g_xusb_padctl_car_info[] = {
	CLK_RST_NO_SRC_ARRAY(XUSB, 2),
};

struct car_info g_xusb_ss_car_info[] = {
	CLK_RST_NO_SRC_ARRAY(XUSB, 3),
};

struct car_info g_xusbf_car_info[] = {
	CLK_RST_NO_SRC_ARRAY(XUSB, 4), /* Dummy */
};

struct car_info g_i2c_car_info[] = {
#if defined(CONFIG_ENABLE_FPGA)
	CLK_RST_ARRAY_ALLOW_SWITCH(I2C1, i2c, CLK_M,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, REGULAR),
	CLK_RST_ARRAY_ALLOW_SWITCH(I2C2, i2c2, CLK_M,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, REGULAR),
	CLK_RST_ARRAY_ALLOW_SWITCH(I2C3, i2c, CLK_M,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, REGULAR),
	CLK_RST_ARRAY_ALLOW_SWITCH(I2C4, i2c, CLK_M,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, REGULAR),
	CLK_RST_ARRAY_ALLOW_SWITCH(I2C5, i2c, CLK_M,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, REGULAR),
#else
	CLK_RST_ARRAY_ALLOW_SWITCH(I2C1, i2c, PLLP_OUT0,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, REGULAR),
	CLK_RST_ARRAY_ALLOW_SWITCH(I2C2, i2c2, PLLP_OUT0,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, REGULAR),
	CLK_RST_ARRAY_ALLOW_SWITCH(I2C3, i2c, PLLP_OUT0,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, REGULAR),
	CLK_RST_ARRAY_ALLOW_SWITCH(I2C4, i2c, PLLP_OUT0,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, REGULAR),
	CLK_RST_ARRAY_ALLOW_SWITCH(I2C5, i2c, PLLP_OUT0,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, REGULAR),
	CLK_RST_ARRAY_ALLOW_SWITCH(I2C6, i2c, PLLP_OUT0,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, REGULAR),
	CLK_RST_ARRAY_ALLOW_SWITCH(I2C7, i2c, PLLP_OUT0,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, REGULAR),
	CLK_RST_ARRAY_ALLOW_SWITCH(I2C8, i2c, PLLP_OUT0,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, REGULAR),
	CLK_RST_ARRAY_ALLOW_SWITCH(I2C9, i2c, PLLP_OUT0,
				   T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, REGULAR),
#endif
};

struct car_info g_kfuse_car_info[] = {
	CLK_RST_NO_CLK_ARRAY(KFUSE, 0),
};

struct car_info g_host1x_car_info[] = {
#if defined(CONFIG_ENABLE_FPGA)
	CLK_RST_ARRAY(HOST1X, CLK_M,
				  T18X_FPGA_DEFAULT_SRC_FREQ_KHZ, FRACTIONAL),
#else
	CLK_RST_ARRAY(HOST1X, PLLP_OUT0, 104000, FRACTIONAL),
#endif
};

struct car_info g_dvfs_car_info[] = {
	CLK_RST_NO_SRC_ARRAY(DVFS, 0),
};

struct car_info g_nvdec_car_info[] = {
	CLK_RST_NO_SRC_ARRAY(NVDEC, 0),
};

struct car_info g_gpcdma_car_info[] = {
	CLK_RST_NO_CLK_ARRAY(AXI_CBB, 1),
};

struct car_info g_bpmpdma_car_info[] = {
	CLK_RST_NO_CLK_ARRAY(BPMP_DMA, 0),
};

struct car_info g_spedma_car_info[] = {
	CLK_RST_NO_CLK_ARRAY(AON_DMA, 0),
};

struct car_info g_soc_therm_car_info[] = {
	CLK_RST_NO_SRC_ARRAY(SOC_THERM, 0),
};

struct car_info g_ape_car_info[] = {
	CLK_RST_ARRAY(APE, PLLP_OUT0, APE_CLOCK_FREQ, FRACTIONAL),
};

struct car_info g_adsp_car_info[] = {
	CLK_RST_NO_SRC_ARRAY(ADSP, 0),
};

struct car_info g_apb2ape_car_info[] = {
	CLK_RST_NO_SRC_NO_RST_ARRAY(APB2APE, 0),
};

#endif /* INCLUDE_TEGRABL_CLK_RST_TABLE_H */
