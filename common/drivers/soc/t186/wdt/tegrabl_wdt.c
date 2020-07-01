/*
 * Copyright (c) 2016 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_timer.h>
#include <tegrabl_io.h>
#include <tegrabl_wdt.h>
#include <tegrabl_drf.h>
#include <artke_bpmp.h>
#include <artke_top.h>
#include <tegrabl_soc_misc.h>

/*
 * Bug 200210349 doesn't allow wdt driver to program wdtcr register for any
 * wdt instance.
 */
#define ENABLE_WDTCR_CONFIG		0

enum tmr_src_id {
	SRC_USECCNT,
	SRC_OSCCNT,
	SRC_TSCCNT_29_0,
	SRC_TSCCNT_41_12,
};

static uint32_t tke_base_address[] = {
	NV_ADDRESS_MAP_BPMP_TKE_BASE,
	NV_ADDRESS_MAP_TOP_TKE_BASE,
};

#if ENABLE_WDTCR_CONFIG
static uint32_t wdtcr[] = {
	TKE_BPMP_WDT0_WDTCR_0,
	TKE_TOP_WDT1_WDTCR_0,
};
#endif

static uint32_t wdtcmdr[] = {
	TKE_BPMP_WDT0_WDTCMDR_0,
	TKE_TOP_WDT1_WDTCMDR_0,
};

static uint32_t wdtur[] = {
	TKE_BPMP_WDT0_WDTUR_0,
	TKE_TOP_WDT1_WDTUR_0,
};

static uint32_t tmrcssr[] = {
	TKE_BPMP_TIMER_TMR0_TMRCSSR_0,
	TKE_TOP_TIMER_TMR0_TMRCSSR_0,
};

static uint32_t tmrcr[] = {
	TKE_BPMP_TIMER_TMR0_TMRCR_0,
	TKE_TOP_TIMER_TMR0_TMRCR_0,
};

#define NV_TKE_READ(instance, reg) NV_READ32(tke_base_address[instance] + reg)

#define NV_TKE_WRITE(instance, reg, data)					\
		NV_WRITE32((tke_base_address[instance] + reg), data)

#define USEC_COUNTER_CYCLES_PER_SECOND	1000000
#define TSC_COUNTER_CYCLES_PER_SECOND	(3125 * 10000)
#define WDTUR_UNLOCK_PATTERN			0x0000C45A
#define IS_EXPIRY(val, num)				((val & (0x1 << num)) >> num)

static void tegrabl_wdt_load_period(enum tegrabl_wdt_instance instance,
	uint8_t period)
{
	uint32_t val;
	TEGRABL_UNUSED(period);

	/* program WDTCR */
	/* Load period */
#if ENABLE_WDTCR_CONFIG
	val = NV_TKE_READ(instance, wdtcr[instance]);
	val = NV_FLD_SET_DRF_NUM(TKE_BPMP, WDT0_WDTCR, Period, period, val);
	NV_TKE_WRITE(instance, wdtcr[instance], val);
#endif

	/* start counter */
	val = NV_TKE_READ(instance, wdtcmdr[instance]);
	val = NV_FLD_SET_DRF_NUM(TKE_BPMP, WDT0_WDTCMDR, StartCounter, 1, val);
	NV_TKE_WRITE(instance, wdtcmdr[instance], val);
}

static tegrabl_error_t tegrabl_wdt_configure(enum tegrabl_wdt_instance instance,
											 uint8_t expiry, uint8_t period,
											 enum tegrabl_wdt_clk_src clk_src)
{
	uint32_t val;
	uint32_t cycles_per_sec = 0;
	uint32_t wdt_to_tmr_src[TEGRABL_WDT_SRC_MAX] = {
		[TEGRABL_WDT_SRC_USECCNT] = SRC_USECCNT,
		[TEGRABL_WDT_SRC_OSCCNT] = SRC_OSCCNT,
		[TEGRABL_WDT_SRC_TSCCNT_29_0] = SRC_TSCCNT_29_0,
		[TEGRABL_WDT_SRC_TSCCNT_41_12] = SRC_TSCCNT_41_12,
	};

	TEGRABL_UNUSED(expiry);
	TEGRABL_UNUSED(period);

	if (clk_src == TEGRABL_WDT_SRC_USECCNT)
		cycles_per_sec = USEC_COUNTER_CYCLES_PER_SECOND;
	else if (clk_src == TEGRABL_WDT_SRC_TSCCNT_29_0)
		cycles_per_sec = TSC_COUNTER_CYCLES_PER_SECOND;
	else
		return TEGRABL_ERR_NOT_SUPPORTED;

