/*
 * Copyright (c) 2016 - 2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#define MODULE TEGRABL_ERR_CLK_RST

#include <tegrabl_error.h>
#include <tegrabl_module.h>
#include <tegrabl_debug.h>
#include <tegrabl_timer.h>
#include <tegrabl_clock.h>
#include <tegrabl_bpmp_fw_interface.h>
#include <tegrabl_clk_rst_soc.h>
#include <tegrabl_qspi.h>
#include <tegrabl_drf.h>
#include <tegrabl_clk_ufs.h>
#include <address_map_new.h>
#include <arpmc_impl.h>

#include <bpmp_abi.h>
#include <clk-t186.h>
#include <reset-t186.h>

#define UFSHC_AUX_UFSHC_DEV_CTRL_0        _MK_ADDR_CONST(0x2460014)
#define UFSHC_AUX_UFSHC_DEV_CTRL_0_UFSHC_DEV_CLK_EN_RANGE   (0) : (0)
#define UFSHC_AUX_UFSHC_DEV_CTRL_0_UFSHC_DEV_RESET_RANGE    (1) : (1)

#define BPMP_CLK_CMD(cmd, id) ((id) | ((cmd) << 24))
#define MODULE_NOT_SUPPORTED (TEGRA186_CLK_CLK_MAX)
#define MAX_PARENTS 16

#define HZ_1K       (1000)
#define KHZ_P4M     (400)
#define KHZ_13M     (13000)
#define KHZ_19P2M   (19200)
#define KHZ_12M     (12000)
#define KHZ_26M     (26000)
#define KHZ_16P8M   (16800)
#define KHZ_38P4M   (38400)
#define KHZ_48M     (48000)

#define RATE_XUSB_DEV_KHZ (102000)
#define RATE_XUSB_SS_KHZ  (120000)

#define NUM_USB_CLKS     15
#define NUM_USB_TRK_CLKS 3

#define NAME_INDEX          0

#define USB_RATE_INDEX      1
#define USB_PARENT_INDEX    2

#define NUM_UFS_CLKS        18
#define NUM_UFS_RSTS        8
#define UFS_RATE_INDEX      1
#define UFS_PARENT_INDEX    2

static uint32_t pllc4_muxed_rate;

static const uint32_t usb_clk_data[NUM_USB_CLKS][3] = {
	{TEGRA186_CLK_USB2_HSIC_TRK,        9600,   TEGRA186_CLK_OSC},
	{TEGRA186_CLK_USB2_TRK,             9600,   TEGRA186_CLK_OSC},
	{TEGRA186_CLK_HSIC_TRK,             9600,   TEGRA186_CLK_OSC},
	{TEGRA186_CLK_PEX_USB_PAD1_MGMT,    102000, TEGRA186_CLK_PLLP_OUT0},
	{TEGRA186_CLK_PEX_USB_PAD0_MGMT,    102000, TEGRA186_CLK_PLLP_OUT0},
	{TEGRA186_CLK_XUSB_FALCON,          204000, TEGRA186_CLK_PLLP_OUT0},
	{TEGRA186_CLK_XUSB_CORE_DEV,        102000, TEGRA186_CLK_PLLP_OUT0},
	{TEGRA186_CLK_PEX_SATA_USB_RX_BYP,  204000, TEGRA186_CLK_PLLP_OUT0},
	{TEGRA186_CLK_XUSB,                 102000, TEGRA186_CLK_PLLP_OUT0},
	{TEGRA186_CLK_XUSB_SS,              102000, TEGRA186_CLK_PLLP_OUT0},
	{TEGRA186_CLK_XUSB_HOST,            102000, TEGRA186_CLK_PLLP_OUT0},
	{TEGRA186_CLK_XUSB_CORE_SS,         120000, TEGRA186_CLK_PLL_U_480M},
	{TEGRA186_CLK_XUSB_FS,              48000,  TEGRA186_CLK_PLL_U_48M},
	{TEGRA186_CLK_XUSB_DEV,             48000,  TEGRA186_CLK_PLL_U_48M},
	{TEGRA186_CLK_UTMIP_PLL_PWRSEQ,     38400,  TEGRA186_CLK_PLLU}
};

static const uint32_t ufs_clk_data[NUM_UFS_CLKS][3] = {
	{TEGRA186_CLK_OSC,                  0,      TEGRA186_CLK_CLK_MAX},  /* 1 */
	{TEGRA186_CLK_CLK_M,                38400,  TEGRA186_CLK_CLK_MAX},
	{TEGRA186_CLK_PLLU,                 38400,  TEGRA186_CLK_OSC},
	{TEGRA186_CLK_UTMIP_PLL_PWRSEQ,     38400,  TEGRA186_CLK_PLLU},
	{TEGRA186_CLK_PLLREFE_REF,          60000,  TEGRA186_CLK_PLLU},     /* 5 */
	{TEGRA186_CLK_PLLREFE_IDDQ,         60000,  TEGRA186_CLK_PLLREFE_REF},
	{TEGRA186_CLK_PLLREFE_OUT1,         625000, TEGRA186_CLK_PLLREFE_IDDQ},
#if defined(CONFIG_ENABLE_UFS_HS_MODE)
	{TEGRA186_CLK_UFSHC,                204000,  TEGRA186_CLK_PLLP_OUT0},
#else
	{TEGRA186_CLK_UFSHC,                51000,  TEGRA186_CLK_PLLP_OUT0},
#endif
	{TEGRA186_CLK_UFSDEV_REF,           0,      TEGRA186_CLK_CLK_M},
	{TEGRA186_CLK_MPHY_CORE_PLL_FIXED,  0,      TEGRA186_CLK_CLK_MAX}, /* 10 */
	{TEGRA186_CLK_MPHY_TX_1MHZ_REF,     0,      TEGRA186_CLK_CLK_MAX},
	{TEGRA186_CLK_MPHY_IOBIST,          0,      TEGRA186_CLK_CLK_MAX},
	{TEGRA186_CLK_MPHY_L0_TX_LS_3XBIT,  0,      TEGRA186_CLK_CLK_MAX},
	{TEGRA186_CLK_MPHY_L0_TX_SYMB,      0,      TEGRA186_CLK_CLK_MAX},
	{TEGRA186_CLK_MPHY_L0_RX_LS_BIT,    0,      TEGRA186_CLK_CLK_MAX}, /* 15 */
	{TEGRA186_CLK_MPHY_L0_RX_SYMB,      0,      TEGRA186_CLK_CLK_MAX},
	{TEGRA186_CLK_MPHY_L0_RX_ANA,       0,      TEGRA186_CLK_CLK_MAX},
	{TEGRA186_CLK_MPHY_L1_RX_ANA,       0,      TEGRA186_CLK_CLK_MAX},
};

static const uint32_t ufs_rst_data[NUM_UFS_RSTS] = {
	TEGRA186_RESET_MPHY_CLK_CTL,    /* 1 */
	TEGRA186_RESET_MPHY_L1_RX,
	TEGRA186_RESET_MPHY_L1_TX,
	TEGRA186_RESET_MPHY_L0_RX,      /* 4 */
	TEGRA186_RESET_MPHY_L0_TX,
	TEGRA186_RESET_UFSHC,
	TEGRA186_RESET_UFSHC_AXI_M,
	TEGRA186_RESET_UFSHC_LP,        /* 8 */
};

static uint32_t tegrabl_pllid_to_bpmp_pllid[TEGRABL_CLK_PLL_ID_MAX] = {
		[TEGRABL_CLK_PLL_ID_PLLP] = TEGRA186_CLK_PLLP,
		[TEGRABL_CLK_PLL_ID_PLLC4] = TEGRA186_CLK_PLLC4_VCO,
		[TEGRABL_CLK_PLL_ID_PLLD] = TEGRA186_CLK_PLLD,
		[TEGRABL_CLK_PLL_ID_PLLD2] = TEGRA186_CLK_PLLD2,
		[TEGRABL_CLK_PLL_ID_PLLD3] = TEGRA186_CLK_PLLD3,
		[TEGRABL_CLK_PLL_ID_PLLDP] = TEGRA186_CLK_PLLDP,
		[TEGRABL_CLK_PLL_ID_PLLE] = TEGRA186_CLK_PLLE,
		[TEGRABL_CLK_PLL_ID_PLLM] = TEGRA186_CLK_CLK_MAX,
		[TEGRABL_CLK_PLL_ID_SATA_PLL] = TEGRA186_CLK_PLLP_OUT0,
		[TEGRABL_CLK_PLL_ID_UTMI_PLL] = TEGRA186_CLK_PLLU,
		[TEGRABL_CLK_PLL_ID_XUSB_PLL] = TEGRA186_CLK_PLLP_OUT0,
		[TEGRABL_CLK_PLL_ID_AON_PLL] = TEGRA186_CLK_PLLAON,
		[TEGRABL_CLK_PLL_ID_PLLDISPHUB] = TEGRA186_CLK_PLLDISPHUB,
		[TEGRABL_CLK_PLL_ID_PLL_NUM] = TEGRA186_CLK_CLK_MAX,
		[TEGRABL_CLK_PLL_ID_PLLMSB] = TEGRA186_CLK_CLK_MAX,
};


