/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_GPCDMA

#include "build_config.h"
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_timer.h>
#include <tegrabl_io.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_gpcdma.h>
#include <tegrabl_clock.h>
#include <tegrabl_dmamap.h>

#define DMA_CHANNEL_OFFSET					(0x10000)

/* DMA channel registers */
#define DMA_CH_CSR							(0x0)
#define DMA_CH_CSR_WEIGHT_SHIFT				(10)
#define DMA_CH_CSR_XFER_MODE_SHIFT			(21)
#define DMA_CH_CSR_XFER_MODE_CLEAR_MASK		(7)
#define DMA_CH_CSR_DMA_MODE_IO2MEM_FC		(1)
#define DMA_CH_CSR_DMA_MODE_MEM2IO_FC		(3)
#define DMA_CH_CSR_REQ_SEL_SHIFT			(16)
#define DMA_CH_CSR_FC_1_MMIO				(1 << 24)
#define DMA_CH_CSR_FC_4_MMIO				(3 << 24)
#define DMA_CH_CSR_IRQ_MASK_ENABLE			(1 << 15)
#define DMA_CH_CSR_RUN_ONCE					(1 << 27)
#define DMA_CH_CSR_ENABLE					(1 << 31)

#define DMA_CH_STAT							(0x4)
#define DMA_CH_STAT_BUSY					(1 << 31)

#define DMA_CH_SRC_PTR						(0xC)

#define DMA_CH_DST_PTR						(0x10)

#define DMA_CH_HI_ADR_PTR					(0x14)
#define DMA_CH_HI_ADR_PTR_SRC_SHIFT			(0)
#define DMA_CH_HI_ADR_PTR_SRC_MASK			(0xFF)
#define DMA_CH_HI_ADR_PTR_DST_SHIFT			(16)
#define DMA_CH_HI_ADR_PTR_DST_MASK			(0xFF)

#define DMA_CH_MC_SEQ						(0x18)
#define DMA_CH_MC_SEQ_REQ_CNT_SHIFT			(25)
#define DMA_CH_MC_SEQ_REQ_CNT_CLEAR			(0x3F)
#define DMA_CH_MC_SEQ_REQ_CNT_VAL			(0x10)
#define DMA_CH_MC_SEQ_BURST_SHIFT			(23)
#define DMA_CH_MC_SEQ_BURST_MASK			(0x3)
#define DMA_CH_MC_SEQ_BURST_2_WORDS			(0x0)
#define DMA_CH_MC_SEQ_BURST_16_WORDS		(0x3)
#define DMA_CH_MC_SEQ_STREAMID0_1			(0x1)

#define DMA_CH_MMIO_SEQ						(0x1C)
#define DMA_CH_MMIO_SEQ_BUS_WIDTH_SHIFT		(28)
#define DMA_CH_MMIO_SEQ_BUS_WIDTH_CLEAR		(7)
#define DMA_CH_MMIO_SEQ_MMIO_BURST_SHIFT	(23)
#define DMA_CH_MMIO_SEQ_MMIO_BURST_CLEAR	(0xF)
#define DMA_CH_MMIO_SEQ_MMIO_BURST_1_WORD	(0)
#define DMA_CH_MMIO_SEQ_MMIO_BURST_2_WORD	(1)
#define DMA_CH_MMIO_SEQ_MMIO_BURST_4_WORD	(3)
#define DMA_CH_MMIO_SEQ_MMIO_BURST_8_WORD	(7)
#define DMA_CH_MMIO_SEQ_MMIO_BURST_16_WORD	(15)

#define DMA_CH_MMIO_WCOUNT					(0x20)

#define DMA_CH_FIXED_PATTERN				(0x34)

#define MAX_TRANSFER_SIZE					(1*1024*1024*1024)	/* 1GB */

struct s_dma_plat_data {
	enum tegrabl_dmatype dma_type;
	uint8_t max_channel_num;
	uintptr_t base_addr;
};

