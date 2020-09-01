/*
* Copyright (c) 2016-2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <tegrabl_debug.h>
#include <tegrabl_module.h>
#include <tegrabl_error.h>
#include <tegrabl_timer.h>
#include <tegrabl_clock.h>
#include <tegrabl_clk_ufs.h>
#include <tegrabl_clk_rst_private.h>
#include <tegrabl_io.h>

#define UFSHC_AUX_UFSHC_DEV_CTRL_0 _MK_ADDR_CONST(0x2460014)
#define UFSHC_AUX_UFSHC_DEV_CTRL_0_UFSHC_DEV_CLK_EN_RANGE  0:0
#define UFSHC_AUX_UFSHC_DEV_CTRL_0_UFSHC_DEV_RESET_RANGE   1:1
static const
struct utmi_pll_clock_params utmi_pll_baseinfo[TEGRABL_CLK_OSC_FREQ_MAX_VAL] = {
	/*N */ /*M*/
	{0x4A, 0x01},
	{0x39, 0x01},
	{0x00, 0x00},
	{0x00, 0x00},
	{0x32, 0x01},
	{0x19, 0x01},
	{0x00, 0x00},
	{0x00, 0x00},
	{0x50, 0x01},
	{0x28, 0x02},
	{0x00, 0x00},
	{0x00, 0x00},
	{0x4A, 0x02}
};
static const
struct usb_pll_delay_params vusb_pll_delay_params[TEGRABL_CLK_OSC_FREQ_MAX_VAL] = {
	{0x02, 0x33, 0x09, 0x7F},
	{0x03, 0x42, 0x0B, 0xA5},
	{0x00, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x00, 0x00},
	{0x03, 0x4B, 0x0C, 0xBC},
	{0x05, 0x96, 0x18, 0x177},
	{0x00, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x00, 0x00},
	{0x02, 0x2F, 0x08, 0x76},
	{0x06, 0xBC, 0x1F, 0x1D5},
	{0x00, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x00, 0x00},
	{0x04, 0x66, 0x11, 0xFE}
};

static tegrabl_clk_osc_freq_t tegrabl_ufs_get_osc_freq(void)
{
	uint32_t reg;
	reg = NV_CLK_RST_READ_REG(OSC_CTRL);
	return (tegrabl_clk_osc_freq_t)NV_DRF_VAL(CLK_RST_CONTROLLER,
		OSC_CTRL, OSC_FREQ, reg);
}

