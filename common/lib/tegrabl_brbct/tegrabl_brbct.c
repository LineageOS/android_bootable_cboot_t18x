/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_BRBCT
#define NVBOOT_TARGET_FPGA 0

#include "build_config.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h>
#include <tegrabl_utils.h>
#include <tegrabl_debug.h>
#include <tegrabl_brbit.h>
#include <tegrabl_brbct.h>
#include <tegrabl_malloc.h>
#include <tegrabl_se.h>
#include <tegrabl_page_allocator.h>
#include <nvboot_bct.h>
#include <nvboot_config.h>
#include <tegrabl_partition_manager.h>
#include <tegrabl_auth.h>

/* TODO: Move this to chip-specific file */
#define DEFAULT_BRBCT_LOAD_ADDRESS	0x4004E800

static uintptr_t brbct;

tegrabl_error_t tegrabl_brbct_relocate_to_sdram(uint64_t sdram_brbct_location)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t br_bct_size = 0;
	uint32_t br_bct_location = 0;
	void *ptr = &br_bct_location;
	uint32_t size = 0;

	if (sdram_brbct_location == 0llu) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	br_bct_size = tegrabl_brbct_size();

	size = sizeof(br_bct_location);
	err = tegrabl_brbit_get_data(TEGRABL_BRBIT_DATA_ACTIVE_BCT_PTR, 0,
			(void **)&ptr, &size);

	if (TEGRABL_NO_ERROR != err) {
		pr_error("Failed to retrieve active bct pointer\n");
		goto fail;
	}

	if (br_bct_location == 0) {
		pr_error("Invalid br bct location\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 0);
		goto fail;
	}

	memcpy((void *)(uintptr_t)sdram_brbct_location,
			(void *)(uintptr_t)br_bct_location, br_bct_size);

	brbct = (uintptr_t)sdram_brbct_location;

	pr_info("BR-BCT relocated to 0x%08"PRIx64"\n", sdram_brbct_location);
fail:
	return err;
}

tegrabl_error_t tegrabl_brbct_init(uintptr_t load_address)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (load_address == 0) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		return err;
	}
	brbct = load_address;

	return err;
}

uintptr_t tegrabl_brbct_get(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t brbct_location = 0;
	void *ptr = &brbct_location;
	uint32_t size = sizeof(brbct_location);

	if (brbct == 0UL) {
		err = tegrabl_brbit_get_data(TEGRABL_BRBIT_DATA_ACTIVE_BCT_PTR, 0,
			(void **)&ptr, &size);
		if (err == TEGRABL_NO_ERROR)
			brbct = brbct_location;
	}

	if (brbct == 0UL)
		brbct = DEFAULT_BRBCT_LOAD_ADDRESS;

	pr_debug("br bct location = %p\n", (void *)brbct);

	return brbct;
}