struct s_dma_plat_data g_dma_gpc_plat = {
	.dma_type = DMA_GPC,
	.max_channel_num = 32,
	.base_addr = NV_ADDRESS_MAP_GPCDMA_BASE,
};
struct s_dma_plat_data g_dma_bpmp_plat = {
	.dma_type = DMA_BPMP,
	.max_channel_num = 4,
	.base_addr = NV_ADDRESS_MAP_BPMP_DMA_BASE,
};
struct s_dma_plat_data g_dma_spe_plat = {
	.dma_type = DMA_SPE,
	.max_channel_num = 8,
	.base_addr = NV_ADDRESS_MAP_AON_DMA_BASE,
};

/* Global strcture to maintain DMA information */
struct s_dma_privdata {
	struct s_dma_plat_data dma_plat_data;
	bool init_done;
};

static struct s_dma_privdata g_dma_data[DMA_MAX_NUM];

tegrabl_gpcdma_handle_t tegrabl_dma_request(enum tegrabl_dmatype dma_type)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	tegrabl_module_t dmamodule_id;

	if (dma_type >= DMA_MAX_NUM)
		return NULL;

	if (g_dma_data[dma_type].init_done)
		return (tegrabl_gpcdma_handle_t)(&g_dma_data[dma_type]);

	switch (dma_type) {
	case DMA_GPC:
		dmamodule_id = TEGRABL_MODULE_GPCDMA;
		g_dma_data[dma_type].dma_plat_data = g_dma_gpc_plat;
		break;
	case DMA_BPMP:
		dmamodule_id = TEGRABL_MODULE_BPMPDMA;
		g_dma_data[dma_type].dma_plat_data = g_dma_bpmp_plat;
		break;
	case DMA_SPE:
		dmamodule_id = TEGRABL_MODULE_SPEDMA;
		g_dma_data[dma_type].dma_plat_data = g_dma_spe_plat;
		break;
	default:
		return NULL;
	}

	/* assert reset of DMA engine */
	err = tegrabl_car_rst_set(dmamodule_id, 0);
	if (err != TEGRABL_NO_ERROR)
		return NULL;

	tegrabl_udelay(2);

	/* de-assert reset of DMA engine */
	err = tegrabl_car_rst_clear(dmamodule_id, 0);
	if (err != TEGRABL_NO_ERROR)
		return NULL;

	g_dma_data[dma_type].init_done = 1;
	return (tegrabl_gpcdma_handle_t)(&g_dma_data[dma_type]);
}

static void tegrabl_unmap_buffers(struct tegrabl_dma_xfer_params *params)
{
	if ((params->dir == DMA_IO_TO_MEM) ||
		(params->dir == DMA_MEM_TO_MEM) ||
		(params->dir == DMA_PATTERN_FILL)) {
		tegrabl_dma_unmap_buffer(0, 0, (void *)params->dst,
								 params->size, TEGRABL_DMA_FROM_DEVICE);
		pr_debug("dst unmapped buffer = 0x%x\n", (unsigned int)params->dst);
	}

	if ((params->dir == DMA_MEM_TO_IO) || (params->dir == DMA_MEM_TO_MEM)) {
		tegrabl_dma_unmap_buffer(0, 0, (void *)params->src,
								 params->size, TEGRABL_DMA_TO_DEVICE);
		pr_debug("src unmapped buffer = 0x%x\n", (unsigned int)params->src);
	}
}


