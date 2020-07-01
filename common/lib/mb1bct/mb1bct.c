/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE	TEGRABL_ERR_MB1BCT

#include "build_config.h"
#include <tegrabl_error.h>
#include <inttypes.h>
#include <tegrabl_debug.h>
#include <tegrabl_mb1_bct.h>
#include <tegrabl_mb1bct_lib.h>
#include <tegrabl_malloc.h>
#include <string.h>
#include <tegrabl_page_allocator.h>
#include <tegrabl_blockdev.h>
#include <tegrabl_partition_manager.h>
#include <tegrabl_carveout_usage.h>

static tegrabl_mb1_bct_t *mb1bct = NULL;

tegrabl_error_t tegrabl_mb1bct_relocate(tegrabl_mb1_bct_t **src_addr,
										 void *reloc_addr)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t mb1bct_size = 0;

	if (*src_addr == NULL) {
		pr_error("Invalid MB1-BCT\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto done;
	}

	/* Get actual size from the bctsize field before mb1bct_init */
	mb1bct_size = (*src_addr)->bctsize;

	if (!reloc_addr) {
		reloc_addr =
			(void *)(uintptr_t)tegrabl_page_alloc(TEGRABL_MEMORY_DRAM,
					mb1bct_size, 0, 0, TEGRABL_MEMORY_START);
	}

	if (!reloc_addr) {
		pr_error("Failed to make space for MB1-BCT\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto done;
	}

	memcpy(reloc_addr, *src_addr, mb1bct_size);
	pr_info("MB1-BCT relocated to %p from %p\n", reloc_addr, *src_addr);

	*src_addr = reloc_addr;
	mb1bct = reloc_addr;

done:
	return err;
}

static tegrabl_error_t tegrabl_mb1bct_check_subversions(
		tegrabl_mb1_bct_t *mb1_bct)
{
	/* NOTE: Update only newly added variables. If there is change
	 * in variable value interpretation then it should be handled at
	 * that place only, but do not update value of variable.
	 */

	TEGRABL_ASSERT(mb1_bct);

	/* Check for ast */
	switch (mb1_bct->ast.version) {
	case 0:
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	case 1:
		/* Fall through. Add new versions below */
	default:
		break;
	}

	/* Check for sw carveout */
	switch (mb1_bct->sw_carveout.version) {
	case 0:
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	case 1:
		mb1_bct->sw_carveout.ramdump.size = 0x20000U;
		/* Fall through */
	case 2:
		mb1_bct->sw_carveout.mb2_heap.size =
			TEGRABL_CARVEOUT_MB2_HEAP_SIZE;
		mb1_bct->sw_carveout.cpubl_params.size =
			TEGRABL_CARVEOUT_CPUBL_PARAMS_SIZE;
		/* Fall through. Add new versions below */
	default:
		break;
	}

	/* Check for debug */
	switch (mb1_bct->debug.version) {
	case 0:
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	case 1:
		/* Fall through. Add new versions below */
	default:
		break;
	}

	/* Check for aotag */
	switch (mb1_bct->aotag.version) {
	case 0:
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	case 1:
		/* Fall through. Add new versions below */
	default:
		break;
	}

	/* Check for device params */
	switch (mb1_bct->dev_param.version) {
	case 0:
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	case 1:
		/* Fall through. Add new versions below */
	default:
		break;
	}

	/* Check for wdtcr version */
	switch (mb1_bct->wdt.version) {
	case 0:
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	case 1:
		if (mb1_bct->wdt.bpmp_wdtcr == 0) {
			mb1_bct->wdt.bpmp_wdtcr = 0x710640;
		}
		if (mb1_bct->wdt.sce_wdtcr == 0) {
			mb1_bct->wdt.sce_wdtcr = 0x700000;
		}
		if (mb1_bct->wdt.aon_wdtcr == 0) {
			mb1_bct->wdt.aon_wdtcr = 0x700000;
		}
		if (mb1_bct->wdt.rtc2_ao_wdtcr == 0) {
			mb1_bct->wdt.rtc2_ao_wdtcr = 0x700000;
		}
		if (mb1_bct->wdt.top_wdt0_wdtcr == 0) {
			mb1_bct->wdt.top_wdt0_wdtcr = 0x715017;
		}
		if (mb1bct->wdt.top_wdt1_wdtcr == 0) {
			mb1_bct->wdt.top_wdt1_wdtcr = 0x710640;
		}
		if (mb1bct->wdt.top_wdt2_wdtcr == 0) {
			mb1_bct->wdt.top_wdt2_wdtcr = 0x700000;
		}
		break;
	default:
		break;
	}

	/* Check for aocluster_data version */
	switch (mb1_bct->aocluster_data.version) {
	case 0:
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	case 1:
		/* Fall through. Add new versions below */
	default:
		break;
	}

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t tegrabl_mb1bct_check_version(
		tegrabl_mb1_bct_t *mb1_bct)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	TEGRABL_ASSERT(mb1_bct);

	/* NOTE: Update only newly added variables. If there is change
	 * in variable value interpretation then it should be handled at
	 * that place only, but do not update value of variable.
	 */

	pr_info("Supported mb1-bct versions %u-%u\n", 9, TEGRABL_MB1BCT_VERSION);
	pr_info("Version of mb1 bct binary %u\n", mb1_bct->version);

	if (mb1_bct->version < 9) {
		pr_critical("Unsupported mb1-bct version %u\n", mb1_bct->version);
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}

	/* Check for mb1-bct */
	switch (mb1_bct->version) {
	case 9:
	case 10:
		mb1_bct->aocluster_data.version = 1;
		mb1_bct->aocluster_data.evp_reset_addr = 0xc480000;
	case 11:
	case 12:
	case 13:
		mb1_bct->clock.pllaon_divn_frac = 0;
		/* Fall through. Add new versions below */
	default:
		break;
	}

	err = tegrabl_mb1bct_check_subversions(mb1_bct);

	return err;
}

tegrabl_error_t tegrabl_mb1bct_init(tegrabl_mb1_bct_t *load_address)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (load_address == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto done;
	}

	err = tegrabl_mb1bct_check_version(load_address);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	mb1bct = load_address;

done:
	return err;
}

