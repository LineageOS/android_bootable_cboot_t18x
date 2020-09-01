/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef TEGRABL_VIC_H
#define TEGRABL_VIC_H

#ifdef CB_VIC_DEBUG
#define CB_VIC_DBG(fmt, args...)	pr_debug(fmt, ##args)
#else
#define CB_VIC_DBG(fmt, args...)
#endif

/* Specifies VIC modes */
enum {
	/*
	 * Used for starting the VIC transfer the Used for VIC transfer. Cache
	 * operations at Src/Dest as required need to be taken care outside this
	 * Src, Dest, Size of transfer shall be passed via VicTransferConfig
	*/
	CB_VIC_TRANSFER = 0,
	/* Used for  checking the VIC transfer completion */
	CB_VIC_WAIT_FOR_TRANSFER_COMPLETE,
};

/* Specifies the type of transfer */
enum {
	/* Asynchronous VIC scrub, start transfer and check for completion later */
	CB_VIC_SCRUB_ASYNC = 0,
	/* Synchronous VIC scrub, start transfer and wait for completion */
	CB_VIC_SCRUB_SYNC,
};


/**
 * Defines the VIC transfer Context.
 */
struct vic_transfer_config {
	/* Specifies the size of transfer */
	uint32_t size;
	/* Specifies the Physical address of source memory */
	uint64_t src_addr_phy;
	/* Specifies the Physical address of Dest. memory */
	uint64_t dest_addr_phy;
};


#define SIZE_1M									(1 * 1024 * 1024)

/* Settings for Size >= 1MB of scrub size */
#define VIC_ADDR_ALIGN_MASK1					0xFFFFF
#define VIC_SIZE_ALIGN_MASK1					VIC_ADDR_ALIGN_MASK1

/* Settings for Size < 1MB of scrub size */
#define VIC_ADDR_ALIGN_MASK2					0x3FF
#define VIC_SIZE_ALIGN_MASK2					VIC_ADDR_ALIGN_MASK2

#define VIC_SRUB_SIZE_MAX						0x40000000
#define VIC_SRUB_SIZE_MIN						0x400

#define VIC_POLL_DELAY_COUNT					3000

#define MAX_VIC_CONTROLLERS						1

#define VIC_CLK_FREQUENCY_VAL					800000

/* Settings for Size >= 1MB of scrub size */
#define VIC_SCRUB_HEIGHT1						0x4000
#define VIC_BLOCK_HEIGHT1						4

/* Settings for Size < 1MB of scrub size */
#define VIC_SCRUB_HEIGHT2						0x40
#define VIC_BLOCK_HEIGHT2						0

/* VIC registers are in different format which calls for different DRF macros */
#define VIC_DRF_SHIFT(drf)						((0 ? drf) % 32)
#define VIC_DRF_MASK(drf)		(0xFFFFFFFF >> (31 - ((1 ? drf) % 32) + \
									((0 ? drf) % 32)))
#define VIC_DRF_DEF(d, r, f, c)	((NV ## d ## r ## f ## c) << \
									VIC_DRF_SHIFT(NV ## d ## r ## f))
#define VIC_DRF_NUM(d, r, f, n)	(((n) & VIC_DRF_MASK(NV ## d ## r ## f)) << \
									VIC_DRF_SHIFT(NV ## d ## r ## f))
#define VIC_DRF_VAL(d, r, f, v)	(((v) >> VIC_DRF_SHIFT(NV ## d ## r ## f)) & \
									VIC_DRF_MASK(NV ## d ## r ## f))

#define SCRUB_BLOCK_SIZE						(4 * SIZE_1M)
/* Values as refered from HW BLIT test */
#define CB_CVIC_BL_CONFIG						0x1F05
#define CB_CVIC_FC_COMPOSE						0x1

/* When ever T_A8R8G8B8 is selected as PIXEL_FORMAT, Bytes per pixel is 4 */
#define CB_VIC_BYTES_PER_PIXEL					4

#ifndef NV_ADDRESS_MAP_VIC_BASE
#define NV_ADDRESS_MAP_VIC_BASE					0x15340000
#endif

/**
 * VIC Scrub Function
 *
 * Based on the cmd value, either wait for the previous copy to finish,
 * or copy the data passed through p_buf
 *
*/
tegrabl_error_t cb_vic_scrub(uint32_t instance, uint32_t cmd, void *p_buf);

/**
 * VIC (Video Image Compositor) initialization
 *
 * Enables the clocks required for VIC FC
 * Boot the FC by loading the firmware
 *
 */
tegrabl_error_t cb_vic_init(void);

/* Disable the clocks for the VIC FC */
tegrabl_error_t cb_vic_exit(void);

#endif
