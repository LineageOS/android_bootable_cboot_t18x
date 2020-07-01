/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <tegrabl_bpmp_fw_interface.h>
#include <tegra-ivc-instance.h>
#include <tegra-ivc.h>
#include <tegrabl_timer.h>
#include <tegrabl_debug.h>
#include <address_map_new.h>
#include <string.h>
#include <arhsp_dbell.h>

#define KB (1024LU)
#define MB (1024*KB)
#define GB (1024*MB)

#define NV_CHECK_PRINT_ERR(expr, ...)               \
	do {                                            \
		e = (expr);                                 \
		if (e != TEGRABL_NO_ERROR) {                \
			tegrabl_printf(__VA_ARGS__);            \
			tegrabl_printf("Error Code 0x%x\n", e); \
			goto fail;                              \
		}                                           \
	} while (0)

#define NV_CHECK_ERROR_CLEANUP(expr)    \
	do {                                \
		e = (expr);                     \
		if (e != TEGRABL_NO_ERROR)      \
			goto fail;                  \
	} while (0)

/** NV_DRF_BIT - look for nth bit in register value.
 *
 *  @param x bit position in a register
 */
#define NV_DRF_BIT(x) (1 << (x))

/**
 * Defines communication channels for CCPlex <--> BPMP
 */
#define T186_CH0_CPU_0_TO_BPMP          0
#define T186_NO_OF_CHANNELS             1

/**
 * IVC specific defines
 */
#define MSG_SZ                          128
#define MSG_DATA_SZ                     120

#define IPC_CPU_MASTER_PHYS_BASE        (0x3004E000)
#define IPC_CPU_MASTER_VIRT_BASE        (IPC_CPU_MASTER_PHYS_BASE)
#define IPC_CPU_MASTER_SIZE             (4 * KB)

#define IPC_CPU_SLAVE_PHYS_BASE         (0x3004F000)
#define IPC_CPU_SLAVE_VIRT_BASE         (IPC_CPU_SLAVE_PHYS_BASE)
#define IPC_CPU_SLAVE_SIZE              (4 * KB)

/**
 * Flags used in IPC req
 */
#define FLAG_DO_ACK                     (1 << 0)
#define FLAG_RING_DOORBELL              (1 << 1)

#define HSP_DB_READ(reg)        NV_READ32(NV_ADDRESS_MAP_TOP0_HSP_DB_0_BASE \
											+ reg)
#define HSP_DB_WRITE(reg, val)  NV_WRITE32((NV_ADDRESS_MAP_TOP0_HSP_DB_0_BASE \
											+ (reg)), val)

/* Bit 17 is designated for CCPlex in non-secure world */
#define HSP_MASTER_CCPLEX      17
/* Bit 19 is designated for BPMP in non-secure world */
#define HSP_MASTER_BPMP        19

/* Timeout to receive response from BPMP is 1 sec */
#define TIMEOUT_RESPONSE_FROM_BPMP  1000000 /* in microseconds */

/**
 * Holds frame data for an IPC request
 */
struct frame_data {
	/* Identification as to what kind of data is being transmitted */
	uint32_t mrq;

	/* Flags for slave as to how to respond back */
	uint32_t flags;

	/* Actual data being sent */
	uint8_t data[MSG_DATA_SZ];
};

struct ccplex_bpmp_ipc {
	/* connect to BPMP */
	tegrabl_error_t (*connect)(void);

	/* Query next frame from output channel */
	struct frame_data *(*get_next_out_frame)(uint32_t channel);

	/* Signal slave when output data is ready to transfer */
	void (*signal_slave)(uint32_t channel);

	/* Wait for ACK from slave */
	tegrabl_error_t (*wait_for_slave_ack)(uint32_t channel);

	/* Free up the master */
	tegrabl_error_t (*free_master)(uint32_t channel);

	/* Query current input frame */
	struct frame_data *(*get_cur_in_frame)(uint32_t channel);
};

/**
 * Holds IVC channel data
 */
struct ccplex_bpmp_channel_data {
	/* Buffer for incoming data */
	struct frame_data *ib;

	/* Buffer for outgoing data */
	struct frame_data *ob;
};

/* Global variables */
static struct ccplex_bpmp_channel_data s_channels[T186_NO_OF_CHANNELS];
static struct ivc ivc_ccplex_bpmp_channels[T186_NO_OF_CHANNELS];
static struct ccplex_bpmp_ipc *s_ipc_callbacks;
static uint32_t channel_id;

/*
 *=============================================================================
 *      IVC wrappers for CCPLEX <-> BPMP communication.
 *=============================================================================
 */

/*
 * Get the next frame where data can be written.
 */
static struct frame_data *tegrabl_ccplex_bpmp_getnext_outframe(uint32_t channel)
{
	s_channels[channel].ob =
						(struct frame_data *)tegra_ivc_write_get_next_frame(
										ivc_ccplex_bpmp_channels + channel);