tegrabl_mb1_bct_t* tegrabl_mb1bct_get(void)
{
	return mb1bct;
}

tegrabl_error_t tegrabl_mb1bct_get_size(uint32_t *mb1bct_size)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (mb1bct == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto done;
	}
	*mb1bct_size = mb1bct->bctsize;

done:
	return err;
}

tegrabl_error_t tegrabl_mb1bct_write_multiple(
	tegrabl_error_t (*writer)(const void *buffer, uint64_t size,
	void *aux_info), void *buffer, void *aux_info, uint64_t part_size,
	uint32_t chunk_size)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t i = 0;
	uint32_t num_copies = 0;
	uint32_t block_size = 0;
	uint32_t rounded_slot_size = 0;
	struct tegrabl_partition *bct_slot_aux_info = NULL;

	num_copies = part_size / TEGRABL_MB1BCT_SLOT_SIZE;
	bct_slot_aux_info = (struct tegrabl_partition *)aux_info;
	block_size = TEGRABL_BLOCKDEV_BLOCK_SIZE(bct_slot_aux_info->block_device);
	rounded_slot_size = ROUND_UP(TEGRABL_MB1BCT_SLOT_SIZE, block_size);

	pr_debug("num_copies:%u, block_size:%u, slot_size_aligned:%u\n",
			num_copies, block_size, rounded_slot_size);

	for (i = 0; i < num_copies; i++) {
		/* Calculate next bct copy offset. Each bct copy will be in
		one slot which is aligned to block size ."Slot" is the smallest
		integral number of pages that can hold a BCT.*/
		bct_slot_aux_info->offset = i * rounded_slot_size;

		pr_debug("bct copy offset: %"PRIu64"\n", bct_slot_aux_info->offset);

		err = writer(buffer, chunk_size, aux_info);
		if (err != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(err);
			pr_error("Failed to write partition\n");
			goto fail;
		}
	}

fail:
	return err;
}
