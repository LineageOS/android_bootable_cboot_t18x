/*
 * Copyright (c) 2015-2016 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdint.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_cpu_arch.h>
#include <tegrabl_timer.h>
#include <tegrabl_io.h>

/* The following parameters denote that on ASMLOOP_COUNT
  * number of iterations of the asm-delay loop, the time
  * elapsed is roughly ASMLOOP_DELAY_US usecs as measured on
  * BPMP/R5 running at 102Mhz */
 #define ASMLOOP_DELAY_US	30
 #define ASMLOOP_COUNT		6

time_t tegrabl_get_timestamp_us(void)
{
	return NV_READ32(NV_ADDRESS_MAP_TSCUS_BASE);
}

time_t tegrabl_get_timestamp_ms(void)
{
	return tegrabl_get_timestamp_us()/1000;
}

void tegrabl_udelay(time_t usec)
{
	uint32_t i = 0;
	time_t t0;
	time_t t1;

	t0 = tegrabl_get_timestamp_us();

	if (usec > ASMLOOP_DELAY_US) {
		while (1) {
			t1 = tegrabl_get_timestamp_us();
			/* its 30usec from here */
			if ((t1 - t0) > (usec - ASMLOOP_DELAY_US))
				break;
			for (i = 0; i < ASMLOOP_COUNT; i++) {
				tegrabl_nop();
			}
		}
	}
	t1 = tegrabl_get_timestamp_us();

	while ((t1 - t0) <= usec)
		t1 = tegrabl_get_timestamp_us();
}

void tegrabl_mdelay(time_t msec)
{
	tegrabl_udelay(msec * 1000);
}
