/*
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <tegra-ivc.h>
#include <tegra-ivc-instance.h>
#include "tegra-ivc-internal.h"

#include <stddef.h>
#include <string.h>
#include <errno.h>

/*
 * IVC channel reset protocol.
 *
 * Each end uses its tx_channel.state to indicate its synchronization state.
 */
enum ivc_state {
	/*
	 * This value is zero for backwards compatibility with services that
	 * assume channels to be initially zeroed. Such channels are in an
	 * initially valid state, but cannot be asynchronously reset, and must
	 * maintain a valid state at all times.
	 *
	 * The transmitting end can enter the established state from the sync or
	 * ack state when it observes the receiving endpoint in the ack or
	 * established state, indicating that has cleared the counters in our
	 * rx_channel.
	 */
	ivc_state_established = 0,

	/*
	 * If an endpoint is observed in the sync state, the remote endpoint is
	 * allowed to clear the counters it owns asynchronously with respect to
	 * the current endpoint. Therefore, the current endpoint is no longer
	 * allowed to communicate.
	 */
	ivc_state_sync,

	/*
	 * When the transmitting end observes the receiving end in the sync
	 * state, it can clear the w_count and r_count and transition to the ack
	 * state. If the remote endpoint observes us in the ack state, it can
	 * return to the established state once it has cleared its counters.
	 */
	ivc_state_ack
};

/*
 * This structure is divided into two-cache aligned parts, the first is only
 * written through the tx_channel pointer, while the second is only written
 * through the rx_channel pointer. This delineates ownership of the cache lines,
 * which is critical to performance and necessary in non-cache coherent
 * implementations.
 */
struct ivc_channel_header {
	union {
		struct {
			/* fields owned by the transmitting end */
			uint32_t w_count;
			uint32_t state;
		};
		uint8_t w_align[IVC_ALIGN];
	};
	union {
		/* fields owned by the receiving end */
		uint32_t r_count;
		uint8_t r_align[IVC_ALIGN];
	};
};

void IVC_WEAK ivc_printf(const char *format IVC_UNUSED, ...)
{
}
void IVC_WEAK ivc_cache_invalidate(void *addr IVC_UNUSED, size_t len IVC_UNUSED)
{
}
void IVC_WEAK ivc_cache_flush(void *addr IVC_UNUSED, size_t len IVC_UNUSED)
{
}

static inline int ivc_channel_empty(struct ivc *ivc,
		volatile struct ivc_channel_header *ch)
{
	/*
	 * This function performs multiple checks on the same values with
	 * security implications, so sample the counters' current values in
	 * shared memory to ensure that these checks use the same values.
	 */
	uint32_t w_count = ch->w_count;
	uint32_t r_count = ch->r_count;

	/*
	 * Perform an over-full check to prevent denial of service attacks where
	 * a server could be easily fooled into believing that there's an
	 * extremely large number of frames ready, since receivers are not
	 * expected to check for full or over-full conditions.
	 *
	 * Although the channel isn't empty, this is an invalid case caused by
	 * a potentially malicious peer, so returning empty is safer, because it
	 * gives the impression that the channel has gone silent.
	 */
	if (w_count - r_count > ivc->nframes)
		return 1;

	return w_count == r_count;
}

static inline int ivc_channel_full(struct ivc *ivc,
		volatile struct ivc_channel_header *ch)
{
	/*
	 * Invalid cases where the counters indicate that the queue is over
	 * capacity also appear full.
	 */
	return ch->w_count - ch->r_count >= ivc->nframes;
}

static inline uint32_t ivc_channel_avail_count(struct ivc *ivc IVC_UNUSED,
		volatile struct ivc_channel_header *ch)
{
	/*
	 * This function isn't expected to be used in scenarios where an
	 * over-full situation can lead to denial of service attacks. See the
	 * comment in ivc_channel_empty() for an explanation about special
	 * over-full considerations.
	 */
	return ch->w_count - ch->r_count;
}