	/* Program timer */
	val = NV_TKE_READ(instance, tmrcssr[instance]);
	val = NV_FLD_SET_DRF_NUM(TKE_BPMP, TIMER_TMR0_TMRCSSR, SRC_ID,
							 wdt_to_tmr_src[clk_src], val);
	NV_TKE_WRITE(instance, tmrcssr[instance], val);

	/* Program PTV, PER for timer */
	val = NV_TKE_READ(instance, tmrcr[instance]);
	val = NV_FLD_SET_DRF_NUM(TKE_BPMP, TIMER_TMR0_TMRCR, PER, 1, val);
	val = NV_FLD_SET_DRF_NUM(TKE_BPMP, TIMER_TMR0_TMRCR, PTV,
			cycles_per_sec, val);
	NV_TKE_WRITE(instance, tmrcr[instance], val);

	/* Enable timer */
	val = NV_TKE_READ(instance, tmrcr[instance]);
	val = NV_FLD_SET_DRF_NUM(TKE_BPMP, TIMER_TMR0_TMRCR, EN, 1, val);
	NV_TKE_WRITE(instance, tmrcr[instance], val);

	/* program WDTCR */
#if ENABLE_WDTCR_CONFIG
	val = NV_TKE_READ(instance, wdtcr[instance]);
	val = NV_FLD_SET_DRF_NUM(TKE_BPMP, WDT0_WDTCR, TimerSource, 0, val);
	val = NV_FLD_SET_DRF_NUM(TKE_BPMP, WDT0_WDTCR, LocalInterruptEnable,
			IS_EXPIRY(expiry, 0), val);
	val = NV_FLD_SET_DRF_NUM(TKE_BPMP, WDT0_WDTCR, LocalFIQEnable,
			IS_EXPIRY(expiry, 1), val);
	val = NV_FLD_SET_DRF_NUM(TKE_BPMP, WDT0_WDTCR, RemoteInterruptEnable,
			IS_EXPIRY(expiry, 2), val);
	val = NV_FLD_SET_DRF_NUM(TKE_BPMP, WDT0_WDTCR, SystemDebugResetEnable,
			IS_EXPIRY(expiry, 3), val);
	val = NV_FLD_SET_DRF_NUM(TKE_BPMP, WDT0_WDTCR, SystemPOResetEnable,
			IS_EXPIRY(expiry, 4), val);
	NV_TKE_WRITE(instance, wdtcr[instance], val);
#endif

	/* Load period and start counter */
	tegrabl_wdt_load_period(instance, period);

	return TEGRABL_NO_ERROR;
}

void tegrabl_wdt_disable(enum tegrabl_wdt_instance instance)
{
	uint32_t val;

	/* set StartCounter to 0 */
	val = NV_TKE_READ(instance, wdtcmdr[instance]);
	val = NV_FLD_SET_DRF_NUM(TKE_BPMP, WDT0_WDTCMDR, StartCounter, 0, val);
	NV_TKE_WRITE(instance, wdtcmdr[instance], val);

	/* program unlock pattern */
	NV_TKE_WRITE(instance, wdtur[instance], WDTUR_UNLOCK_PATTERN);

	/* Set Disable Counter*/
	val = NV_TKE_READ(instance, wdtcmdr[instance]);
	val = NV_FLD_SET_DRF_NUM(TKE_BPMP, WDT0_WDTCMDR, DisableCounter, 1, val);
	NV_TKE_WRITE(instance, wdtcmdr[instance], val);
}

tegrabl_error_t tegrabl_wdt_enable(enum tegrabl_wdt_instance instance,
								   uint8_t expiry, uint8_t period,
								   enum tegrabl_wdt_clk_src clk_src)
{
	if (tegrabl_is_wdt_enable()) {
		return tegrabl_wdt_configure(instance, expiry, period, clk_src);
	} else {
		pr_debug("WDT not enabled\n");
		return TEGRABL_NO_ERROR;
	}
}

void tegrabl_wdt_load_or_disable(enum tegrabl_wdt_instance instance,
	uint8_t period)
{
	if (tegrabl_is_wdt_enable())
		tegrabl_wdt_load_period(instance, period);
	else
		tegrabl_wdt_disable(instance);
}
