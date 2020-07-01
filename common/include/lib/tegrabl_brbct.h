/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef TEGRABL_BRBCT_H
#define TEGRABL_BRBCT_H

#include <stddef.h>
#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_partition_manager.h>
#include <tegrabl_global_data.h>

#define BR_BCT_MAX_COPIES 64U
#define BR_BLOCK_SIZE (16 * 1024)

/* TODO: Converge mb1-bct and br-bct libraries into a single library */

/**
 * @brief Copies bootrom bct from sysram/iram to sdram.
 * Uses the CARVEOUT_CPUBL_PARAMS in SDRAM.
 *
 * @param sdram_brbct_location will have the SDRAM location
 * in which BR BCT will be copied
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_brbct_relocate_to_sdram(uint64_t sdram_brbct_location);


/**
 * @brief Returns the size of bootrom bct.
 *
 * @return Non zero value.
 */
uint32_t tegrabl_brbct_size(void);

/**
 * @brief Initialize BR-BCT library
 *
 * @param load_address Address where BR-BCT is loaded.
 *
 * @return TEGRABL_NO_ERROR in case of success, TEGRABL_ERR_INVALID if
 * load_address is 0
 */
tegrabl_error_t tegrabl_brbct_init(uintptr_t load_address);

/**
 * @brief Get the BR-BCT address
 *
 * @return Address of BR-BCT, 0 if library not initialized
 */
uintptr_t tegrabl_brbct_get(void);

/**
 * @brief Get the BR-BCT's RSA public key address
 *
 * @return RSA public key's Address
 */
uintptr_t tegrabl_brbct_pubkey_rsa_get(void);

/**
 * @brief Write multiple copies of BR-BCT to storage
 *
 * @param buffer Input buffer.
 * @param partition handle to br-bct partition.
 * @param part_size Size of the BR-BCT partition
 * @param bct_size Size of the BR-BCT buffer
 * @param chunk_size Maximum transfer chunk size
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error code.
 */
tegrabl_error_t tegrabl_brbct_write_multiple(
	void *buffer, struct tegrabl_partition *partition, uint64_t part_size,
	uint64_t bct_size, uint32_t chunk_size);

/**
 * @brief Get PT offset wrt BR_BCT
 *
 * @return returns pointer to PT location in BR_BCT.
 */
uintptr_t tegrabl_get_nvpt_offset(void);
/**
 * @brief calculate PT offset in BR_BCT
 *
 * @return returns pointer to PT location.
 */
uint32_t tegrabl_brbct_nvpt_offset(void);

/**
 * @brief returns customer data offset in BR_BCT
 *
 * @return offset of customer data.
 */
uint32_t tegrabl_brbct_customerdata_offset(void);

/**
 * @brief returns customer data size in BR_BCT
 *
 * @return size of customer data.
 */
uint32_t tegrabl_brbct_customerdata_size(void);

/**
 * @brief verifies the bct auxdata signature
 *
 * @param bctptr Location of brbct
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error code.
 */
tegrabl_error_t tegrabl_brbct_verify_customerdata(uintptr_t bctptr);

/**
 * @brief Returns the offset of active marker structure.
 *
 * @return Offset of active marker structure in br-bct.
 */
struct active_marker_info* tegrabl_brbct_get_active_marker_data(void);

/**
 * @brief Copy current brbct's customer data to new bct
 *
 * @param new_bct start addr of the new bct
 * @param size size of new bct
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error code.
 */
tegrabl_error_t tegrabl_brbct_update_customer_data(uintptr_t new_bct,
												   uint32_t size);

#endif /* TEGRABL_BRBCT_H */

