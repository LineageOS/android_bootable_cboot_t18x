/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef __tegra_ivc_instance_h__
#define __tegra_ivc_instance_h__

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#define IVC_ALIGN 64

struct ivc;
struct ivc_channel_header;

typedef void (* ivc_notify_function)(struct ivc *);

struct ivc {
	volatile struct ivc_channel_header *rx_channel, *tx_channel;
	uint32_t w_pos, r_pos;

	ivc_notify_function notify;
	uint32_t nframes, frame_size;
};

struct tegra_hv_queue_data;

/*
 * Helper routine for initializing IVC queues using hypervisor data structures.
 */
void tegra_ivc_hv_init(struct ivc *ivc, uintptr_t area_start,
		uintptr_t area_size, const struct tegra_hv_queue_data *qd,
		ivc_notify_function notify);

void tegra_ivc_hv_analyze_qd(const struct tegra_hv_queue_data *qd,
		unsigned int guestid, uintptr_t area_start, uintptr_t area_size,
		uintptr_t *rx_base, uintptr_t *tx_base,
		unsigned int *other_guestid);

int tegra_ivc_init(struct ivc *ivc, uintptr_t rx_base, uintptr_t tx_base,
		unsigned nframes, unsigned frame_size,
		ivc_notify_function notify);
unsigned tegra_ivc_total_queue_size(unsigned queue_size);
size_t tegra_ivc_align(size_t size);
int tegra_ivc_channel_sync(struct ivc *ivc);

/* Link dependencies. */
void ivc_printf(const char *format, ...);
void ivc_cache_invalidate(void *addr, size_t len);
void ivc_cache_flush(void *addr, size_t len);

#endif /* __tegra_ivc_instance_h__ */