tegrabl_error_t tegrabl_dma_transfer(tegrabl_gpcdma_handle_t handle,
	uint8_t c_num, struct tegrabl_dma_xfer_params *params)
{
	uintptr_t cb = 0;
	uint32_t val = 0;
	dma_addr_t src_dma_addr = 0;
	dma_addr_t dst_dma_addr = 0;
	struct s_dma_privdata *dma_data = (struct s_dma_privdata *)handle;

	if ((handle == NULL) || (params == NULL))
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);

	if (!dma_data->init_done)
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 0);

	if (c_num >= dma_data->dma_plat_data.max_channel_num)
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID_CHANNEL_NUM, 0);

	/* get channel base offset */
	cb = dma_data->dma_plat_data.base_addr + (DMA_CHANNEL_OFFSET * (c_num + 1));

	/* make sure channel isn't busy */
	val = NV_READ32(cb + DMA_CH_STAT);
	if (val & DMA_CH_STAT_BUSY) {
		pr_error("DMA channel %u is busy\n", c_num);
		return TEGRABL_ERROR(TEGRABL_ERR_CHANNEL_BUSY, 0);
	}

	NV_WRITE32_FENCE(cb + DMA_CH_CSR, 0x0);

	/* configure MC sequencer */
	val = NV_READ32(cb + DMA_CH_MC_SEQ);
	val &= ~(DMA_CH_MC_SEQ_REQ_CNT_CLEAR << DMA_CH_MC_SEQ_REQ_CNT_SHIFT);
	val |= (DMA_CH_MC_SEQ_REQ_CNT_VAL << DMA_CH_MC_SEQ_REQ_CNT_SHIFT);
	val &= ~(DMA_CH_MC_SEQ_BURST_MASK << DMA_CH_MC_SEQ_BURST_SHIFT);

	if ((params->dir == DMA_IO_TO_MEM) || (params->dir == DMA_MEM_TO_IO)) {
		val |= (DMA_CH_MC_SEQ_BURST_2_WORDS << DMA_CH_MC_SEQ_BURST_SHIFT);
	} else {
		val |= (DMA_CH_MC_SEQ_BURST_16_WORDS << DMA_CH_MC_SEQ_BURST_SHIFT);
	}

	NV_WRITE32_FENCE(cb + DMA_CH_MC_SEQ, val);

	/* configure MMIO sequencer if either end of DMA transaction is an MMIO */
	if ((params->dir == DMA_IO_TO_MEM) || (params->dir == DMA_MEM_TO_IO)) {
		val = NV_READ32(cb + DMA_CH_MMIO_SEQ);
		val &= ~(DMA_CH_MMIO_SEQ_BUS_WIDTH_CLEAR <<
			DMA_CH_MMIO_SEQ_BUS_WIDTH_SHIFT);
		val |= (params->io_bus_width << DMA_CH_MMIO_SEQ_BUS_WIDTH_SHIFT);
		val &= ~(DMA_CH_MMIO_SEQ_MMIO_BURST_CLEAR <<
			DMA_CH_MMIO_SEQ_MMIO_BURST_SHIFT);
		val |= (DMA_CH_MMIO_SEQ_MMIO_BURST_8_WORD <<
			DMA_CH_MMIO_SEQ_MMIO_BURST_SHIFT);
		NV_WRITE32_FENCE(cb + DMA_CH_MMIO_SEQ, val);
	}

	if (params->dir != DMA_PATTERN_FILL)
		params->pattern = 0;
	NV_WRITE32_FENCE(cb + DMA_CH_FIXED_PATTERN, params->pattern);

	if ((params->dir == DMA_IO_TO_MEM) ||
		(params->dir == DMA_MEM_TO_MEM) ||
		(params->dir == DMA_PATTERN_FILL)) {
		dst_dma_addr = tegrabl_dma_map_buffer(0, 0,
				(void *)params->dst, params->size, TEGRABL_DMA_FROM_DEVICE);
		pr_debug("dst mapped buffer = 0x%x\n", (unsigned int)params->dst);
	} else {
		dst_dma_addr = (uintptr_t)(params->dst);
	}

	if ((params->dir == DMA_MEM_TO_IO) || (params->dir == DMA_MEM_TO_MEM)) {
		src_dma_addr = tegrabl_dma_map_buffer(0, 0,
				(void *)params->src, params->size, TEGRABL_DMA_TO_DEVICE);
		pr_debug("src unmapped buffer = 0x%x\n", (unsigned int)params->src);
	} else {
		src_dma_addr = (uintptr_t)(params->src);
	}

	/* populate src and dst address registers */
	NV_WRITE32_FENCE(cb + DMA_CH_SRC_PTR, (uint32_t)src_dma_addr);
	NV_WRITE32_FENCE(cb + DMA_CH_DST_PTR, (uint32_t)dst_dma_addr);
	val = ((uint64_t)src_dma_addr >> 32) & DMA_CH_HI_ADR_PTR_SRC_MASK;
	val |= ((((uint64_t)dst_dma_addr >> 32) & DMA_CH_HI_ADR_PTR_DST_MASK) <<
		DMA_CH_HI_ADR_PTR_DST_SHIFT);
	NV_WRITE32_FENCE(cb + DMA_CH_HI_ADR_PTR, val);

	/* transfer size */
	if ((params->size > MAX_TRANSFER_SIZE) || (params->size & 0x3))
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID_XFER_SIZE, 0);
	NV_WRITE32_FENCE(cb + DMA_CH_MMIO_WCOUNT, ((params->size >> 2) - 1));

	/* populate value for CSR */
	val = DMA_CH_CSR_IRQ_MASK_ENABLE |
		  DMA_CH_CSR_RUN_ONCE |
		  (1 << DMA_CH_CSR_WEIGHT_SHIFT);

	val &= ~(DMA_CH_CSR_XFER_MODE_CLEAR_MASK << DMA_CH_CSR_XFER_MODE_SHIFT);
	if (params->dir == DMA_IO_TO_MEM)
		val |= (DMA_CH_CSR_DMA_MODE_IO2MEM_FC << DMA_CH_CSR_XFER_MODE_SHIFT);
	else if (params->dir == DMA_MEM_TO_IO)
		val |= (DMA_CH_CSR_DMA_MODE_MEM2IO_FC << DMA_CH_CSR_XFER_MODE_SHIFT);
	else
		val |= (params->dir << DMA_CH_CSR_XFER_MODE_SHIFT);


	if ((params->dir == DMA_IO_TO_MEM) || (params->dir == DMA_MEM_TO_IO)) {
		val |= (params->io << DMA_CH_CSR_REQ_SEL_SHIFT);
		val |= DMA_CH_CSR_FC_4_MMIO;
	}
	NV_WRITE32_FENCE(cb + DMA_CH_CSR, val);

	val = NV_READ32(cb + DMA_CH_CSR);
	val |= DMA_CH_CSR_ENABLE;
	NV_WRITE32_FENCE(cb + DMA_CH_CSR, val);

	if (params->is_async_xfer) {
		return TEGRABL_NO_ERROR;
	} else {
		uint32_t val = DMA_CH_STAT_BUSY;
		while (val & DMA_CH_STAT_BUSY)
			val = NV_READ32(cb + DMA_CH_STAT);
		tegrabl_unmap_buffers(params);
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_dma_transfer_status(tegrabl_gpcdma_handle_t handle,
	uint8_t c_num, struct tegrabl_dma_xfer_params *params)
{
	uintptr_t cb = 0;
	struct s_dma_privdata *dma_data;

	if (handle == NULL || !params)
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);

	dma_data = (struct s_dma_privdata *)handle;

	if (c_num >= dma_data->dma_plat_data.max_channel_num)
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID_CHANNEL_NUM, 0);

	cb = dma_data->dma_plat_data.base_addr + (DMA_CHANNEL_OFFSET * (c_num + 1));
	if (NV_READ32(cb + DMA_CH_STAT) & DMA_CH_STAT_BUSY) {
		pr_debug("DMA channel %u is busy\n", c_num);
		return TEGRABL_ERROR(TEGRABL_ERR_CHANNEL_BUSY, 1);
	} else {
		tegrabl_unmap_buffers(params);
		return TEGRABL_NO_ERROR;
	}
}