static inline void ivc_advance_tx(struct ivc *ivc)
{
	ivc->tx_channel->w_count = ivc->tx_channel->w_count + 1;

	if (ivc->w_pos == ivc->nframes - 1)
		ivc->w_pos = 0;
	else
		ivc->w_pos++;
}

static inline void ivc_advance_rx(struct ivc *ivc)
{
	ivc->rx_channel->r_count = ivc->rx_channel->r_count + 1;

	if (ivc->r_pos == ivc->nframes - 1)
		ivc->r_pos = 0;
	else
		ivc->r_pos++;
}


static inline int ivc_check_read(struct ivc *ivc)
{
	/*
	 * tx_channel->state is set locally, so it is not synchronized with
	 * state from the remote peer. The remote peer cannot reset its
	 * transmit counters until we've acknowledged its synchronization
	 * request, so no additional synchronization is required because an
	 * asynchronous transition of rx_channel->state to ivc_state_ack is not
	 * allowed.
	 */
	if (ivc->tx_channel->state != ivc_state_established)
		return -ECONNRESET;

	/*
	* Avoid unnecessary invalidations when performing repeated accesses to
	* an IVC channel by checking the old queue pointers first.
	* Synchronization is only necessary when these pointers indicate empty
	* or full.
	*/
	if (!ivc_channel_empty(ivc, ivc->rx_channel))
		return 0;

	ivc_cache_invalidate((void *)&ivc->rx_channel->w_count,
			sizeof(ivc->rx_channel->w_count));
	return ivc_channel_empty(ivc, ivc->rx_channel) ? -ENOMEM : 0;
}

static inline int ivc_check_write(struct ivc *ivc)
{
	if (ivc->tx_channel->state != ivc_state_established)
		return -ECONNRESET;

	if (!ivc_channel_full(ivc, ivc->tx_channel))
		return 0;

	ivc_cache_invalidate((void *)&ivc->tx_channel->r_count,
			sizeof(ivc->tx_channel->r_count));
	return ivc_channel_full(ivc, ivc->tx_channel) ? -ENOMEM : 0;
}

int tegra_ivc_can_read(struct ivc *ivc)
{
	return ivc_check_read(ivc) == 0;
}

int tegra_ivc_can_write(struct ivc *ivc)
{
	return ivc_check_write(ivc) == 0;
}

int tegra_ivc_tx_empty(struct ivc *ivc)
{
	ivc_cache_invalidate((void *)&ivc->tx_channel->r_count,
			sizeof(ivc->tx_channel->r_count));
	return ivc_channel_empty(ivc, ivc->tx_channel);
}

static void *ivc_frame_pointer(struct ivc *ivc,
		volatile struct ivc_channel_header *ch, uint32_t frame)
{
	IVC_ASSERT(frame < ivc->nframes);
	return (void *)((uintptr_t)(ch + 1) + ivc->frame_size * frame);
}

static void ivc_invalidate_frame(struct ivc *ivc,
		volatile struct ivc_channel_header *ch, uint32_t frame,
		uint32_t offset, size_t len)
{
	IVC_ASSERT(frame < ivc->nframes);
	ivc_cache_invalidate((void *)((uintptr_t)(ch + 1) +
			ivc->frame_size * frame + offset), len);
}

static void ivc_flush_frame(struct ivc *ivc,
		volatile struct ivc_channel_header *ch, uint32_t frame,
		uint32_t offset, size_t len)
{
	IVC_ASSERT(frame < ivc->nframes);
	ivc_cache_flush((void *)((uintptr_t)(ch + 1) +
			ivc->frame_size * frame + offset), len);
}