static
void tegrabl_ufs_xusb_device_setup_sw_control_utmi_pll(
	tegrabl_clk_osc_freq_t osc_freq)
{
	uint32_t reg_data = 0;
	uint32_t lock_time = 0;
	uint32_t utmi_pll_up = 0;

	/* Check if xusb boot brought up UTMI PLL */
	reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_UTMIPLL_HW_PWRDN_CFG0_0);
	if(NV_DRF_VAL(CLK_RST_CONTROLLER,
		UTMIPLL_HW_PWRDN_CFG0, UTMIPLL_LOCK, reg_data)) {
		utmi_pll_up = 1;
	}
	if (utmi_pll_up == 0U) {
		reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
				CLK_RST_CONTROLLER_UTMIPLL_HW_PWRDN_CFG0_0);

		reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
				UTMIPLL_HW_PWRDN_CFG0,
				UTMIPLL_IDDQ_SWCTL,
				0x1, reg_data);

		reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
				UTMIPLL_HW_PWRDN_CFG0,
				UTMIPLL_IDDQ_OVERRIDE_VALUE,
				0x0, reg_data);

		NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE+
				CLK_RST_CONTROLLER_UTMIPLL_HW_PWRDN_CFG0_0,
				reg_data);

		tegrabl_udelay(5);


		/* Configure UTMI PLL dividers based on oscillator frequency.*/
		reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
				CLK_RST_CONTROLLER_UTMIP_PLL_CFG0_0);

		reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP,
				PLL_CFG0,UTMIP_PLL_NDIV,
				utmi_pll_baseinfo[osc_freq].n,
				reg_data);

		reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP,
				PLL_CFG0,UTMIP_PLL_MDIV,
				utmi_pll_baseinfo[osc_freq].m,
				reg_data);

		NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_UTMIP_PLL_CFG0_0, reg_data);


		reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP,
				PLL_CFG2,
				UTMIP_PLLU_STABLE_COUNT,
				0,
				reg_data);

		reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, UTMIP_PLL_CFG2,
				UTMIP_PLL_ACTIVE_DLY_COUNT,
				vusb_pll_delay_params[osc_freq].active_delay_count,
						reg_data);

		NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_UTMIP_PLL_CFG2_0, reg_data);

		/* Set PLL enable delay count and Crystal frequency count ( clock reset domain)*/
		reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
				CLK_RST_CONTROLLER_UTMIP_PLL_CFG1_0);

		reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER_UTMIP, PLL_CFG1,
					UTMIP_PLLU_ENABLE_DLY_COUNT,
					0,
					reg_data);

		reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, UTMIP_PLL_CFG1,
						UTMIP_XTAL_FREQ_COUNT,
 						vusb_pll_delay_params[osc_freq].xtal_freq_count,
 						reg_data);
		reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, UTMIP_PLL_CFG1,
					UTMIP_FORCE_PLL_ENABLE_POWERUP,
					0x1,
					reg_data);

		reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, UTMIP_PLL_CFG1,
						UTMIP_FORCE_PLL_ENABLE_POWERDOWN,
						0x0,
						reg_data);
		reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, UTMIP_PLL_CFG1,
						UTMIP_FORCE_PLL_ACTIVE_POWERDOWN,
						0x0,
						reg_data);

		NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_UTMIP_PLL_CFG1_0, reg_data);
        
		lock_time = 100;
		while(lock_time) {
			reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE+
					CLK_RST_CONTROLLER_UTMIPLL_HW_PWRDN_CFG0_0);
			if(NV_DRF_VAL(CLK_RST_CONTROLLER,
				UTMIPLL_HW_PWRDN_CFG0,
				UTMIPLL_LOCK,
				reg_data)) {
				break;
			}
			tegrabl_udelay(1);
			lock_time--;
		}


		reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
				CLK_RST_CONTROLLER_UTMIP_PLL_CFG2_0);
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

		NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_UTMIP_PLL_CFG2_0, reg_data);

		tegrabl_udelay(2);
	}

}