void tegrabl_dma_transfer_abort(tegrabl_gpcdma_handle_t handle,
			uint8_t c_num)
{
	uintptr_t cb = 0;
	struct s_dma_privdata *dma_data;

	if (handle == NULL)
		return;

	dma_data = (struct s_dma_privdata *)handle;

	cb = dma_data->dma_plat_data.base_addr + (DMA_CHANNEL_OFFSET * (c_num + 1));
	NV_WRITE32_FENCE(cb + DMA_CH_CSR, 0);
}

/* General pupose DMA will be used for utility APIs */
int tegrabl_dma_memcpy(void *priv, void *dest, const void *src, size_t size)
{
	tegrabl_gpcdma_handle_t handle;
	struct tegrabl_dma_xfer_params params;
	enum tegrabl_dmatype dma_type = (enum tegrabl_dmatype)priv;

	/* Check for word-aligned bufs and word-multiple length */
	if (!dest || !src || (size & 0x3) ||
		(((uintptr_t)dest) & 0x3) || (((uintptr_t)src) & 0x3))
		return -1;

	pr_debug("%s(%p,%p,%u)\n", __func__, dest, src, (uint32_t)size);

	handle = tegrabl_dma_request(dma_type);

	params.src = (uintptr_t)src;
	params.dst = (uintptr_t)dest;
	params.size = size;
	params.is_async_xfer = 0;
	params.dir = DMA_MEM_TO_MEM;

	return (tegrabl_dma_transfer(handle, 0, &params) != TEGRABL_NO_ERROR);
}

