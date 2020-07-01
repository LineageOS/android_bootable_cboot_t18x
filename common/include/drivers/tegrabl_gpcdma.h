/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_GPCDMA_H
#define INCLUDED_TEGRABL_GPCDMA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Defines DMA engines available
 */
enum tegrabl_dmatype {
	DMA_GPC = 0,
	DMA_BPMP,
	DMA_SPE,
	DMA_MAX_NUM
};

/**
 * @brief Defines DMA transfer directions
 */
enum tegrabl_dmatransferdir {
	DMA_IO_TO_MEM = 0,
	DMA_MEM_TO_IO = 2,
	DMA_MEM_TO_MEM = 4,
	DMA_PATTERN_FILL = 6,
};

/**
 * @brief Defines IO bus width
 */
enum tegrabl_buswidth {
	BUS_WIDTH_8 = 0,
	BUS_WIDTH_16 = 1,
	BUS_WIDTH_32 = 2,
};

/**
 * @brief Defines IO modules that work with DMA
 */
enum tegrabl_dmaio {
	/* BPMP-DMA */
	DMA_IO_UART = 0,
	DMA_IO_QSPI = 1,
	DMA_IO_I2C = 2,
	/* GPC-DMA */
	GPCDMA_IO_QSPI = 5,
	DMA_IO_MAX,
};

/**
 * @brief params for a DMA transaction
 */
struct tegrabl_dma_xfer_params {
	uintptr_t src;
	uintptr_t dst;
	uint32_t size;
	uint32_t pattern;
	bool is_async_xfer;
	enum tegrabl_dmatransferdir dir;
	enum tegrabl_dmaio io;
	enum tegrabl_buswidth io_bus_width;
};

typedef void *tegrabl_gpcdma_handle_t;

/**
 * @brief Returns a opaque handle of the requested DMA type.
 * Once requested, client can use it for multiple DMA transfers.
 *
 * @param dma_type type of DMA engine (GPC / BPMP / SPE) being requested
 *
 * @return tegrabl_gpcdma_handle_t handle to requested dma type if successful
 *		   NULL if the dma type is invalid
*/
tegrabl_gpcdma_handle_t tegrabl_dma_request(enum tegrabl_dmatype dma_type);

/**
 * @brief Initiates a DMA transfer
 *
 * @param handle Handle to the dma (acquired using tegrabl_dma_request() API)
 * @param c_num Channel number
 * @param params params to be passed for DMA configuration
 *		src : source memory address
 *		dst : destination memory address
 *		size : size of the transfer (NOTE : this must be aligned to word)
 *		pattern : value to be filled in memory if it is a pattern filling op
 *		is_async_xfer : needs to be set to start a asynchronous transfer
 *			tegrabl_dma_transfer_status() API needs to be used to know status
 *		dir : direction of the transfer (M->I, I->M, M->M, PATTERN_FILL)
 *		io_bus_width : IO bus width
 *
 * @return err out if any
 */
tegrabl_error_t tegrabl_dma_transfer(tegrabl_gpcdma_handle_t handle,
			uint8_t c_num,
			struct tegrabl_dma_xfer_params *params);

/**
 * @brief Query the status of an ongoing DMA transfer
 *
 * @param handle Handle to the dma (acquired using tegrabl_dma_request() API)
 * @param c_num Channel number
 * @param params params to be passed for DMA configuration
 *		src : source memory address
 *		dst : destination memory address
 *		size : size of the transfer (NOTE : this must be aligned to word)
 *		pattern : value to be filled in memory if it is a pattern filling op
 *		is_async_xfer : needs to be set to start a asynchronous transfer
 *			tegrabl_dma_transfer_status() API needs to be used to know status
 *		dir : direction of the transfer (M->I, I->M, M->M, PATTERN_FILL)
 *		io_bus_width : IO bus width
 *
 * @return err out if any
 */
tegrabl_error_t tegrabl_dma_transfer_status(tegrabl_gpcdma_handle_t handle,
	uint8_t c_num, struct tegrabl_dma_xfer_params *params);

/**
 * @brief Aborts a DMA transfer
 *
 * @param handle Handle to the dma (acquired using tegrabl_dma_request() API)
 * @param c_num Channel number
 */
void tegrabl_dma_transfer_abort(tegrabl_gpcdma_handle_t handle, uint8_t c_num);

/*			UTILITY FUNCTIONS			*/

/* NOTE: size should be alinged to word (4bytes) */

/**
 * @brief memcpy using DMA
 *
 * @param priv specify the dma-type to be used for transfer
 * @param src pointer to source buffer
 * @param dst pointer to destination buffer
 * @param size size of the transfer (NOTE : this must be aligned to word)
 *
 * @return 0 in case of success, non-0 in case of failure
 */
int tegrabl_dma_memcpy(void *priv, void *dest, const void *src, size_t size);

/**
 * @brief memset using DMA
 *
 * @param priv specify the dma-type to be used for transfer
 * @param s pointer to source buffer
 * @param c value to set each location with
 * @param size size of the transfer (NOTE : this must be aligned to word)
 *
 * @return 0 in case of success, non-0 in case of failure
 */
int tegrabl_dma_memset(void *priv, void *s, int c, size_t size);

/**
 * @brief Registers the DMA callbacks with the clib which can invoke these
 * based on specified threshold
 *
 * @param dma_type type of dma engine used for transfer
 * @param threshold Minimum size of buffers beyond which memset and memcpy DMA
 * callbacks can be invoked
 */
void tegrabl_dma_enable_clib_callbacks(enum tegrabl_dmatype dma_type,
									   size_t threshold);

/**
 * @brief DRAM Init scrubbing using GPCDMA
 *
 * @param dest pointer to destination buffer
 * @param src pointer to source buffer
 * @param pattern fixed pattern in case of fixed pattern DMA
 * @param size size of the transfer (NOTE : this must be aligned to word)
 * @param data_dir type of dma fixed pattern or mem2mem
 *
 * @return TEGRABL_NO_ERROR in case of success, error code from tegrabl_error_t
 *		in case of failure
 */
tegrabl_error_t tegrabl_init_scrub_dma(uint64_t dest, uint64_t src,
									   uint32_t pattern, uint32_t size,
									   enum tegrabl_dmatransferdir data_dir);

/* API to test DMA */
void test_dma(void);

#endif	/* INCLUDED_TEGRABL_GPCDMA_H */