int tegra_ivc_read(struct ivc *ivc, void *buf, size_t max_read)
{
	const void *src;
	int result;

	if (max_read > ivc->frame_size)
		return -E2BIG;


	result = ivc_check_read(ivc);
	if (result)
		return result;

	/*
	 * Order observation of w_pos potentially indicating new data before
	 * data read.
	 */
	ivc_rmb();

	ivc_invalidate_frame(ivc, ivc->rx_channel, ivc->r_pos, 0, max_read);
	src = ivc_frame_pointer(ivc, ivc->rx_channel, ivc->r_pos);

	memcpy(buf, src, max_read);

	ivc_advance_rx(ivc);
	ivc_cache_flush((void *)&ivc->rx_channel->r_count,
			sizeof(ivc->rx_channel->r_count));

	/*
	 * Ensure our write to r_pos occurs before our read from w_pos.
	 */
	ivc_mb();

	/*
	 * Notify only upon transition from full to non-full.
	 * The available count can only asynchronously increase, so the
	 * worst possible side-effect will be a spurious notification.
	 */
	ivc_cache_invalidate((void *)&ivc->rx_channel->w_count,
			sizeof(ivc->rx_channel->w_count));
	if (ivc_channel_avail_count(ivc, ivc->rx_channel) == ivc->nframes - 1)
		ivc->notify(ivc);

	return (int)max_read;
}

/* peek in the next rx buffer at offset off, the count bytes */
int tegra_ivc_read_peek(struct ivc *ivc, void *buf, size_t off, size_t count)
{
	const void *src;
	int result;

	if (off > ivc->frame_size || off + count > ivc->frame_size)
		return -E2BIG;

	result = ivc_check_read(ivc);
	if (result)
		return result;

	/*
	 * Order observation of w_pos potentially indicating new data before
	 * data read.
	 */
	ivc_rmb();

	ivc_invalidate_frame(ivc, ivc->rx_channel, ivc->r_pos, off, count);
	src = ivc_frame_pointer(ivc, ivc->rx_channel, ivc->r_pos);

	memcpy(buf, (void *)((uintptr_t)src + off), count);

	/* note, no interrupt is generated */

	return (int)count;
}

/* directly peek at the next frame rx'ed */
void *tegra_ivc_read_get_next_frame(struct ivc *ivc)
{
	if (ivc_check_read(ivc))
		return NULL;

	/*
	 * Order observation of w_pos potentially indicating new data before
	 * data read.
	 */
	ivc_rmb();

	ivc_invalidate_frame(ivc, ivc->rx_channel, ivc->r_pos, 0,
			ivc->frame_size);
	return ivc_frame_pointer(ivc, ivc->rx_channel, ivc->r_pos);
}

int tegra_ivc_read_advance(struct ivc *ivc)
{
	/*
	 * No read barriers or synchronization here: the caller is expected to
	 * have already observed the channel non-empty. This check is just to
	 * catch programming errors.
	 */
	int result = ivc_check_read(ivc);
	if (result)
		return result;

	ivc_advance_rx(ivc);
	ivc_cache_flush((void *)&ivc->rx_channel->r_count,
			sizeof(ivc->rx_channel->r_count));

	/*
	 * Ensure our write to r_pos occurs before our read from w_pos.
	 */
	ivc_mb();

	/*
	 * Notify only upon transition from full to non-full.
	 * The available count can only asynchronously increase, so the
	 * worst possible side-effect will be a spurious notification.
	 */
	ivc_cache_invalidate((void *)&ivc->rx_channel->w_count,
			sizeof(ivc->rx_channel->w_count));
	if (ivc_channel_avail_count(ivc, ivc->rx_channel) == ivc->nframes - 1)
		ivc->notify(ivc);

	return 0;
}