/*
 * As the below SDRAM Init scrub API is using the GPCDMA for accessing the
 * SDRAM directy, data is not cached so no need to invoke DMA-mapping apis
 */
tegrabl_error_t tegrabl_init_scrub_dma(uint64_t dest, uint64_t src,
									   uint32_t pattern, uint32_t size,
									   enum tegrabl_dmatransferdir data_dir)
{
	tegrabl_gpcdma_handle_t handle;
	struct tegrabl_dma_xfer_params params;
	uintptr_t cb = 0;
	uint32_t val = 0;
	uint32_t c_num = 0;
	dma_addr_t src_dma_addr = 0;
	dma_addr_t dst_dma_addr = 0;
	struct s_dma_privdata *dma_data;
	tegrabl_error_t result = TEGRABL_NO_ERROR;

	handle = tegrabl_dma_request(DMA_GPC);

	params.size = size;
	params.is_async_xfer = 0;
	params.pattern = pattern;
	params.dir = data_dir;

	dma_data = (struct s_dma_privdata *)handle;

	/* get channel base offset */
	cb = dma_data->dma_plat_data.base_addr + (DMA_CHANNEL_OFFSET * (c_num + 1));

	/* make sure channel isn't busy */
	val = NV_READ32(cb + DMA_CH_STAT);
	if (val & DMA_CH_STAT_BUSY) {
		pr_error("DMA channel %u is busy\n", c_num);
		result = TEGRABL_ERROR(TEGRABL_ERR_CHANNEL_BUSY, 2);
		goto fail;
	}

	val = NV_READ32(cb + DMA_CH_MC_SEQ);
	val &= ~(DMA_CH_MC_SEQ_REQ_CNT_CLEAR << DMA_CH_MC_SEQ_REQ_CNT_SHIFT);
	val |= (DMA_CH_MC_SEQ_REQ_CNT_VAL << DMA_CH_MC_SEQ_REQ_CNT_SHIFT);
	val &= ~(DMA_CH_MC_SEQ_BURST_MASK << DMA_CH_MC_SEQ_BURST_SHIFT);
	val |= (DMA_CH_MC_SEQ_BURST_16_WORDS << DMA_CH_MC_SEQ_BURST_SHIFT);
	NV_WRITE32_FENCE(cb + DMA_CH_MC_SEQ, val);

	NV_WRITE32_FENCE(cb + DMA_CH_FIXED_PATTERN, params.pattern);

	src_dma_addr = src;
	dst_dma_addr = dest;

	pr_debug("dst_dma_addr = 0x%016"PRIx64"\n", dst_dma_addr);
	pr_debug("src_dma_addr = 0x%016"PRIx64"\n", src_dma_addr);
	pr_debug("size = 0x%08x\n", params.size);

	/* populate src and dst address registers */
	NV_WRITE32_FENCE(cb + DMA_CH_SRC_PTR, (uint32_t)src_dma_addr);
	NV_WRITE32_FENCE(cb + DMA_CH_DST_PTR, (uint32_t)dst_dma_addr);
	val = ((uint64_t)src_dma_addr >> 32) & DMA_CH_HI_ADR_PTR_SRC_MASK;
	val |= ((((uint64_t)dst_dma_addr >> 32) & DMA_CH_HI_ADR_PTR_DST_MASK) <<
		DMA_CH_HI_ADR_PTR_DST_SHIFT);
	NV_WRITE32_FENCE(cb + DMA_CH_HI_ADR_PTR, val);

	/* transfer size */
	if ((params.size > MAX_TRANSFER_SIZE) || (params.size & 0x3)) {
		pr_error("Invalid Size\n");
		result = TEGRABL_ERROR(TEGRABL_ERR_INVALID_XFER_SIZE, 0);
		goto fail;
	}
	NV_WRITE32_FENCE(cb + DMA_CH_MMIO_WCOUNT, ((params.size >> 2) - 1));