static void tegrabl_ufs_clock_enable(void)
{

	uint32_t reg_data; 
	uint32_t lock_timeout;
	tegrabl_clk_osc_freq_t osc_freq;

	osc_freq = tegrabl_ufs_get_osc_freq();


	tegrabl_ufs_xusb_device_setup_sw_control_utmi_pll(osc_freq);

	/* Bring up PLLREFE. Clk input is UTMIP branch @60 Mhz */
	reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
					CLK_RST_CONTROLLER_PLLREFE_MISC_0);
	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
			PLLREFE_MISC,
			PLLREFE_IDDQ,
			OFF,
			reg_data);

	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_PLLREFE_MISC_0, reg_data);

	tegrabl_udelay(5);

	/* We need PLLREFE @625 Mhz which is fed to M-PHY RX and TX blocks.*/
	reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_PLLREFE_BASE_0);
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLREFE_BASE, PLLREFE_DIVM,
			0xc,
			reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLREFE_BASE, PLLREFE_DIVN,
			0x7d,
			reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, PLLREFE_BASE, PLLREFE_DIVP,
			0x2,
			reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_PLLREFE_BASE_0, reg_data);

	reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_PLLREFE_BASE_0);
	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
			PLLREFE_BASE,
			PLLREFE_ENABLE,
			ENABLE,
			reg_data);

	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_PLLREFE_BASE_0, reg_data);

	tegrabl_udelay(2);

	/* Poll for lock */
	lock_timeout = 1000;
	while(lock_timeout) {
		reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE + CLK_RST_CONTROLLER_PLLREFE_MISC_0);
		if(NV_DRF_VAL(CLK_RST_CONTROLLER, PLLREFE_MISC, PLLREFE_LOCK, reg_data))
			break;
		tegrabl_udelay(1);
		lock_timeout--;
	}

	/* Enable clks for UFSHC and UFSDEV_REF */
	reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_OUT_ENB_UFS_0);
	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
			CLK_OUT_ENB_UFS,
			CLK_ENB_UFSHC,
			ENABLE,
			reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
			CLK_OUT_ENB_UFS,
			CLK_ENB_UFSDEV_REF,
			ENABLE,
			reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_OUT_ENB_UFS_0, reg_data);

	tegrabl_udelay(1);

	/* Set UFSHC clock src to PLLP */
	reg_data =  NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_SOURCE_UFSHC_CG_SYS_0);
	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
			CLK_SOURCE_UFSHC_CG_SYS,
			UFSHC_CG_SYS_CLK_SRC,
			PLLP_OUT0,
			reg_data);
	/* Target Frequency = 51 Mhz from PLLP. */
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
			CLK_SOURCE_UFSHC_CG_SYS,
			UFSHC_CG_SYS_CLK_DIVISOR,
			0xE,
			reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_SOURCE_UFSHC_CG_SYS_0, reg_data);

	tegrabl_udelay(1);

	/* Set Device reference clock source to osc (CLK_M, 38.4 Mhz) */
	reg_data =  NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_SOURCE_UFSDEV_REF_0);
	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
			CLK_SOURCE_UFSDEV_REF,
			UFSDEV_REF_CLK_SRC,
			CLK_M,
			reg_data);
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
			CLK_SOURCE_UFSDEV_REF,
			UFSDEV_REF_CLK_DIVISOR,
			0,
			reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_SOURCE_UFSDEV_REF_0,
			reg_data);

	tegrabl_udelay(1);

	/* Enable MPHY Clocks.*/
	reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_OUT_ENB_MPHY_0);

	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_MPHY,
			CLK_ENB_MPHY_CORE_PLL_FIXED, ENABLE, reg_data);

	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_MPHY,
			CLK_ENB_MPHY_TX_1MHZ_REF, ENABLE, reg_data);

	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_MPHY,
			CLK_ENB_MPHY_IOBIST, ENABLE, reg_data);

	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_MPHY,
			CLK_ENB_MPHY_L0_TX_LS_3XBIT, ENABLE, reg_data);

	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_MPHY,
			CLK_ENB_MPHY_L0_TX_SYMB, ENABLE, reg_data);

	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_MPHY,
			CLK_ENB_MPHY_L0_RX_LS_BIT, ENABLE, reg_data);

	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_MPHY,
			CLK_ENB_MPHY_L0_RX_SYMB, ENABLE, reg_data);

	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_MPHY,
			CLK_ENB_MPHY_L0_RX_ANA, ENABLE, reg_data);

	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_MPHY,
			CLK_ENB_MPHY_L1_RX_ANA, ENABLE, reg_data);

	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_OUT_ENB_MPHY_0, reg_data);

	tegrabl_udelay(1);

	/* Set MPHY core PLL to be 625/3=208 Mhz */
	reg_data =  NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_SOURCE_MPHY_CORE_PLL_FIXED_0);
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
			CLK_SOURCE_MPHY_CORE_PLL_FIXED,
			MPHY_CORE_PLL_FIXED_CLK_DIVISOR,
			0x4,
			reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_SOURCE_MPHY_CORE_PLL_FIXED_0, reg_data);

	reg_data =  NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_SOURCE_MPHY_TX_1MHZ_REF_0);
	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER,
			CLK_SOURCE_MPHY_TX_1MHZ_REF,
			MPHY_TX_1MHZ_REF_CLK_DIVISOR,
			0x4A,
			reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_SOURCE_MPHY_TX_1MHZ_REF_0, reg_data);

	tegrabl_udelay(1);

}