int tegra_ivc_write(struct ivc *ivc, const void *buf, size_t size)
{
	void *p;
	int result;

	if (size > ivc->frame_size)
		return -E2BIG;

	result = ivc_check_write(ivc);
	if (result)
		return result;

	p = ivc_frame_pointer(ivc, ivc->tx_channel, ivc->w_pos);

	memcpy(p, buf, size);
	memset((void *)((uintptr_t)p + size), 0, ivc->frame_size - size);
	ivc_flush_frame(ivc, ivc->tx_channel, ivc->w_pos, 0, size);

	/*
	 * Ensure that updated data is visible before the w_pos counter
	 * indicates that it is ready.
	 */
	ivc_wmb();

	ivc_advance_tx(ivc);
	ivc_cache_flush((void *)&ivc->tx_channel->w_count,
			sizeof(ivc->tx_channel->w_count));

	/*
	 * Ensure our write to w_pos occurs before our read from r_pos.
	 */
	ivc_mb();

	/*
	 * Notify only upon transition from empty to non-empty.
	 * The available count can only asynchronously decrease, so the
	 * worst possible side-effect will be a spurious notification.
	 */
	ivc_cache_invalidate((void *)&ivc->tx_channel->r_count,
			sizeof(ivc->tx_channel->r_count));
	if (ivc_channel_avail_count(ivc, ivc->tx_channel) == 1)
		ivc->notify(ivc);

	return (int)size;
}

/* poke in the next tx buffer at offset off, the count bytes */
int tegra_ivc_write_poke(struct ivc *ivc, const void *buf, size_t off,
		size_t count)
{
	void *dest;
	int result;

	if (off > ivc->frame_size || off + count > ivc->frame_size)
		return -E2BIG;

	result = ivc_check_write(ivc);
	if (result)
		return result;

	dest = ivc_frame_pointer(ivc, ivc->tx_channel, ivc->w_pos);
	memcpy((void *)((uintptr_t)dest + off), buf, count);

	return (int)count;
}

/* directly poke at the next frame to be tx'ed */
void *tegra_ivc_write_get_next_frame(struct ivc *ivc)
{
	if (ivc_check_write(ivc))
		return NULL;

	return ivc_frame_pointer(ivc, ivc->tx_channel, ivc->w_pos);
}

/* advance the tx buffer */
int tegra_ivc_write_advance(struct ivc *ivc)
{
	int result = ivc_check_write(ivc);
	if (result)
		return result;

	ivc_flush_frame(ivc, ivc->tx_channel, ivc->w_pos, 0, ivc->frame_size);

	/*
	 * Order any possible stores to the frame before update of w_pos.
	 */
	ivc_wmb();

	ivc_advance_tx(ivc);
	ivc_cache_flush((void *)&ivc->tx_channel->w_count,
			sizeof(ivc->tx_channel->w_count));

	/*
	 * Ensure our write to w_pos occurs before our read from r_pos.
	 */
	ivc_mb();

	/*
	 * Notify only upon transition from empty to non-empty.
	 * The available count can only asynchronously decrease, so the
	 * worst possible side-effect will be a spurious notification.
	 */
	ivc_cache_invalidate((void *)&ivc->tx_channel->r_count,
			sizeof(ivc->tx_channel->r_count));
	if (ivc_channel_avail_count(ivc, ivc->tx_channel) == 1)
		ivc->notify(ivc);

	return 0;
}

void tegra_ivc_channel_reset(struct ivc *ivc)
{
	ivc->tx_channel->state = ivc_state_sync;
	ivc_cache_flush((void *)&ivc->tx_channel->w_count,
			sizeof(ivc->tx_channel->w_count));
	ivc->notify(ivc);
}

/*
 * ===============================================================
 *  IVC State Transition Table - see tegra_ivc_channel_notified()
 * ===============================================================
 *
 *	local	remote	action
 *	-----	------	-----------------------------------
 *	SYNC	EST	<none>
 *	SYNC	ACK	reset counters; move to EST; notify
 *	SYNC	SYNC	reset counters; move to ACK; notify
 *	ACK	EST	move to EST; notify
 *	ACK	ACK	move to EST; notify
 *	ACK	SYNC	reset counters; move to ACK; notify
 *	EST	EST	<none>
 *	EST	ACK	<none>
 *	EST	SYNC	reset counters; move to ACK; notify
 *
 * ===============================================================
 */