	/* populate value for CSR */
	if ((params.dir != DMA_PATTERN_FILL) &&
			(params.dir != DMA_MEM_TO_MEM)) {
		pr_error("Invalid Transfer type\n");
		result = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 0);
		goto fail;
	}
	val = NV_READ32(cb + DMA_CH_CSR);
	val &= ~(DMA_CH_CSR_XFER_MODE_CLEAR_MASK << DMA_CH_CSR_XFER_MODE_SHIFT);
	val |= (params.dir << DMA_CH_CSR_XFER_MODE_SHIFT);
	NV_WRITE32_FENCE(cb + DMA_CH_CSR, val);

	val = NV_READ32(cb + DMA_CH_CSR);
	val |= DMA_CH_CSR_ENABLE;
	NV_WRITE32_FENCE(cb + DMA_CH_CSR, val);

	pr_debug("Waiting for DMA ......%s\n", __func__);

	while (NV_READ32(cb + DMA_CH_STAT) & DMA_CH_STAT_BUSY)
			;

	pr_debug("DMA Complete......%s\n\n", __func__);

fail:
	return result;
}

int tegrabl_dma_memset(void *priv, void *s, int c, size_t size)
{
	tegrabl_gpcdma_handle_t handle;
	struct tegrabl_dma_xfer_params params;
	enum tegrabl_dmatype dma_type = (enum tegrabl_dmatype)priv;

	/* Check for word-aligned buf and word-multiple size */
	if (!s || (size & 0x3) || (((uintptr_t)s) & 0x3))
		return -1;

	handle = tegrabl_dma_request(dma_type);

	pr_debug("%s(%p,%d,%u)\n", __func__, s, c, (uint32_t)size);

	params.src = 0;
	params.dst = (uintptr_t)s;
	params.size = size;
	params.is_async_xfer = 0;
	c &= 0xff;
	c |= c << 8;
	c |= c << 16;
	params.pattern = c;
	params.dir = DMA_PATTERN_FILL;

	return (tegrabl_dma_transfer(handle, 0, &params) != TEGRABL_NO_ERROR);
}

static struct tegrabl_clib_dma clib_dma;

void tegrabl_dma_enable_clib_callbacks(enum tegrabl_dmatype dma_type,
									   size_t threshold)
{
	clib_dma.memcpy_priv = (void *)dma_type;
	clib_dma.memset_priv = (void *)dma_type;
	clib_dma.memcpy_threshold = threshold;
	clib_dma.memset_threshold = threshold;
	clib_dma.memcpy_callback = tegrabl_dma_memcpy;
	clib_dma.memset_callback = tegrabl_dma_memset;

	tegrabl_clib_dma_register(&clib_dma);
}

void test_dma(void)
{
	unsigned long src;
	unsigned long dst;
	uint32_t size = 0x1000;

	pr_info("=== DMA testing starts ===\n");

	pr_info("ExtMEM to ExtMEM .... ");
	/* FIXME : Use allocator to get buffers */
	src = 0x80000000;
	dst = 0x80002000;
	memset((void *)src, 0xA5, size);
	memset((void *)dst, 0x0, size);
	tegrabl_dma_memcpy((void *)DMA_GPC, (void *)dst, (void *)src, size);
	if (memcmp((void *)src, (void *)dst, size))
		pr_info("FAIL\n");
	else
		pr_info("PASS\n");

	pr_info("SysRAM to ExtMEM .... ");
	src = 0x40000000;
	dst = 0x80002000;
	memset((void *)src, 0xA5, size);
	memset((void *)dst, 0x0, size);
	/* FIXME : Use allocator to get buffers */
	tegrabl_dma_memcpy((void *)DMA_GPC, (void *)dst, (void *)src, size);
	if (memcmp((void *)src, (void *)dst, size))
		pr_info("FAIL\n");
	else
		pr_info("PASS\n");

	pr_info("Pattern filling .... ");
	src = 0x80000000;
	dst = 0x80002000;
	memset((void *)src, 0xA5, size);
	memset((void *)dst, 0x0, size);
	tegrabl_dma_memset((void *)DMA_GPC, (void *)dst, 0xA5, size);
	if (memcmp((void *)src, (void *)dst, size))
		pr_info("FAIL\n");
	else
		pr_info("PASS\n");

	pr_info("=== DMA testing ends   ===\n");
}