#define MOD_CLK                     0
#define MOD_RST                     1

#define UART_MAX_INSTANCES_A2G      7
#define SDMMC_MAX_INSTANCES_1TO4    4
#define I2C_MAX_INSTANCES_1TO14     14
#define SPI_MAX_INSTANCES_1TO4      4

static uint32_t uart_module_instances[UART_MAX_INSTANCES_A2G][2] = {
	{TEGRA186_CLK_UARTA, TEGRA186_RESET_UARTA},
	{TEGRA186_CLK_UARTB, TEGRA186_RESET_UARTB},
	{TEGRA186_CLK_UARTC, TEGRA186_RESET_UARTC},
	{TEGRA186_CLK_UARTD, TEGRA186_RESET_UARTD},
	{TEGRA186_CLK_UARTE, TEGRA186_RESET_UARTE},
	{TEGRA186_CLK_UARTF, TEGRA186_RESET_UARTF},
	{TEGRA186_CLK_UARTG, TEGRA186_RESET_UARTG}
};

static uint32_t sdmmc_module_instances[SDMMC_MAX_INSTANCES_1TO4][2] = {
	{TEGRA186_CLK_SDMMC1, TEGRA186_RESET_SDMMC1},
	{TEGRA186_CLK_SDMMC2, TEGRA186_RESET_SDMMC2},
	{TEGRA186_CLK_SDMMC3, TEGRA186_RESET_SDMMC3},
	{TEGRA186_CLK_SDMMC4, TEGRA186_RESET_SDMMC4}
};

static uint32_t i2c_module_instances[I2C_MAX_INSTANCES_1TO14][2] = {
	{TEGRA186_CLK_I2C1,    TEGRA186_RESET_I2C1},
	{TEGRA186_CLK_I2C2,    TEGRA186_RESET_I2C2},
	{TEGRA186_CLK_I2C3,    TEGRA186_RESET_I2C3},
	{TEGRA186_CLK_I2C4,    TEGRA186_RESET_I2C4},
	{TEGRA186_CLK_I2C5,    TEGRA186_RESET_I2C5},
	{TEGRA186_CLK_I2C6,    TEGRA186_RESET_I2C6},
	{TEGRA186_CLK_I2C7,    TEGRA186_RESET_I2C7},
	{TEGRA186_CLK_I2C8,    TEGRA186_RESET_I2C8},
	{TEGRA186_CLK_I2C9,    TEGRA186_RESET_I2C9},
	{TEGRA186_CLK_I2C10,   TEGRA186_RESET_I2C10},
	{MODULE_NOT_SUPPORTED, MODULE_NOT_SUPPORTED},
	{TEGRA186_CLK_I2C12,   TEGRA186_RESET_I2C12},
	{TEGRA186_CLK_I2C13,   TEGRA186_RESET_I2C13},
	{TEGRA186_CLK_I2C14,   TEGRA186_RESET_I2C14}
};

static enum {
	TEGRABL_XUSB, /*0*/
	TEGRABL_XUSB_DEV,
	TEGRABL_XUSB_HOST,/*2*/
	TEGRABL_XUSB_SS,
	TEGRABL_XUSB_PADCTL, /*4*/
	XUSB_MAX_INSTANCES
} internal_xusb_index_map;

static uint32_t xusb_module_instances[XUSB_MAX_INSTANCES][2] = {
	{TEGRA186_CLK_XUSB,      MODULE_NOT_SUPPORTED},
	{TEGRA186_CLK_XUSB_DEV,  TEGRA186_RESET_XUSB_DEV},
	{TEGRA186_CLK_XUSB_HOST, TEGRA186_RESET_XUSB_HOST},
	{TEGRA186_CLK_XUSB_SS,   TEGRA186_RESET_XUSB_SS},
	{MODULE_NOT_SUPPORTED,   TEGRA186_RESET_XUSB_PADCTL}
};

static enum {
	TEGRABL_RST_NVDISPLAY0_HEAD0,
	TEGRABL_RST_NVDISPLAY0_HEAD1,
	TEGRABL_RST_NVDISPLAY0_HEAD2,
	TEGRABL_RST_NVDISPLAY0_WGRP0,
	TEGRABL_RST_NVDISPLAY0_WGRP1,
	TEGRABL_RST_NVDISPLAY0_WGRP2,
	TEGRABL_RST_NVDISPLAY0_WGRP3,
	TEGRABL_RST_NVDISPLAY0_WGRP4,
	TEGRABL_RST_NVDISPLAY0_WGRP5,
	TEGRABL_RST_NVDISPLAY0_MISC,
	TEGRABL_NVDISP_DSI,
	TEGRABL_NVDISP_DSIB,
	TEGRABL_NVDISP_DSIC,
	TEGRABL_NVDISP_DSID,
	TEGRABL_NVDISP_SOR0,
	TEGRABL_NVDISP_SOR1,
	TEGRABL_NVDISP_P0,
	TEGRABL_NVDISP_P1,
	TEGRABL_NVDISP_P2,
	TEGRABL_NVDISP_HOST1X,
	TEGRABL_NVDISP_HUB,
	TEGRABL_NVDISP_DSC,
	TEGRABL_NVDISP_DISP,
	TEGRABL_NVDISP_SOR0_PAD_CLKOUT,
	TEGRABL_NVDISP_SOR1_PAD_CLKOUT,
	TEGRABL_NVDISP_SOR_SAFE,
	TEGRABL_NVDISP_SOR0_OUT,
	TEGRABL_NVDISP_SOR1_OUT,
	TEGRABL_NVDISP_DPAUX,
	TEGRABL_NVDISP_DPAUX1,
	NVDISP_MAX_INSTANCES
} index_nvdisp_map;

static uint32_t nvdisp_module_instance[NVDISP_MAX_INSTANCES][2] = {
	{MODULE_NOT_SUPPORTED,         TEGRA186_RESET_NVDISPLAY0_HEAD0},
	{MODULE_NOT_SUPPORTED,         TEGRA186_RESET_NVDISPLAY0_HEAD1},
	{MODULE_NOT_SUPPORTED,         TEGRA186_RESET_NVDISPLAY0_HEAD2},
	{MODULE_NOT_SUPPORTED,         TEGRA186_RESET_NVDISPLAY0_WGRP0},
	{MODULE_NOT_SUPPORTED,         TEGRA186_RESET_NVDISPLAY0_WGRP1},
	{MODULE_NOT_SUPPORTED,         TEGRA186_RESET_NVDISPLAY0_WGRP2},
	{MODULE_NOT_SUPPORTED,         TEGRA186_RESET_NVDISPLAY0_WGRP3},
	{MODULE_NOT_SUPPORTED,         TEGRA186_RESET_NVDISPLAY0_WGRP4},
	{MODULE_NOT_SUPPORTED,         TEGRA186_RESET_NVDISPLAY0_WGRP5},
	{MODULE_NOT_SUPPORTED,         TEGRA186_RESET_NVDISPLAY0_MISC},
	{TEGRA186_CLK_DSI,             TEGRA186_RESET_DSI},
	{TEGRA186_CLK_DSIB,            TEGRA186_RESET_DSIB},
	{TEGRA186_CLK_DSIC,            TEGRA186_RESET_DSIC},
	{TEGRA186_CLK_DSID,            TEGRA186_RESET_DSID},
	{TEGRA186_CLK_SOR0,            TEGRA186_RESET_SOR0},
	{TEGRA186_CLK_SOR1,            TEGRA186_RESET_SOR1},
	{TEGRA186_CLK_NVDISPLAY_P0,    MODULE_NOT_SUPPORTED},
	{TEGRA186_CLK_NVDISPLAY_P1,    MODULE_NOT_SUPPORTED},
	{TEGRA186_CLK_NVDISPLAY_P2,    MODULE_NOT_SUPPORTED},
	{TEGRA186_CLK_HOST1X,          TEGRA186_RESET_HOST1X},
	{TEGRA186_CLK_NVDISPLAYHUB,    MODULE_NOT_SUPPORTED},
	{TEGRA186_CLK_NVDISPLAY_DSC,   MODULE_NOT_SUPPORTED},
	{TEGRA186_CLK_NVDISPLAY_DISP,  MODULE_NOT_SUPPORTED},
	{TEGRA186_CLK_SOR0_PAD_CLKOUT, MODULE_NOT_SUPPORTED},
	{TEGRA186_CLK_SOR1_PAD_CLKOUT, MODULE_NOT_SUPPORTED},
	{TEGRA186_CLK_SOR_SAFE,        MODULE_NOT_SUPPORTED},
	{TEGRA186_CLK_SOR0_OUT,        MODULE_NOT_SUPPORTED},
	{TEGRA186_CLK_SOR1_OUT,        MODULE_NOT_SUPPORTED},
	{TEGRA186_CLK_DPAUX,           TEGRA186_RESET_DPAUX},
	{TEGRA186_CLK_DPAUX1,          TEGRA186_RESET_DPAUX1},
	};

