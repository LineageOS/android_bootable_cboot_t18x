/*
 * Copyright (c) 2015-2018, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_MB1BCT_LIB_H
#define INCLUDED_TEGRABL_MB1BCT_LIB_H

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_mb1_bct.h>
#include <tegrabl_partition_manager.h>

#define TEGRABL_MB1BCT_SLOT_SIZE (64U * 1024U)

/**
 * @brief Initialize MB1BCT library
 *
 * @param load_address Address where MB1-BCT is loaded.
 *
 * @return TEGRABL_NO_ERROR in case of success, TEGRABL_ERR_INVALID if
 * load_address is NULL
 */
tegrabl_error_t tegrabl_mb1bct_init(tegrabl_mb1_bct_t *load_address);

/**
 * @brief Get the MB1-BCT pointer
 *
 * @return Pointer to Mb1-BCT, NULL if library not initialized
 */
tegrabl_mb1_bct_t *tegrabl_mb1bct_get(void);

/**
 * @brief MB1BCT is loaded in SysRAM. MB1 reads MB1-BCT and configures/locks GSC
 * carveouts. Some of these GSC carveouts are in SysRAM colliding with MB1-BCT.
 * So before allocating these carveouts, MB1BCT needs to be moved
 * to some other place (to BPMP-BTCM/DRAM).
 *
 * @param src_addr the original location of mb1bct at the time of call,
 * and is updated by this API after relocation
 *
 * @param reloc_addr the destination location to where mb1bct would be copied.
 * And if param is null, it would allocate memory on DRAM and copy over there.
 *
 * @return TEGRABL_NO_ERROR in case of success, else returns appropriate error
 */
tegrabl_error_t tegrabl_mb1bct_relocate(tegrabl_mb1_bct_t **src_addr,
										 void *reloc_addr);

/**
 * @brief Get the MB1-BCT size
 *
 * @param mb1bct_size pointer to the size field to be filled by MB1-BCT size
 *
 * @return TEGRABL_NO_ERROR in case of success, TEGRABL_ERR_INVALID if
 * mb1bct global pointer is NULL
 */
tegrabl_error_t tegrabl_mb1bct_get_size(uint32_t *mb1bct_size);

/**
 * @brief Write multiple copies of MB1-BCT to storage
 *
 * @param buffer Input buffer.
 * @param partition Handle to mb1-bct partition
 * @param part_size Size of the MB1-BCT partition
 * @param chunk_size Maximum transfer chunk size
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error code.
 */
tegrabl_error_t tegrabl_mb1bct_write_multiple(
	void *buffer, struct tegrabl_partition *partition, uint64_t part_size,
	uint32_t chunk_size);

#endif /* INCLUDED_TEGRABL_MB1BCT_LIB_H */