tegrabl_error_t tegrabl_brbct_write_multiple(
	tegrabl_error_t (*writer)(const void *buffer, uint64_t size,
	void *aux_info), void *buffer, void *aux_info, uint64_t part_size,
	uint64_t bct_size, uint32_t chunk_size)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t i = 0;
	uint32_t num_blocks = 0;
	uint32_t bootrom_block_size = 0;
	uint32_t bootrom_page_size = 0;
	uint32_t pages_in_bct = 0;
	uint32_t slot_size = 0;
	uint32_t br_bct_count = 0;
	uint32_t block_size = 0;
	uint32_t rounded_br_block_size = 0;
	NvBootConfigTable *br_bct = NULL;
	struct tegrabl_partition *bct_slot_aux_info = NULL;

	/* Block 0 : slot 0 , slot 1  & Block 1 <-> n-1 : slot 0 */
	br_bct = (NvBootConfigTable *)tegrabl_brbct_get();
	bootrom_block_size = 1 << br_bct->BlockSizeLog2;
	bootrom_page_size = 1 << br_bct->PageSizeLog2;

	num_blocks = part_size / bootrom_block_size;
	pages_in_bct = DIV_CEIL(bct_size, bootrom_page_size);

	/*
	The term "slot" refers to a potential location
	of a BCT in a block. A slot is the smallest integral
	number of pages that can hold a BCT.Thus, every
	BCT begins at the start of a page and may span
	multiple pages. A block is a space in memory that
	can hold multiple slots of BCTs.
	*/
	slot_size = pages_in_bct * bootrom_page_size;

	/*
	The BCT search sequence followed by BootROM is:
	Block 0, Slot 0
	Block 0, Slot 1
	Block 1, Slot 0
	Block 1, Slot 1
	Block 1, Slot 2
	.....
	Block 1, Slot N
	Block 2, Slot 0
	.....
	Block 2, Slot N
	.....
	Block 63, Slot N
	Based on the search sequence, we write the
	block 0, slot 1 BCT first, followed by one BCT
	in slot 0 of subsequent blocks and lastly one BCT
	in block0, slot 0.
	*/
	bct_slot_aux_info = (struct tegrabl_partition *)aux_info;
	/* Special case block 0 : slot 0 & 1 BCT's */
	/* Block 0 Slot 0 */
	err = writer(buffer, chunk_size, aux_info);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		pr_error("Failed to write partition\n");
		goto fail;
	}
	br_bct_count++;

	/* Block 0 Slot 1 */
	/* Get next sector byte , basically slot size aligned to block size */
	block_size = TEGRABL_BLOCKDEV_BLOCK_SIZE(bct_slot_aux_info->block_device);
	bct_slot_aux_info->offset = ROUND_UP(slot_size, block_size);

	err = writer(buffer, chunk_size, aux_info);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		pr_error("Failed to write partition\n");
		goto fail;
	}
	br_bct_count++;

	/* Fill Slot 0 for all other blocks */
	rounded_br_block_size = ROUND_UP(bootrom_block_size, block_size);
	for (i = 1; i < num_blocks &&
		(br_bct_count < BR_BCT_MAX_COPIES); i++) {
		bct_slot_aux_info->offset = i * rounded_br_block_size;

		err = writer(buffer, chunk_size, aux_info);
		if (err != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(err);
			pr_error("Failed to write partition\n");
			goto fail;
		}
		br_bct_count++;
	}

fail:
	return err;
}

uintptr_t tegrabl_get_nvpt_offset(void)
{
	uintptr_t bctptr;
	bctptr = tegrabl_brbct_get();
	if (bctptr)
		return bctptr + tegrabl_brbct_nvpt_offset();
	else
		return bctptr;
}

tegrabl_error_t tegrabl_brbct_verify_customerdata(uintptr_t bctptr)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uintptr_t customerdataptr;
	uint32_t signed_section_length = 0;

	if (bctptr == 0UL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	customerdataptr = bctptr + tegrabl_brbct_customerdata_offset();
	signed_section_length =
		tegrabl_brbct_customerdata_size() - CMAC_HASH_SIZE_BYTES;

	err = tegrabl_verify_cmachash(
			(void *)customerdataptr, signed_section_length,
			(void *)(customerdataptr + signed_section_length));

	if (err != TEGRABL_NO_ERROR)
		TEGRABL_SET_HIGHEST_MODULE(err);

fail:
	return err;
}

tegrabl_error_t tegrabl_brbct_update_customer_data(uintptr_t new_bct,
												   uint32_t size)
{
	tegrabl_error_t status = TEGRABL_NO_ERROR;
	uintptr_t cur_bct;
	uint32_t bct_size;
	uint32_t custdata_offset;
	uint32_t custdata_size;

	/* Initialize brbct from device */
	bct_size = tegrabl_brbct_size();

	if (size != bct_size) {
		pr_error("Brbct size is invalid.\n");
		status = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto exit;
	}

	cur_bct = tegrabl_brbct_get();
	/* copy costomer data from original brbct to new brbct */
	custdata_offset = tegrabl_brbct_customerdata_offset();
	custdata_size = tegrabl_brbct_customerdata_size();
	(void)memcpy((void *)(new_bct + custdata_offset),
				 (void *)(cur_bct + custdata_offset), custdata_size);

exit:
	return status;
}