int tegra_ivc_channel_notified(struct ivc *ivc)
{
	uint32_t peer_state;

	/* Copy the receiver's state out of shared memory. */
	ivc_cache_invalidate((void *)&ivc->rx_channel->state,
			sizeof(ivc->rx_channel->state));
	peer_state = ivc->rx_channel->state;

	if (peer_state == ivc_state_sync) {
		/*
		 * Order observation of ivc_state_sync before stores clearing
		 * tx_channel.
		 */
		ivc_rmb();

		/*
		 * Reset tx_channel counters. The remote end is in the SYNC
		 * state and won't make progress until we change our state,
		 * so the counters are not in use at this time.
		 */
		ivc->tx_channel->w_count = 0;
		ivc->rx_channel->r_count = 0;

		ivc->w_pos = 0;
		ivc->r_pos = 0;

		ivc_cache_flush((void *)&ivc->rx_channel->r_count,
				sizeof(ivc->rx_channel->r_count));

		/*
		 * Ensure that counters appear cleared before new state can be
		 * observed.
		 */
		ivc_wmb();

		/*
		 * Move to ACK state. We have just cleared our counters, so it
		 * is now safe for the remote end to start using these values.
		 */
		ivc->tx_channel->state = ivc_state_ack;
		ivc_cache_flush((void *)&ivc->tx_channel->w_count,
				sizeof(ivc->tx_channel->w_count));

		/*
		 * Notify remote end to observe state transition.
		 */
		ivc->notify(ivc);

	} else if (ivc->tx_channel->state == ivc_state_sync &&
			peer_state == ivc_state_ack) {
		/*
		 * Order observation of ivc_state_sync before stores clearing
		 * tx_channel.
		 */
		ivc_rmb();

		/*
		 * Reset tx_channel counters. The remote end is in the ACK
		 * state and won't make progress until we change our state,
		 * so the counters are not in use at this time.
		 */
		ivc->tx_channel->w_count = 0;
		ivc->rx_channel->r_count = 0;

		ivc->w_pos = 0;
		ivc->r_pos = 0;

		ivc_cache_flush((void *)&ivc->rx_channel->r_count,
				sizeof(ivc->rx_channel->r_count));

		/*
		 * Ensure that counters appear cleared before new state can be
		 * observed.
		 */
		ivc_wmb();

		/*
		 * Move to ESTABLISHED state. We know that the remote end has
		 * already cleared its counters, so it is safe to start
		 * writing/reading on this channel.
		 */
		ivc->tx_channel->state = ivc_state_established;
		ivc_cache_flush((void *)&ivc->tx_channel->w_count,
				sizeof(ivc->tx_channel->w_count));

		/*
		 * Notify remote end to observe state transition.
		 */
		ivc->notify(ivc);

	} else if (ivc->tx_channel->state == ivc_state_ack) {
		/*
		 * At this point, we have observed the peer to be in either
		 * the ACK or ESTABLISHED state. Next, order observation of
		 * peer state before storing to tx_channel.
		 */
		ivc_rmb();

		/*
		 * Move to ESTABLISHED state. We know that we have previously
		 * cleared our counters, and we know that the remote end has
		 * cleared its counters, so it is safe to start writing/reading
		 * on this channel.
		 */
		ivc->tx_channel->state = ivc_state_established;
		ivc_cache_flush((void *)&ivc->tx_channel->w_count,
				sizeof(ivc->tx_channel->w_count));

		/*
		 * Notify remote end to observe state transition.
		 */
		ivc->notify(ivc);

	} else {
		/*
		 * There is no need to handle any further action. Either the
		 * channel is already fully established, or we are waiting for
		 * the remote end to catch up with our current state. Refer
		 * to the diagram in "IVC State Transition Table" above.
		 */
	}

	return ivc->tx_channel->state == ivc_state_established ? 0 : -EAGAIN;
}

