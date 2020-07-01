/*
 * Copyright (c) 2015-2019, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_PARTITION_LOADER_H
#define INCLUDED_TEGRABL_PARTITION_LOADER_H

#include <tegrabl_error.h>
#include <tegrabl_binary_types.h>
#include <tegrabl_blockdev.h>
#include <tegrabl_partition_manager.h>

/**
 *@brief Binary information table
 */
struct tegrabl_binary_info {
	char *partition_name;
	void *load_address;
};

/**
 * @brief Provide name of a partition.
 *
 * @param bin_type Type of binary whose name is to be provided
 * @param binary_copy primary or recovery copy which needs to be read
 * @param partition_name buffer to hold the name of the partition
 *
 * @return TEGRABL_NO_ERROR if partition name was found, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_get_partition_name(tegrabl_binary_type_t bin_type,
                                                tegrabl_binary_copy_t binary_copy,
                                                char *partition_name);

/**
 * @brief Read specified binary from storage into memory.
 *
 * @param bin_type Type of binary to be loaded
 * @param load_address Gets updated with memory address where
 * binary is loaded.
 * @param binary_length length of the binary which is read.
 * @param binary_copy primary or recovery copy which needs to be read
 *
 * @return TEGRABL_NO_ERROR if loading was successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_load_binary_copy(
	tegrabl_binary_type_t bin_type, void **load_address,
	uint32_t *binary_length, tegrabl_binary_copy_t binary_copy);

/**
 * @brief Read specified binary from storage into memory.
 *		  If loading primary copy fails, loads recovery copy of the binary.
.*
 * @param bin_type Type of binary to be loaded
 * @param load_address Gets updated with memory address where
 * binary is loaded.
 * @param binary_length length of the binary which is read.
 *
 * @return TEGRABL_NO_ERROR if loading was successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_load_binary(tegrabl_binary_type_t bin_type,
	void **load_address, uint32_t *binary_length);

/**
 * @brief Read specified binary from given block device storage into memory.
.*
 * @param bin_type Type of binary to be loaded
 * @param load_address Gets updated with memory address where binary is loaded.
 * @param binary_length length of the binary which is read.
 * @param bdev block device from which the binary should be read.
 *
 * @return TEGRABL_NO_ERROR if loading was successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_load_binary_bdev(tegrabl_binary_type_t bin_type, void **load_address,
										 uint32_t *binary_length,  tegrabl_bdev_t *bdev);

/**
 * @brief Updates the location of recovery image blob downloaded
 * in recovery for flashing or rcm boot.
 *
 * @param blob New location of blob.
 */
void tegrabl_loader_set_blob_address(void *blob);

#endif /* INCLUDED_TEGRABL_PARTITION_LOADER_H */