static uint32_t spi_module_instances[SPI_MAX_INSTANCES_1TO4][2] = {
	{TEGRA186_CLK_SPI1, TEGRA186_RESET_SPI1},
	{TEGRA186_CLK_SPI2, TEGRA186_RESET_SPI2},
	{TEGRA186_CLK_SPI3, TEGRA186_RESET_SPI3},
	{TEGRA186_CLK_SPI4, TEGRA186_RESET_SPI4},
};

static int32_t tegrabl_module_to_bpmp_id(
				tegrabl_module_t module_num,
				uint8_t instance,
				bool clk_or_rst)
{
	/* TODO - Complete below mapping */
	switch (module_num) {
	case (TEGRABL_MODULE_UART):
	{
		if (instance < UART_MAX_INSTANCES_A2G)
			return uart_module_instances[instance][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_SDMMC):
	{
		if (instance < SDMMC_MAX_INSTANCES_1TO4)
			return sdmmc_module_instances[instance][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_GPCDMA):
	{
		if (clk_or_rst == MOD_RST)
			return TEGRA186_RESET_GPCDMA;
		else if (clk_or_rst == MOD_CLK)
			return TEGRA186_CLK_GPCCLK;
		else
			return MODULE_NOT_SUPPORTED;
		break;
	}
	case (TEGRABL_MODULE_QSPI):
	{
		if (clk_or_rst == MOD_RST)
			return TEGRA186_RESET_QSPI;
		else if (clk_or_rst == MOD_CLK)
			return TEGRA186_CLK_QSPI;
		else
			return MODULE_NOT_SUPPORTED;
		break;
	}
	case (TEGRABL_MODULE_I2C):
	{
		if (instance < I2C_MAX_INSTANCES_1TO14)
			return i2c_module_instances[instance][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_XUSBF):
	{
		internal_xusb_index_map = TEGRABL_XUSB;
		return xusb_module_instances[internal_xusb_index_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_XUSB_DEV):
	{
		internal_xusb_index_map = TEGRABL_XUSB_DEV;
		return xusb_module_instances[internal_xusb_index_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_XUSB_HOST):
	{
		internal_xusb_index_map = TEGRABL_XUSB_HOST;
		return xusb_module_instances[internal_xusb_index_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_XUSB_SS):
	{
		internal_xusb_index_map = TEGRABL_XUSB_SS;
		return xusb_module_instances[internal_xusb_index_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_XUSB_PADCTL):
	{
		internal_xusb_index_map = TEGRABL_XUSB_PADCTL;
		return xusb_module_instances[internal_xusb_index_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_DSI):
	{
		if (instance > 3) /* DSI, DSIB, DSIC, DSID */
			return MODULE_NOT_SUPPORTED;
		/* index w.r.t DSI */
		index_nvdisp_map = TEGRABL_NVDISP_DSI + instance;
		return nvdisp_module_instance[index_nvdisp_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_SOR):
	{
		if (instance > 1) /* SOR0, SOR1 */
			return MODULE_NOT_SUPPORTED;
		/* index w.r.t SOR0 */
		index_nvdisp_map = TEGRABL_NVDISP_SOR0 + instance;
		return nvdisp_module_instance[index_nvdisp_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_SOR_OUT):
	{
		if (instance > 1) /* SOR0_OUT, SOR1_OUT */
			return MODULE_NOT_SUPPORTED;
		/* index w.r.t SOR0_OUT */
		index_nvdisp_map = TEGRABL_NVDISP_SOR0_OUT + instance;
		return nvdisp_module_instance[index_nvdisp_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_SOR_PAD_CLKOUT):
	{
		if (instance > 1) /* SOR0_PAD_CLKOUT, SOR1_PAD_CLKOUT */
			return MODULE_NOT_SUPPORTED;
		/* index w.r.t SOR0_PAD_CLKOUT */
		index_nvdisp_map = TEGRABL_NVDISP_SOR0_PAD_CLKOUT + instance;
		return nvdisp_module_instance[index_nvdisp_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_SOR_SAFE):
	{
		index_nvdisp_map = TEGRABL_NVDISP_SOR_SAFE;
		return nvdisp_module_instance[index_nvdisp_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_DPAUX):
	{
		index_nvdisp_map = TEGRABL_NVDISP_DPAUX;
		return nvdisp_module_instance[index_nvdisp_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_DPAUX1):
	{
		index_nvdisp_map = TEGRABL_NVDISP_DPAUX1;
		return nvdisp_module_instance[index_nvdisp_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_NVDISPLAYHUB):
	{
		index_nvdisp_map = TEGRABL_NVDISP_HUB;
		return nvdisp_module_instance[index_nvdisp_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_NVDISPLAY_DSC):
	{
		index_nvdisp_map = TEGRABL_NVDISP_DSC;
		return nvdisp_module_instance[index_nvdisp_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_NVDISPLAY_DISP):
	{
		index_nvdisp_map = TEGRABL_NVDISP_DISP;
		return nvdisp_module_instance[index_nvdisp_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_NVDISPLAY_P):
	{
		if (instance > 2) /* P0 to P2 */
			return MODULE_NOT_SUPPORTED;
		/* index w.r.t P0 */
		index_nvdisp_map = TEGRABL_NVDISP_P0 + instance;
		return nvdisp_module_instance[index_nvdisp_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_HOST1X):
	{
		index_nvdisp_map = TEGRABL_NVDISP_HOST1X;
		return nvdisp_module_instance[index_nvdisp_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_NVDISPLAY0_HEAD):
	{
		if (instance > 2) /* HEAD0 to HEAD2 */
			return MODULE_NOT_SUPPORTED;
		/* index w.r.t HEAD0 */
		index_nvdisp_map =
					(TEGRABL_RST_NVDISPLAY0_HEAD0 + instance);
		return nvdisp_module_instance[index_nvdisp_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_NVDISPLAY0_WGRP):
	{
		if (instance > 5) /* WGRP0 to WGRP5 */
			return MODULE_NOT_SUPPORTED;
		/* index w.r.t WGRP0 */
		index_nvdisp_map =
					(TEGRABL_RST_NVDISPLAY0_WGRP0 + instance);
		return nvdisp_module_instance[index_nvdisp_map][clk_or_rst];
		break;
	}
	case (TEGRABL_MODULE_NVDISPLAY0_MISC):
	{
		index_nvdisp_map = TEGRABL_RST_NVDISPLAY0_MISC;
		return nvdisp_module_instance[index_nvdisp_map][clk_or_rst];
		break;
	}
	case TEGRABL_MODULE_SE:
	{
		switch (clk_or_rst) {
		case MOD_RST:
			return TEGRA186_RESET_SE;
			break;
		case MOD_CLK:
			return TEGRA186_CLK_SE;
			break;
		}
		break;
	}
	case TEGRABL_MODULE_SPI:
	{
		if (instance < SPI_MAX_INSTANCES_1TO4)
			return spi_module_instances[instance][clk_or_rst];
		break;
	}
	case TEGRABL_MODULE_AUD_MCLK:
	{
		switch (clk_or_rst) {
		case MOD_RST:
			return TEGRA186_RESET_AUD_MCLK;
			break;
		case MOD_CLK:
			return TEGRA186_CLK_AUD_MCLK;
			break;
		default:
			break;
		}
	}
	case TEGRABL_MODULE_SATA:
	{
		switch (clk_or_rst) {
		case MOD_RST:
			return TEGRA186_RESET_SATA;
			break;
		case MOD_CLK:
			return TEGRA186_CLK_SATA;
			break;
		default:
			break;
		}
	}
	case TEGRABL_MODULE_SATACOLD:
	{
		switch (clk_or_rst) {
		case MOD_RST:
			return TEGRA186_RESET_SATACOLD;
			break;
		default:
			break;
		}
	}
	case TEGRABL_MODULE_SATA_OOB:
	{
		switch (clk_or_rst) {
		case MOD_CLK:
			return TEGRA186_CLK_SATA_OOB;
			break;
		default:
			break;
		}
	}
	case TEGRABL_MODULE_PCIE:
	{
		switch (clk_or_rst) {
		case MOD_RST:
			return TEGRA186_RESET_PCIE;
			break;
		case MOD_CLK:
			return TEGRA186_CLK_PCIE;
			break;
		default:
			break;
		}
	}
	case TEGRABL_MODULE_PCIEXCLK:
	{
		switch (clk_or_rst) {
		case MOD_RST:
			return TEGRA186_RESET_PCIEXCLK;
			break;
		default:
			break;
		}
	}
	case TEGRABL_MODULE_AFI:
	{
		switch (clk_or_rst) {
		case MOD_RST:
			return TEGRA186_RESET_AFI;
			break;
		case MOD_CLK:
			return TEGRA186_CLK_AFI;
			break;
		default:
			break;
		}
	}
	default:
		break;
	}
	return MODULE_NOT_SUPPORTED;
}

static enum tegrabl_clk_src_id_t src_clk_bpmp_to_tegrabl(uint32_t src)
{
	/* TODO - Complete below mapping */
	switch (src) {
	case TEGRA186_CLK_PLLP_OUT0:
		return TEGRABL_CLK_SRC_PLLP_OUT0;
		break;
	case TEGRA186_CLK_CLK_M:
		return TEGRABL_CLK_SRC_CLK_M;
		break;
	case TEGRA186_CLK_PLLC4_OUT1:
	case TEGRA186_CLK_PLLC4_OUT2:
	case TEGRA186_CLK_PLLC4_VCO_DIV2:
		return TEGRABL_CLK_SRC_PLLC4_MUXED;
		break;
	case TEGRA186_CLK_CLK_32K:
		return TEGRABL_CLK_SRC_CLK_S;
		break;
	case TEGRA186_CLK_PLLD_OUT1:
		return TEGRABL_CLK_SRC_PLLD_OUT1;
		break;
	case TEGRA186_CLK_PLLD2:
		return TEGRABL_CLK_SRC_PLLD2_OUT0;
		break;
	case TEGRA186_CLK_PLLD3:
		return TEGRABL_CLK_SRC_PLLD3_OUT0;
		break;
	case TEGRA186_CLK_PLLDP:
		return TEGRABL_CLK_SRC_PLLDP;
		break;
	case TEGRA186_CLK_NVDISPLAY_P0:
		return TEGRABL_CLK_SRC_NVDISPLAY_P0_CLK;
		break;
	case TEGRA186_CLK_NVDISPLAY_P1:
		return TEGRABL_CLK_SRC_NVDISPLAY_P1_CLK;
		break;
	case TEGRA186_CLK_NVDISPLAY_P2:
		return TEGRABL_CLK_SRC_NVDISPLAY_P2_CLK;
		break;
	case TEGRA186_CLK_SOR0:
		return TEGRABL_CLK_SRC_SOR0;
		break;
	case TEGRA186_CLK_SOR1:
		return TEGRABL_CLK_SRC_SOR1;
		break;
	case TEGRA186_CLK_SOR_SAFE:
		return TEGRABL_CLK_SRC_SOR_SAFE_CLK;
		break;
	case TEGRA186_CLK_SOR0_PAD_CLKOUT:
		return TEGRABL_CLK_SRC_SOR0_PAD_CLKOUT;
		break;
	case TEGRA186_CLK_SOR1_PAD_CLKOUT:
		return TEGRABL_CLK_SRC_SOR1_PAD_CLKOUT;
		break;
	case TEGRA186_CLK_DFLLDISP_DIV:
		return TEGRABL_CLK_SRC_DFLLDISP_DIV;
		break;
	case TEGRA186_CLK_PLLDISPHUB_DIV:
		return TEGRABL_CLK_SRC_PLLDISPHUB_DIV;
		break;
	case TEGRA186_CLK_PLLDISPHUB:
		return TEGRABL_CLK_SRC_PLLDISPHUB;
		break;
	default:
		return TEGRABL_CLK_SRC_INVALID;
	}
}

static uint32_t src_clk_tegrabl_to_bpmp(enum tegrabl_clk_src_id_t src)
{
	/* TODO - Complete below mapping */
	switch (src) {
	case TEGRABL_CLK_SRC_PLLP_OUT0:
		return TEGRA186_CLK_PLLP_OUT0;
		break;
	case TEGRABL_CLK_SRC_CLK_M:
		return TEGRA186_CLK_CLK_M;
		break;
	case TEGRABL_CLK_SRC_CLK_S:
		return TEGRA186_CLK_CLK_32K;
		break;
	case TEGRABL_CLK_SRC_PLLD_OUT1:
		return TEGRA186_CLK_PLLD_OUT1;
		break;
	case TEGRABL_CLK_SRC_PLLD2_OUT0:
		return TEGRA186_CLK_PLLD2;
		break;
	case TEGRABL_CLK_SRC_PLLD3_OUT0:
		return TEGRA186_CLK_PLLD3;
		break;
	case TEGRABL_CLK_SRC_PLLDP:
		return TEGRA186_CLK_PLLDP;
		break;
	case TEGRABL_CLK_SRC_NVDISPLAY_P0_CLK:
		return TEGRA186_CLK_NVDISPLAY_P0;
		break;
	case TEGRABL_CLK_SRC_NVDISPLAY_P1_CLK:
		return TEGRA186_CLK_NVDISPLAY_P1;
		break;
	case TEGRABL_CLK_SRC_NVDISPLAY_P2_CLK:
		return TEGRA186_CLK_NVDISPLAY_P2;
		break;
	case TEGRABL_CLK_SRC_SOR0:
		return TEGRA186_CLK_SOR0;
		break;
	case TEGRABL_CLK_SRC_SOR1:
		return TEGRA186_CLK_SOR1;
		break;
	case TEGRABL_CLK_SRC_SOR_SAFE_CLK:
		return TEGRA186_CLK_SOR_SAFE;
		break;
	case TEGRABL_CLK_SRC_SOR0_PAD_CLKOUT:
		return TEGRA186_CLK_SOR0_PAD_CLKOUT;
		break;
	case TEGRABL_CLK_SRC_SOR1_PAD_CLKOUT:
		return TEGRA186_CLK_SOR1_PAD_CLKOUT;
		break;
	case TEGRABL_CLK_SRC_DFLLDISP_DIV:
		return TEGRA186_CLK_DFLLDISP_DIV;
		break;
	case TEGRABL_CLK_SRC_PLLDISPHUB_DIV:
		return TEGRA186_CLK_PLLDISPHUB_DIV;
		break;
	case TEGRABL_CLK_SRC_PLLDISPHUB:
		return TEGRA186_CLK_PLLDISPHUB;
		break;
	default:
		return TEGRA186_CLK_CLK_MAX;
	}
}

static tegrabl_error_t internal_tegrabl_car_set_clk_src(
		uint32_t clk_id,
		uint32_t clk_src)
{
	struct mrq_clk_request req_clk_set_src;
	struct mrq_clk_response resp_clk_set_src;

	if ((clk_id == MODULE_NOT_SUPPORTED) ||
		(clk_src == TEGRA186_CLK_CLK_MAX)) {
			pr_error("%s coudn't set %d (bpmpid) as parent, returning\n",
					 __func__, clk_src);
		return TEGRABL_ERR_NOT_SUPPORTED;
	}

	req_clk_set_src.clk_set_parent.parent_id = clk_src;
	req_clk_set_src.cmd_and_id = BPMP_CLK_CMD(CMD_CLK_SET_PARENT, clk_id);

	pr_debug("(%s,%d) bpmp_src: %d\n", __func__, __LINE__, clk_src);

	/* TX */
	if (TEGRABL_NO_ERROR != tegrabl_ccplex_bpmp_xfer(
					&req_clk_set_src, &resp_clk_set_src,
					sizeof(struct mrq_clk_request),
					sizeof(struct mrq_clk_response),
					MRQ_CLK)) {
		pr_error("Error in tx-rx: %s,%d\n", __func__, __LINE__);
		return TEGRABL_ERR_INVALID;
	}

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t internal_tegrabl_car_get_clk_rate(
		uint32_t clk_id,
		uint32_t *rate_khz)
{
	struct mrq_clk_request req_clk_get_rate;
	struct mrq_clk_response resp_clk_get_rate;

	if (clk_id == TEGRA186_CLK_CLK_MAX)
		return TEGRABL_ERR_NOT_SUPPORTED;

	req_clk_get_rate.cmd_and_id = BPMP_CLK_CMD(CMD_CLK_GET_RATE, clk_id);

	/* TX */
	if (TEGRABL_NO_ERROR != tegrabl_ccplex_bpmp_xfer(
					&req_clk_get_rate, &resp_clk_get_rate,
					sizeof(struct mrq_clk_request),
					sizeof(struct mrq_clk_response),
					MRQ_CLK)) {
		pr_error("Error in tx-rx: %s,%d\n", __func__, __LINE__);
		return TEGRABL_ERR_INVALID;
	}

	/* RX */
	*rate_khz = (resp_clk_get_rate.clk_get_rate.rate)/HZ_1K;
	pr_debug("Received data (from BPMP) %d\n", *rate_khz);

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t internal_tegrabl_car_set_clk_rate(
		uint32_t clk_id,
		uint32_t rate_khz,
		uint32_t *rate_set_khz)
{
	struct mrq_clk_request req_clk_set_rate;
	struct mrq_clk_response resp_clk_set_rate;

	if (clk_id == MODULE_NOT_SUPPORTED)
		return TEGRABL_ERR_NOT_SUPPORTED;

	req_clk_set_rate.cmd_and_id = BPMP_CLK_CMD(CMD_CLK_SET_RATE, clk_id);
	req_clk_set_rate.clk_set_rate.rate = rate_khz*HZ_1K;

	/* TX */
	if (TEGRABL_NO_ERROR != tegrabl_ccplex_bpmp_xfer(
					&req_clk_set_rate, &resp_clk_set_rate,
					sizeof(struct mrq_clk_request),
					sizeof(struct mrq_clk_response),
					MRQ_CLK)) {
		pr_error("Error in tx-rx: %s,%d\n", __func__, __LINE__);
		return TEGRABL_ERR_INVALID;
	}

	/* RX */
	*rate_set_khz = (resp_clk_set_rate.clk_set_rate.rate)/HZ_1K;

	pr_debug("(%s,%d) Enabled rate %d for %d\n", __func__, __LINE__,
			 *rate_set_khz, clk_id);

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t internal_tegrabl_car_clk_enable(uint32_t clk_id)
{
	struct mrq_clk_request req_clk_enable;
	struct mrq_clk_response resp_clk_enable;

	if (clk_id == MODULE_NOT_SUPPORTED)
		return TEGRABL_ERR_NOT_SUPPORTED;

	req_clk_enable.cmd_and_id = BPMP_CLK_CMD(CMD_CLK_ENABLE, clk_id);

	/* TX */
	if (TEGRABL_NO_ERROR != tegrabl_ccplex_bpmp_xfer(
					&req_clk_enable, &resp_clk_enable,
					sizeof(struct mrq_clk_request),
					sizeof(struct mrq_clk_response),
					MRQ_CLK)) {
		pr_error("Error in tx-rx: %s,%d\n", __func__, __LINE__);
		return TEGRABL_ERR_INVALID;
	}

	pr_debug("(%s,%d) Enabled - %d\n", __func__, __LINE__, clk_id);

		return TEGRABL_NO_ERROR;
}

static bool internal_tegrabl_car_clk_is_enabled(uint32_t clk_id)
{
	struct mrq_clk_request req_clk_is_enabled;
	struct mrq_clk_response resp_clk_is_enabled;

	if (clk_id == MODULE_NOT_SUPPORTED)
		return false;

	req_clk_is_enabled.cmd_and_id = BPMP_CLK_CMD(CMD_CLK_IS_ENABLED, clk_id);

	/* TX */
	if (TEGRABL_NO_ERROR != tegrabl_ccplex_bpmp_xfer(
					&req_clk_is_enabled, &resp_clk_is_enabled,
					sizeof(struct mrq_clk_request),
					sizeof(struct mrq_clk_response),
					MRQ_CLK)) {
		pr_error("Error in tx-rx: %s,%d\n", __func__, __LINE__);
		return false;
	}

	pr_debug("(%s,%d) clk(%d) state = %d\n", __func__, __LINE__, clk_id,
			 resp_clk_is_enabled.clk_is_enabled.state);

	return (bool)resp_clk_is_enabled.clk_is_enabled.state;
}

bool tegrabl_car_clk_is_enabled(tegrabl_module_t module, uint8_t instance)
{
	uint32_t bpmp_id;

	bpmp_id = tegrabl_module_to_bpmp_id(module, instance, MOD_CLK);
	if (bpmp_id == MODULE_NOT_SUPPORTED) {
		return false;
	}

	return internal_tegrabl_car_clk_is_enabled(bpmp_id);
}


static tegrabl_error_t internal_tegrabl_car_clk_disable(uint32_t clk_id)
{
	struct mrq_clk_request req_clk_disable;
	struct mrq_clk_response resp_clk_disable;

	if (!internal_tegrabl_car_clk_is_enabled(clk_id)) {
		pr_debug("clock (id - %d) not enabled. skipping disable request\n",
				 clk_id);
		return TEGRABL_NO_ERROR;
	}

	if (clk_id == MODULE_NOT_SUPPORTED)
		return TEGRABL_ERR_NOT_SUPPORTED;

	req_clk_disable.cmd_and_id = BPMP_CLK_CMD(CMD_CLK_DISABLE, clk_id);

	/* TX */
	if (TEGRABL_NO_ERROR != tegrabl_ccplex_bpmp_xfer(
					&req_clk_disable, &resp_clk_disable,
					sizeof(struct mrq_clk_request),
					sizeof(struct mrq_clk_response),
					MRQ_CLK)) {
		pr_error("Error in tx-rx: %s,%d\n", __func__, __LINE__);
		return TEGRABL_ERR_INVALID;
	}

	pr_debug("(%s,%d) Disabled - %d\n", __func__, __LINE__, clk_id);

		return TEGRABL_NO_ERROR;
}

static tegrabl_error_t internal_tegrabl_car_rst(uint32_t rst_id, uint32_t flag)
{
	struct mrq_reset_request req_rst;
	uint32_t resp_rst;

	if (rst_id == MODULE_NOT_SUPPORTED)
		return TEGRABL_ERR_NOT_SUPPORTED;

	pr_debug("(%s,%d) reset operation on %d\n", __func__, __LINE__, rst_id);
	req_rst.cmd = flag;
	req_rst.reset_id = rst_id;

	/* TX */
	if (TEGRABL_NO_ERROR != tegrabl_ccplex_bpmp_xfer(
					&req_rst, &resp_rst,
					sizeof(req_rst),
					sizeof(resp_rst),
					MRQ_RESET)) {
		pr_error("Error in tx-rx: %s,%d\n", __func__, __LINE__);
	}

	return TEGRABL_NO_ERROR;
}

/**
 * ------------------------NOTES------------------------
 * Please read below before using these APIs.
 * For using APIs that query clock state, namely get_clk_rate()
 * and get_clk_src(), it is necessary that clk_enable has been
 * called for the module before regardless of whether the clock is
 * enabled by default on POR. This is how the driver keeps initializes
 * the module clock states.
 */

/**
 * @brief - Gets the current clock source of the module
 *
 * @module - Module ID of the module
 * @instance - Instance of the module
 * @return - Enum of clock source if module is found and has a valid clk source
 * configured. TEGRABL_CLK_SRC_INVAID otherwise.
 */
enum tegrabl_clk_src_id_t tegrabl_car_get_clk_src(
		tegrabl_module_t module,
		uint8_t instance)
{
	struct mrq_clk_request req_clk_get_src;
	struct mrq_clk_response resp_clk_get_src;
	int32_t clk_id;

	pr_debug("(%s,%d) %d, %d\n", __func__, __LINE__,
			 module, instance);

	clk_id =  tegrabl_module_to_bpmp_id(module, instance, MOD_CLK);
	if (clk_id == MODULE_NOT_SUPPORTED)
		return TEGRABL_ERR_NOT_SUPPORTED;

	req_clk_get_src.cmd_and_id = BPMP_CLK_CMD(CMD_CLK_GET_PARENT, clk_id);

	/* TX */
	if (TEGRABL_NO_ERROR != tegrabl_ccplex_bpmp_xfer(
					&req_clk_get_src, &resp_clk_get_src,
					sizeof(struct mrq_clk_request),
					sizeof(struct mrq_clk_response),
					MRQ_CLK)) {
		pr_error("Error in tx-rx: %s,%d\n", __func__, __LINE__);
		return TEGRABL_CLK_SRC_INVALID;
	}

	/* RX */
	pr_debug("Received parent_id (from BPMP): %d\n",
			 resp_clk_get_src.clk_get_parent.parent_id);

	return src_clk_bpmp_to_tegrabl(resp_clk_get_src.clk_get_parent.parent_id);
}

/**
 * @brief - Sets the clock source of the module to
 * the source specified.
 * NOTE: If the module clock is disabled when this function is called,
 * the new settings will take effect only after enabling the clock.
 *
 * @module - Module ID of the module
 * @instance - Instance of the module
 * @clk_src - Specified source
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_set_clk_src(
		tegrabl_module_t module,
		uint8_t instance,
		enum tegrabl_clk_src_id_t clk_src)
{
	pr_debug("(%s,%d) %d, %d\n", __func__, __LINE__,
			 module, instance);

	return internal_tegrabl_car_set_clk_src(
					tegrabl_module_to_bpmp_id(module, instance, MOD_CLK),
					src_clk_tegrabl_to_bpmp(clk_src));
}

/**
 * @brief - Gets the current clock rate of the module
 *
 * @module - Module ID of the module
 * @instance - Instance of the module
 * @rate_khz - Address to store the current clock rate
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_get_clk_rate(
		tegrabl_module_t module,
		uint8_t instance,
		uint32_t *rate_khz)
{
	pr_debug("(%s,%d) %d, %d\n", __func__, __LINE__,
			 module, instance);

	return internal_tegrabl_car_get_clk_rate(
			tegrabl_module_to_bpmp_id(module, instance, MOD_CLK),
			rate_khz);
}

/**
 * @brief - Get current frequency of the specified
 * clock source.
 *
 * @src_id - enum of the clock source
 * @rate_khz - Address to store the frequency of the clock source
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_get_clk_src_rate(
		enum tegrabl_clk_src_id_t src_id,
		uint32_t *rate_khz)
{
	pr_debug("(%s,%d) %d\n", __func__, __LINE__, src_id);

	if ((src_id == TEGRABL_CLK_SRC_PLLC4_MUXED) && (pllc4_muxed_rate != 0)) {
		*rate_khz = pllc4_muxed_rate;
		return TEGRABL_NO_ERROR;
	}

	return internal_tegrabl_car_get_clk_rate(
			src_clk_tegrabl_to_bpmp(src_id),
			rate_khz);
}

/**
 * @brief - Set frequency for the specified
 * clock source.
 *
 * @src_id - enum of the clock source
 * @rate_khz - the frequency of the clock source
 * @rate_set_khz - Rate set
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_set_clk_src_rate(
		enum tegrabl_clk_src_id_t src_id,
		uint32_t rate_khz,
		uint32_t *rate_set_khz)
{
	pr_debug("(%s,%d) %d\n", __func__, __LINE__, src_id);

	if (src_id == TEGRABL_CLK_SRC_PLLC4_MUXED) {
		/* MUX SRC rate cannot be edited directly - Exception
		 *  Save the requested rate for future use */
		pllc4_muxed_rate = rate_khz;
		return TEGRABL_NO_ERROR;
	}

	return internal_tegrabl_car_set_clk_rate(
			src_clk_tegrabl_to_bpmp(src_id),
			rate_khz,
			rate_set_khz);
}
/**
 * @brief - Attempts to set the current clock rate of
 * the module to the value specified and returns the actual rate set.
 * NOTE: If the module clock is disabled when this function is called,
 * it will also enable the clock.
 *
 * @module - Module ID of the module
 * @instance - Instance of the module
 * @rate_khz - Rate requested
 * @rate_set_khz - Rate set
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_set_clk_rate(
		tegrabl_module_t module,
		uint8_t instance,
		uint32_t rate_khz,
		uint32_t *rate_set_khz)
{
	pr_debug("(%s,%d) %d, %d, %d\n", __func__, __LINE__,
			 module, instance, rate_khz);

	/* TODO - Add a condition to check if already enabled */
	tegrabl_car_clk_enable(module, instance, NULL);

	return internal_tegrabl_car_set_clk_rate(
			tegrabl_module_to_bpmp_id(module, instance, MOD_CLK),
			rate_khz,
			rate_set_khz);
}

/**
 * @brief - Configures the essential PLLs, Oscillator,
 * and other essential clocks.
 */
void tegrabl_car_clock_init(void)
{
	/* Since BPMP takes care of initializing clk, hence this init is empty */
	return;
}

/**
 * @brief - Returns the oscillator frequency in KHz
 *
 * @freq_khz - Pointer to store the freq in kHz
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_get_osc_freq_khz(uint32_t *freq_khz)
{
	pr_debug("(%s,%d)\n", __func__, __LINE__);

	return internal_tegrabl_car_get_clk_rate(
			TEGRA186_CLK_OSC,
			freq_khz);
}

/**
 * @brief - Initializes the pll specified by pll_id.
 * Does nothing if pll already initialized
 *
 * @pll_id - ID of the pll to be initialized
 * @rate_khz - Rate to which the PLL is to be initialized
 * @priv_data - Any PLL specific initialization data to send
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_init_pll_with_rate(
		enum tegrabl_clk_pll_id pll_id, uint32_t rate_khz,
		void *priv_data)
{
	uint32_t rate_set_khz; /* filler - to avoid NULL exception */
	uint32_t clk_id = tegrabl_pllid_to_bpmp_pllid[pll_id];
	bool state;

	TEGRABL_UNUSED(priv_data);

	pr_debug("(%s,%d) %d, %d\n", __func__, __LINE__, pll_id, rate_khz);
	/* Check if requested PLL is supported (at present) */
	if (clk_id == TEGRA186_CLK_CLK_MAX)
		return TEGRABL_ERR_NOT_SUPPORTED;

	state = internal_tegrabl_car_clk_is_enabled(clk_id);
	/* Check if already initialized by BR/BL. If so, do nothing */
	if (state) {
		pr_info("(%s) Requested PLL(%d) already enabled. skipping init\n",
				__func__, pll_id);
		return TEGRABL_NO_ERROR;
	}

	/* Set rate */
	if (TEGRABL_NO_ERROR != internal_tegrabl_car_set_clk_rate(
			clk_id,
			rate_khz,
			&rate_set_khz))
		return TEGRABL_ERR_INVALID;

	/* Enable PLL */
	if (TEGRABL_NO_ERROR != internal_tegrabl_car_clk_enable(clk_id))
		return TEGRABL_ERR_INVALID;

	return TEGRABL_NO_ERROR;
}

static inline uint32_t div_round_off(uint32_t n, uint32_t d)
{
	if (((n) % (d)) >= ((d)/2))
		return (n) / (d) + 1;
	else
		return (n) / (d);
}

/**
 * @brief Configures the clock source and divider if needed
 * and enables clock for the module specified.
 *
 * @module - Module ID of the module
 * @instance - Instance of the module
 * @priv_data - module specific private data pointer to module specific clock
 * init data
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_clk_enable(tegrabl_module_t module,
					uint8_t instance,
					void *priv_data)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	/* Do QSPI specific init based on priv_data */
	if ((module == TEGRABL_MODULE_QSPI) && (priv_data != NULL)) {
		uint32_t src, rate_khz, rate_set_khz, c4_mux_sel, c4_vco_khz;
		struct qspi_clk_data *clk_data;

		clk_data = (struct qspi_clk_data *)priv_data;

		/* Map TEGRABL_CLK_SRC ids to bpmp-abi clk ids */
		switch (clk_data->clk_src) {
		case TEGRABL_CLK_SRC_PLLC4_MUXED:
			/* Fetch exact parent info in case of C4_MUXED */
			/* 1. Obtain pllc4 vco freq */
			internal_tegrabl_car_get_clk_rate(TEGRA186_CLK_PLLC4_VCO,
											  &c4_vco_khz);
			/* 2. Calculate divisor */
			tegrabl_car_get_clk_src_rate(TEGRABL_CLK_SRC_PLLC4_MUXED,
										 &rate_set_khz);
			c4_mux_sel = div_round_off(c4_vco_khz, rate_set_khz);
			/* 3. Obtain parent info */
			pr_info("c4 vco-div (mux selection) = %d\n", c4_mux_sel);
			switch (c4_mux_sel) {
			case (5):
				src = TEGRA186_CLK_PLLC4_OUT2;
				break;
			case (2):
				src = TEGRA186_CLK_PLLC4_VCO_DIV2;
				break;
			case (3):
			default:
				src = TEGRA186_CLK_PLLC4_OUT1;
			}
			break;
		case TEGRABL_CLK_SRC_CLK_M:
			src = TEGRABL_CLK_SRC_CLK_M;
			break;
		case TEGRABL_CLK_SRC_PLLP_OUT0:
		default:
			src = TEGRA186_CLK_PLLP_OUT0;
			break;
		}

		/* Set parent */
		err = internal_tegrabl_car_set_clk_src(TEGRA186_CLK_QSPI, src);
		if (err != TEGRABL_NO_ERROR)
			goto fail;

		/* Get parent rate */
		err = internal_tegrabl_car_get_clk_rate(src, &rate_set_khz);
		if (err != TEGRABL_NO_ERROR)
			goto fail;

		/* Derive clk rate from src rate and divisor */
		/* clk_rate = (pllp_freq * 2) / (N + 2) */
		rate_khz = (rate_set_khz << 1) / (clk_data->clk_divisor + 2);
		/* Round down rate to the nearest 1000 */
		rate_khz = ROUND_DOWN(rate_khz, 1000);

		pr_info("QSPI source rate = %d Khz\n", rate_set_khz);
		pr_info("Requested rate for QSPI clock = %d Khz\n", rate_khz);
		err = internal_tegrabl_car_set_clk_rate
				(TEGRA186_CLK_QSPI, rate_khz, &rate_set_khz);
		if (err != TEGRABL_NO_ERROR)
			goto fail;
		pr_info("BPMP-set rate for QSPI clk = %d Khz\n", rate_set_khz);

		/* Enable QSPI clk */
		err = internal_tegrabl_car_clk_enable(TEGRA186_CLK_QSPI);
		goto fail;
	}

	pr_debug("(%s,%d) %d, %d\n", __func__, __LINE__,
			 module, instance);

	err = internal_tegrabl_car_clk_enable(
			tegrabl_module_to_bpmp_id(module, instance, MOD_CLK));

fail:
	if (err != TEGRABL_NO_ERROR)
		err = TEGRABL_ERROR_HIGHEST_MODULE(err);

	return err;
}

/**
 * @brief  Disables clock for the module specified
 *
 * @module  Module ID of the module
 * @instance  Instance of the module
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_clk_disable(tegrabl_module_t module,
					uint8_t instance)
{
	pr_debug("(%s,%d) %d, %d\n", __func__, __LINE__,
			 module, instance);

	return internal_tegrabl_car_clk_disable(
			tegrabl_module_to_bpmp_id(module, instance, MOD_CLK));
}

/**
 * @brief Power downs plle.
 */
void tegrabl_car_disable_plle(void)
{
	pr_debug("(%s,%d)\n", __func__, __LINE__);

	internal_tegrabl_car_clk_disable(TEGRA186_CLK_PLLE);
}

/**
 * @brief - Configures PLLM0 for WB0 Override
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_pllm_wb0_override(void)
{
	/* Stub - Functionality not needed */
	return 0;
}

/**
 * @brief - Configures CAR dividers for slave TSC
 *
 * Configuration is done for both OSC and PLL paths.
 * If OSC >= 38400, Osc is chosen as source
 * else PLLP is chosen as source.
 *
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_setup_tsc_dividers(void)
{
	/* Stub - Functionality not needed */
	return TEGRABL_NO_ERROR;
}

/**
 * @brief - Set/Clear fuse register visibility
 *
 * @param visibility if true, it will make all reg visible otherwise invisible.
 *
 * @return existing visibility before programming the value
 */
bool tegrabl_set_fuse_reg_visibility(bool visibility)
{
	/* Stub to return TRUE
	 * BPMP fw keeps this enabled */
	TEGRABL_UNUSED(visibility);
	return true;
}

/**
 * @brief Puts the module in reset
 *
 * @module - Module ID of the module
 * @instance - Instance of the module
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_rst_set(tegrabl_module_t module,
				uint8_t instance)
{
	pr_debug("(%s,%d) %d, %d\n", __func__, __LINE__,
			 module, instance);

	return internal_tegrabl_car_rst(
				tegrabl_module_to_bpmp_id(module, instance, MOD_RST),
				CMD_RESET_ASSERT);
}

/**
 * @brief  Releases the module from reset
 *
 * @module - Module ID of the module
 * @instance - Instance of the module
 * @return - TEGRABL_NO_ERROR if success, error-reason otherwise.
 */
tegrabl_error_t tegrabl_car_rst_clear(tegrabl_module_t module,
				uint8_t instance)
{
	pr_debug("(%s,%d) %d, %d\n", __func__, __LINE__,
			 module, instance);

	return internal_tegrabl_car_rst(
				tegrabl_module_to_bpmp_id(module, instance, MOD_RST),
				CMD_RESET_DEASSERT);
}

/**
 * @brief - Returns the enum of oscillator frequency
 * @return - Enum value of current oscillator frequency
 */
enum tegrabl_clk_osc_freq tegrabl_get_osc_freq(void)
{
	uint32_t freq_khz;
	enum tegrabl_clk_osc_freq return_freq = TEGRABL_CLK_OSC_FREQ_UNKNOWN;

	pr_debug("(%s,%d)\n", __func__, __LINE__);

	if (tegrabl_car_get_osc_freq_khz(&freq_khz) == TEGRABL_NO_ERROR) {
		switch (freq_khz) {
		case (KHZ_13M - KHZ_P4M) ... (KHZ_13M + KHZ_P4M):
			return_freq = TEGRABL_CLK_OSC_FREQ_13;
			break;
		case (KHZ_19P2M - KHZ_P4M) ... (KHZ_19P2M + KHZ_P4M):
			return_freq = TEGRABL_CLK_OSC_FREQ_19_2;
			break;
		case (KHZ_12M - KHZ_P4M) ... (KHZ_12M + KHZ_P4M):
			return_freq = TEGRABL_CLK_OSC_FREQ_12;
			break;
		case (KHZ_26M - KHZ_P4M) ... (KHZ_26M + KHZ_P4M):
			return_freq = TEGRABL_CLK_OSC_FREQ_26;
			break;
		case (KHZ_16P8M - KHZ_P4M) ... (KHZ_16P8M + KHZ_P4M):
			return_freq = TEGRABL_CLK_OSC_FREQ_16_8;
			break;
		case (KHZ_38P4M - KHZ_P4M) ... (KHZ_38P4M + KHZ_P4M):
			return_freq = TEGRABL_CLK_OSC_FREQ_38_4;
			break;
		case (KHZ_48M - KHZ_P4M) ... (KHZ_48M + KHZ_P4M):
			return_freq = TEGRABL_CLK_OSC_FREQ_48;
			break;
		default:
			break;
		}
	}
	return return_freq;
}

tegrabl_error_t tegrabl_init_utmipll(void)
{
	/* Stub */
	/* usbf_clock_init takes care of utmipll init
	 * Refer last row of usb_clk_data for the utmipll config
	 */
	return TEGRABL_NO_ERROR;
}

void tegrabl_usbf_program_tracking_clock(bool is_enable)
{
	uint32_t dummy;
	int i;

	for (i = 0; i < NUM_USB_TRK_CLKS; i++) {
		if (is_enable == true) {
			/* 1. Enable clks */
			internal_tegrabl_car_clk_enable(usb_clk_data[i][NAME_INDEX]);
			/* 2. set clk parents */
			internal_tegrabl_car_set_clk_src(
					usb_clk_data[i][NAME_INDEX],
					usb_clk_data[i][USB_PARENT_INDEX]);
			/* 3. Set clk rates */
			internal_tegrabl_car_set_clk_rate(
					usb_clk_data[i][NAME_INDEX],
					usb_clk_data[i][USB_RATE_INDEX],
				&dummy);
		} else {
			internal_tegrabl_car_clk_disable(usb_clk_data[i][NAME_INDEX]);
		}
	}
	return;
}

tegrabl_error_t tegrabl_usbf_clock_init(void)
{
	uint32_t dummy;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	internal_tegrabl_car_set_clk_src(TEGRA186_CLK_XUSB_DEV,
									 TEGRA186_CLK_PLLP_OUT0);
	internal_tegrabl_car_set_clk_rate(TEGRA186_CLK_XUSB_DEV, RATE_XUSB_DEV_KHZ,
									  &dummy);
	tegrabl_udelay(2);
	internal_tegrabl_car_set_clk_rate(TEGRA186_CLK_XUSB_SS, RATE_XUSB_SS_KHZ,
									  &dummy);
	internal_tegrabl_car_set_clk_src(TEGRA186_CLK_XUSB_FS,
									 TEGRA186_CLK_PLL_U_48M);

	/* Take XUSB - DEV, SS out of reset */
	err = tegrabl_car_rst_clear(TEGRABL_MODULE_XUSB_DEV, 0);
	if (err != TEGRABL_NO_ERROR)
		goto fail;
	err = tegrabl_car_rst_clear(TEGRABL_MODULE_XUSB_SS, 0);
	if (err != TEGRABL_NO_ERROR)
		goto fail;
	err = tegrabl_car_rst_clear(TEGRABL_MODULE_XUSB_HOST, 0);
	if (err != TEGRABL_NO_ERROR)
		goto fail;
fail:
	return err;
}

static void tegrabl_ufs_program_clock(void)
{
	uint32_t reg_data = 0;
	uint32_t rate = 0;
	uint32_t i;
	bool enabled;

	for (i = 0; i < NUM_UFS_CLKS; i++) {
		/* 1. Enable clks */
		internal_tegrabl_car_clk_enable(ufs_clk_data[i][NAME_INDEX]);
		tegrabl_udelay(200);
		/* 2. set clk parents */
		if (ufs_clk_data[i][UFS_PARENT_INDEX] != TEGRA186_CLK_CLK_MAX)
			internal_tegrabl_car_set_clk_src(
					ufs_clk_data[i][NAME_INDEX],
					ufs_clk_data[i][UFS_PARENT_INDEX]);
		tegrabl_udelay(200);
		/* 3. Set clk rates */
		if (ufs_clk_data[i][UFS_RATE_INDEX] != 0)
			internal_tegrabl_car_set_clk_rate(
					ufs_clk_data[i][NAME_INDEX],
					ufs_clk_data[i][UFS_RATE_INDEX],
				&rate);
		tegrabl_udelay(200);
	}

	for (i = 0; i < NUM_UFS_CLKS; i++) {
		enabled =
			internal_tegrabl_car_clk_is_enabled(ufs_clk_data[i][NAME_INDEX]);
		internal_tegrabl_car_get_clk_rate(ufs_clk_data[i][NAME_INDEX], &rate);
		pr_info("index=%d enabled=%d rate=%u\n", i, enabled, rate);
	}

	for (i = 0; i < NUM_UFS_RSTS; i++) {
		internal_tegrabl_car_rst(ufs_rst_data[i], CMD_RESET_DEASSERT);
		tegrabl_udelay(200);
	}

	/*  Set the following PMC register bits to ‘0’ to remove
		isolation between UFSHC AO logic inputs coming from PSW domain */
	reg_data = NV_READ32(NV_ADDRESS_MAP_PMC_IMPL_BASE +
						 PMC_IMPL_UFSHC_PWR_CNTRL_0);
	reg_data = NV_FLD_SET_DRF_DEF(PMC_IMPL, UFSHC_PWR_CNTRL,
								  LP_ISOL_EN, DISABLE, reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_PMC_IMPL_BASE + PMC_IMPL_UFSHC_PWR_CNTRL_0,
			   reg_data);

	/* Enable reference clock to Device. OE pin on UFS GPIO pad */
	reg_data = NV_READ32(UFSHC_AUX_UFSHC_DEV_CTRL_0);
	reg_data = NV_FLD_SET_DRF_NUM(UFSHC_AUX, UFSHC_DEV_CTRL,
								  UFSHC_DEV_CLK_EN, 1, reg_data);
	NV_WRITE32(UFSHC_AUX_UFSHC_DEV_CTRL_0, reg_data);

	/* Release reset to device */
	reg_data = NV_READ32(UFSHC_AUX_UFSHC_DEV_CTRL_0);
	reg_data = NV_FLD_SET_DRF_NUM(UFSHC_AUX, UFSHC_DEV_CTRL,
								  UFSHC_DEV_RESET, 1, reg_data);
	NV_WRITE32(UFSHC_AUX_UFSHC_DEV_CTRL_0, reg_data);

	return;
}

void tegrabl_ufs_clock_deinit(void)
{
	uint32_t i;

	pr_info("disabling ufs clocks\n");
	for (i = NUM_UFS_CLKS - 1; i > 2; i--) {
		/* 1. disable clks */
		internal_tegrabl_car_clk_disable(ufs_clk_data[i][NAME_INDEX]);
	}

}

void tegrabl_ufs_disable_device(void)
{
        uint32_t reg_data;

        /* disable reference clock to Device. OE pin on UFS GPIO pad */
        reg_data = NV_READ32(UFSHC_AUX_UFSHC_DEV_CTRL_0);
        reg_data = NV_FLD_SET_DRF_NUM(UFSHC_AUX,
                        UFSHC_DEV_CTRL, UFSHC_DEV_CLK_EN, 0, reg_data);
        NV_WRITE32(UFSHC_AUX_UFSHC_DEV_CTRL_0, reg_data);

        /* reset to device.*/
        reg_data = NV_READ32(UFSHC_AUX_UFSHC_DEV_CTRL_0);
        reg_data = NV_FLD_SET_DRF_NUM(UFSHC_AUX,
                        UFSHC_DEV_CTRL, UFSHC_DEV_RESET, 0, reg_data);
        NV_WRITE32(UFSHC_AUX_UFSHC_DEV_CTRL_0, reg_data);

}

tegrabl_error_t tegrabl_ufs_clock_init(void)
{
	pr_info("Programming ufs clocks\n");
	/* Configure clocks and de-assert relevant reset modules */
	tegrabl_ufs_program_clock();

	return TEGRABL_NO_ERROR;
}

#if defined(CONFIG_ENABLE_QSPI)
tegrabl_error_t tegrabl_qspi_clk_div_mode(uint32_t val)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t dummy;
	/* Init last_val = 0xFFFF as unknown state */
	static uint32_t last_val = 0xFFFFU;

	/* Set last_val to unknown state if input is NOT:
	 * - 0: disable QSPI QDDR read
	 * - 1: enable QSPI QDDR read */
	if (val > 1U) {
		last_val = 0xFFFFU;
		goto done;
	}

	/* Return early if input val is the same as last input */
	if (last_val == val) {
		goto done;
	}

	pr_debug("QSPI clk div mode - input val = %u\n", val);

	if (val != 0U) { /* ENABLE QSPI QDDR READ */
		err = internal_tegrabl_car_set_clk_rate(TEGRA186_CLK_QSPI_OUT,
												0x0U,
												&dummy);
	} else {
		err = internal_tegrabl_car_set_clk_rate(TEGRA186_CLK_QSPI_OUT,
												0xFFFFFFFFU,
												&dummy);
	}

	if (err != TEGRABL_NO_ERROR) {
		err = TEGRABL_ERROR_HIGHEST_MODULE(err);
		goto done;
	}

	last_val = val;
done:
	return err;
}
#endif
