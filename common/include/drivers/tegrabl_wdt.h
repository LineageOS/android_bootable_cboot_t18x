/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

#ifndef TEGRABL_WDT_H
#define TEGRABL_WDT_H

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_wdt_misc.h>

/**
 * @brief enum specifying the wdt clock sources
 */
enum tegrabl_wdt_clk_src {
	TEGRABL_WDT_SRC_USECCNT,
	TEGRABL_WDT_SRC_OSCCNT,
	TEGRABL_WDT_SRC_TSCCNT_29_0,
	TEGRABL_WDT_SRC_TSCCNT_41_12,
	TEGRABL_WDT_SRC_MAX,
};

/**
 * @brief disable wdt
 *
 * @param instance wdt instance to be disabled
 */
void tegrabl_wdt_disable(enum tegrabl_wdt_instance instance);

/**
 * @brief program watchdog timer based on odm data bit and HALT_IN_FIQ bit
 *
 * @param instance wdt instance to be configured
 * @param expiry enable/disable corresponding expiries
 *		  bit 0 indicates first_expiry, bit1 indicates second_expiry and so on
 * @param period timer expiry period in seconds
 * @param clk_src specifies clk_src from enum tegrabl_wdt_clk_src
 *
 * @return TEGRABL_NO_ERROR if successful, else proper error code
 *
 */
tegrabl_error_t tegrabl_wdt_enable(enum tegrabl_wdt_instance instance,
								   uint8_t expiry, uint8_t period,
								   enum tegrabl_wdt_clk_src clk_src);

/**
 * @brief load wdt peroid and start counter / disable wdt
 * only if WDT configure conditions are met
 *
 * @param instance wdt instance to be disabled
 * @param period timer expiry period in seconds
 */
void tegrabl_wdt_load_or_disable(enum tegrabl_wdt_instance instance,
	uint8_t period);


#endif