	if (s_channels[channel].ob == NULL)
		pr_error("%s: Error in getting next frame, exiting\n", __func__);

	return s_channels[channel].ob;
}

static void tegrabl_ccpelx_bpmp_signal_slave(uint32_t channel)
{
	tegra_ivc_write_advance(ivc_ccplex_bpmp_channels + channel);
}

static tegrabl_error_t tegrabl_ccplex_bpmp_free_master(uint32_t channel)
{
	uint32_t e;

	e = tegra_ivc_read_advance(ivc_ccplex_bpmp_channels + channel);

	if (e) {
		pr_error("%s: Error in tegrabl_ccplex_bpmp_free_master()\n",
				 __func__);
		return TEGRABL_ERR_INVALID_STATE;
	}

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t tegrabl_ccplex_bpmp_slave_acked(uint32_t channel)
{
	s_channels[channel].ib =
					(struct frame_data *)tegra_ivc_read_get_next_frame(
									ivc_ccplex_bpmp_channels + channel);

	if (s_channels[channel].ib == NULL)
		return TEGRABL_ERR_INVALID_STATE;

	return TEGRABL_NO_ERROR;
}

static struct frame_data *tegrabl_get_cur_in_frame(uint32_t channel)
{
	return s_channels[channel].ib;
}

/*
 * Enables BPMP to ring CCPlex doorbell
 */
static void tegrabl_enable_bpmp_to_ring_ccplex_db(void)
{
	uint32_t reg;

	reg = HSP_DB_READ(HSP_DBELL_1_ENABLE_0);
	reg |= NV_DRF_BIT(HSP_MASTER_BPMP);

	HSP_DB_WRITE(HSP_DBELL_1_ENABLE_0, reg);
}

/*
 * CCPlex rings the BPMP doorbell
 */
static void tegrabl_ring_bpmp_doorbell(void)
{
	/*
	 * Any writes to this register has the same effect, uses master ID of
	 * the write transaction and set corresponding flag.
	 */
	HSP_DB_WRITE(HSP_DBELL_3_TRIGGER_0, 1);
}

/*
 * Returns 1 if CCPLex can ring BPMP doorbell, otherwise 0.
 * This also signals that BPMP is up and ready.
 */
static bool tegrabl_can_ccplex_ring_bpmpdb(void)
{
	uint32_t reg;

	reg = HSP_DB_READ(HSP_DBELL_3_ENABLE_0);
	if (reg & NV_DRF_BIT(HSP_MASTER_CCPLEX))
		return true;

	return false;
}

static tegrabl_error_t tegrabl_ccplex_bpmp_wait_for_slave_ack(uint32_t channel)
{
	uint32_t timeout = TIMEOUT_RESPONSE_FROM_BPMP;

	do {
		if (tegrabl_ccplex_bpmp_slave_acked(channel) == TEGRABL_NO_ERROR) {
			pr_debug("%s: Got ack from slave\n", __func__);
			return TEGRABL_NO_ERROR;
		}

		/*TODO - Replace with timer delay (Bug 200188318)*/
		tegrabl_udelay(50);

		timeout--;
	} while (timeout);

	pr_error("%s: Didn't get response from BPMP, exiting\n", __func__);
	return TEGRABL_ERR_TIMEOUT;
}

/*
 * Notification to ivc layer? empty as of now
 */
static void tegrabl_ivc_notify(struct ivc *ivc)
{
	TEGRABL_UNUSED(ivc);

	tegrabl_ring_bpmp_doorbell();
}

/*
 * Initializes the BPMP<--->CCPlex connection.
 */
static tegrabl_error_t tegrabl_connect_bpmp(void)
{
	uint32_t i;
	uint32_t error = 0;
	uint32_t msg_size;
	uint32_t frame_size;
	uint32_t rx_base;
	uint32_t tx_base;
	uint32_t timeout;
	tegrabl_error_t e;

	tegrabl_enable_bpmp_to_ring_ccplex_db();

	do {
		timeout = TIMEOUT_RESPONSE_FROM_BPMP;
		do {
			/* Check if BPMP FW is ready, else wait */
			if (tegrabl_can_ccplex_ring_bpmpdb())
				break;

		tegrabl_udelay(1); /* bpmp turn-around time */
		timeout--;
		} while (timeout);

		if (!timeout)
			pr_debug("%s: Waiting for BPMP FW readiness..\n", __func__);
		else
			break;
	} while (1);

	pr_info("%s: BPMP handshake completed\n", __func__);

	msg_size = tegra_ivc_align(MSG_SZ);
	frame_size = tegra_ivc_total_queue_size(msg_size);

	if ((frame_size * T186_NO_OF_CHANNELS) > IPC_CPU_MASTER_SIZE)
		NV_CHECK_PRINT_ERR
					(TEGRABL_ERR_NO_ACCESS,
					 "%s: carvout size is not sufficient for %u channels\n",
					 __func__, T186_NO_OF_CHANNELS);

	for (i = 0; i < T186_NO_OF_CHANNELS; i++) {
		rx_base = (uint32_t)(IPC_CPU_SLAVE_PHYS_BASE + (frame_size * i));
		tx_base = (uint32_t)(IPC_CPU_MASTER_PHYS_BASE + (frame_size * i));
		error = tegra_ivc_init(ivc_ccplex_bpmp_channels + i, rx_base, tx_base,
							   1, msg_size, tegrabl_ivc_notify);

		if (error)
			NV_CHECK_PRINT_ERR
			(TEGRABL_ERR_CHANNEL_BUSY,
			 "%s: Failed in tegra_ivc_init(), channel id %d, error code%d\n",
			 __func__, i, error);

		tegra_ivc_channel_reset(ivc_ccplex_bpmp_channels + i);
		while (tegra_ivc_channel_notified(ivc_ccplex_bpmp_channels + i))
			;
	}

	pr_info("%s: All communication channels initialized\n", __func__);
	return TEGRABL_NO_ERROR;

fail:
	return e;
}

static struct ccplex_bpmp_ipc ccplex_bpmp_ipc = {
	.connect = tegrabl_connect_bpmp,
	.get_next_out_frame = tegrabl_ccplex_bpmp_getnext_outframe,
	.signal_slave = tegrabl_ccpelx_bpmp_signal_slave,
	.wait_for_slave_ack = tegrabl_ccplex_bpmp_wait_for_slave_ack,
	.free_master = tegrabl_ccplex_bpmp_free_master,
	.get_cur_in_frame = tegrabl_get_cur_in_frame,
};

static void tegrabl_ipc_callbacks(struct ccplex_bpmp_ipc **s_ipc_callbacks)
{
	TEGRABL_ASSERT(s_ipc_callbacks);
	*s_ipc_callbacks = &ccplex_bpmp_ipc;
}

static tegrabl_error_t tegrabl_do_sanity_check(void)
{
	tegrabl_error_t e = TEGRABL_ERR_BAD_PARAMETER;
	channel_id = T186_CH0_CPU_0_TO_BPMP;

	if (
			(!s_ipc_callbacks->connect) ||
			(!s_ipc_callbacks->get_next_out_frame) ||
			(!s_ipc_callbacks->signal_slave) ||
			(!s_ipc_callbacks->wait_for_slave_ack) ||
			(!s_ipc_callbacks->get_cur_in_frame) ||
			(!s_ipc_callbacks->free_master)
	   )
		goto fail;

	return TEGRABL_NO_ERROR;

fail:
	pr_error("%s: One or more parameters are set incorrectly\n", __func__);
	return e;
}

/*
 * Initializes the BPMP<--->CCPlex communication path.
 */
tegrabl_error_t tegrabl_ipc_init(void)
{
	tegrabl_ipc_callbacks(&s_ipc_callbacks);
	s_ipc_callbacks->connect();
	return TEGRABL_NO_ERROR;
}

/*
 * Atomic send/receive API, which means it waits until slave acks
 */
tegrabl_error_t tegrabl_ccplex_bpmp_xfer(
		void *p_out,
		void *p_in,
		uint32_t size_out,
		uint32_t size_in,
		uint32_t mrq)
{
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	struct frame_data *p;

	if (p_out == NULL) {
		pr_error("%s: out buffer is null, exiting\n", __func__);
		return -1;
	}

	NV_CHECK_ERROR_CLEANUP(tegrabl_do_sanity_check());

	p = s_ipc_callbacks->get_next_out_frame(channel_id);
	if (!p)
		NV_CHECK_PRINT_ERR(TEGRABL_ERR_INVALID_STATE,
						   "Failed to get next output frame!\n");

	p->mrq = mrq;
	p->flags = FLAG_DO_ACK;
	memcpy(p->data, p_out, size_out);

	/* signal the slave */
	s_ipc_callbacks->signal_slave(channel_id);

	/* wait for slave to ack */
	NV_CHECK_ERROR_CLEANUP(
						   s_ipc_callbacks->wait_for_slave_ack(channel_id));

	/* retrieve the frame */
	p = s_ipc_callbacks->get_cur_in_frame(channel_id);
	if (!p)
		NV_CHECK_PRINT_ERR(TEGRABL_ERR_INVALID_STATE,
						   "Failed to query current input frame\n");

	if ((size_in) && (p_in != NULL))
		memcpy(p_in, p->data, size_in);
	NV_CHECK_ERROR_CLEANUP(s_ipc_callbacks->free_master(channel_id));

	return TEGRABL_NO_ERROR;

fail:
	if (e)
		pr_error("%s: failed to send/receive, err:%x\n", __func__, e);

	return TEGRABL_ERR_INVALID;
}