/*
 * Temporary routine for re-synchronizing the channel across a reboot.
 */
int tegra_ivc_channel_sync(struct ivc *ivc)
{
	if ((ivc == NULL) || (ivc->nframes == 0)) {
		return -EINVAL;
	} else {
		ivc->w_pos = ivc->tx_channel->w_count % ivc->nframes;
		ivc->r_pos = ivc->rx_channel->r_count % ivc->nframes;
	}
	return 0;
}

size_t tegra_ivc_align(size_t size)
{
	return (size + (IVC_ALIGN - 1)) & ~(IVC_ALIGN - 1);
}

unsigned tegra_ivc_total_queue_size(unsigned queue_size)
{
	if (queue_size & (IVC_ALIGN - 1)) {
		ivc_printf("queue_size (%u) must be %u-byte aligned\n",
				queue_size, IVC_ALIGN);
		return 0;
	}
	return queue_size + sizeof(struct ivc_channel_header);
}

static int check_ivc_params(uintptr_t queue_base1, uintptr_t queue_base2,
		unsigned nframes, unsigned frame_size)
{
	IVC_ASSERT(!(offsetof(struct ivc_channel_header, w_count)
				& (IVC_ALIGN - 1)));
	IVC_ASSERT(!(offsetof(struct ivc_channel_header, r_count)
				& (IVC_ALIGN - 1)));
	IVC_ASSERT(!(sizeof(struct ivc_channel_header) & (IVC_ALIGN - 1)));

	if ((uint64_t)nframes * (uint64_t)frame_size >= 0x100000000ull) {
		ivc_printf("nframes * frame_size overflows\n");
		return -EINVAL;
	}

	/*
	 * The headers must at least be aligned enough for counters
	 * to be accessed atomically.
	 */
	if (queue_base1 & (IVC_ALIGN - 1)) {
		ivc_printf("ivc channel start not aligned: %lx\n", queue_base1);
		return -EINVAL;
	}
	if (queue_base2 & (IVC_ALIGN - 1)) {
		ivc_printf("ivc channel start not aligned: %lx\n", queue_base2);
		return -EINVAL;
	}

	if (frame_size & (IVC_ALIGN - 1)) {
		ivc_printf("frame size not adequately aligned: %u\n",
				frame_size);
		return -EINVAL;
	}

	if (queue_base1 < queue_base2) {
		if (queue_base1 + frame_size * nframes > queue_base2) {
			ivc_printf("queue regions overlap: %lx + %x, %x\n",
					queue_base1, frame_size,
					frame_size * nframes);
			return -EINVAL;
		}
	} else {
		if (queue_base2 + frame_size * nframes > queue_base1) {
			ivc_printf("queue regions overlap: %lx + %x, %x\n",
					queue_base2, frame_size,
					frame_size * nframes);
			return -EINVAL;
		}
	}

	return 0;
}

int tegra_ivc_init(struct ivc *ivc, uintptr_t rx_base, uintptr_t tx_base,
		unsigned nframes, unsigned frame_size,
		void (*notify)(struct ivc *))
{
	int result;

	result = check_ivc_params(rx_base, tx_base, nframes, frame_size);
	if (result) {
		return result;
	}

	IVC_ASSERT(ivc);
	IVC_ASSERT(notify);

	/*
	 * All sizes that can be returned by communication functions should
	 * fit in a 32-bit integer.
	 */
	if (frame_size > (1u << 31))
		return -E2BIG;

	ivc->rx_channel = (struct ivc_channel_header *)rx_base;
	ivc->tx_channel = (struct ivc_channel_header *)tx_base;
	ivc->notify = notify;
	ivc->frame_size = frame_size;
	ivc->nframes = nframes;
	ivc->w_pos = 0;
	ivc->r_pos = 0;

	return 0;
}