static void tegrabl_ufs_reset_disable(void)
{
	uint32_t reg_data;

	/* MPHY RESETS Disable */
	reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_RST_DEV_MPHY_0);

	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
			RST_DEV_MPHY,
			SWR_MPHY_CLK_CTL_RST,
			DISABLE,
			reg_data);
	
	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
			RST_DEV_MPHY,
			SWR_MPHY_L1_RX_RST,
			DISABLE,
			reg_data);

	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
			RST_DEV_MPHY,
			SWR_MPHY_L1_TX_RST,
			DISABLE,
			reg_data);

	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
			RST_DEV_MPHY,
			SWR_MPHY_L0_RX_RST,
			DISABLE,
			reg_data);
	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
			RST_DEV_MPHY,
			SWR_MPHY_L0_TX_RST,
			DISABLE,
			reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_RST_DEV_MPHY_0, reg_data);

	tegrabl_udelay(1);

	/* De-Assert UFS resets. */

	reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_RST_DEV_UFS_AON_0);
	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
			RST_DEV_UFS_AON,
			SWR_UFSHC_RST,
			DISABLE,
			reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_RST_DEV_UFS_AON_0, reg_data);

	tegrabl_udelay(1);

	reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_RST_DEV_UFS_0);
	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
			RST_DEV_UFS,
			SWR_UFSHC_AXI_M_RST,
			DISABLE,
			reg_data);

	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_RST_DEV_UFS_0, reg_data);

	tegrabl_udelay(1);

	reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_RST_DEV_UFS_0);
	reg_data = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER,
			RST_DEV_UFS,
			SWR_UFSHC_LP_RST,
			DISABLE,
			reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
		CLK_RST_CONTROLLER_RST_DEV_UFS_0, reg_data);

	tegrabl_udelay(1);

	reg_data = NV_READ32(NV_ADDRESS_MAP_PMC_IMPL_BASE +
			PMC_IMPL_UFSHC_PWR_CNTRL_0);
	reg_data = NV_FLD_SET_DRF_DEF(PMC_IMPL,
			UFSHC_PWR_CNTRL,LP_ISOL_EN, DISABLE, reg_data);
	NV_WRITE32(NV_ADDRESS_MAP_PMC_IMPL_BASE +
		PMC_IMPL_UFSHC_PWR_CNTRL_0, reg_data);
#if !defined(CONFIG_ENABLE_FPGA)
	/* Enable reference clock to Device. OE pin on UFS GPIO pad */
	reg_data = NV_READ32(UFSHC_AUX_UFSHC_DEV_CTRL_0);
	reg_data = NV_FLD_SET_DRF_NUM(UFSHC_AUX,
			UFSHC_DEV_CTRL, UFSHC_DEV_CLK_EN, 1, reg_data);
	NV_WRITE32(UFSHC_AUX_UFSHC_DEV_CTRL_0, reg_data);

	/* Release reset to device.*/
	reg_data = NV_READ32(UFSHC_AUX_UFSHC_DEV_CTRL_0);
	reg_data = NV_FLD_SET_DRF_NUM(UFSHC_AUX,
			UFSHC_DEV_CTRL, UFSHC_DEV_RESET, 1, reg_data);
	NV_WRITE32(UFSHC_AUX_UFSHC_DEV_CTRL_0, reg_data);
#endif
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

	/* putting mphy clocks in default state */
	reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_MPHY_MISC_0);

	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_MPHY_MISC,
			MPHY_FORCE_LS_MODE, 1, reg_data);

	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
                        CLK_RST_CONTROLLER_CLK_MPHY_MISC_0, reg_data);
	tegrabl_mdelay(1);

	reg_data = NV_READ32(NV_ADDRESS_MAP_CAR_BASE +
			CLK_RST_CONTROLLER_CLK_MPHY_MISC_0);

	reg_data = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CLK_MPHY_MISC,
			MPHY_FORCE_LS_MODE, 0, reg_data);

	NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE +
                        CLK_RST_CONTROLLER_CLK_MPHY_MISC_0, reg_data);
	tegrabl_mdelay(1);
}

tegrabl_error_t tegrabl_ufs_clock_init(void)
{
	/* enable MPHY ufshc clocks*/
	tegrabl_ufs_clock_enable();
	/* reset disable for MPHY and ufshc */
	tegrabl_ufs_reset_disable();

	return TEGRABL_NO_ERROR;
}


