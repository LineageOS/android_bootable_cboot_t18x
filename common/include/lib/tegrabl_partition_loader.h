/*
 * Copyright (c) 2015 - 2017, NVIDIA Corporation.  All Rights Reserved.
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

/**
 * @brief Defines various binaries which can be loaded via loader.
 */
enum tegrabl_binary_type {
	TEGRABL_BINARY_KERNEL,
	TEGRABL_BINARY_KERNEL_DTB,
	TEGRABL_BINARY_RECOVERY_KERNEL,
	TEGRABL_BINARY_NCT,
	TEGRABL_BINARY_SMD,
	TEGRABL_BINARY_MAX /* cardinality */
};

/**
 * @brief Indicates which copy of the binary needs to be loaded
 */
enum tegrabl_binary_copy {
	TEGRABL_BINARY_COPY_PRIMARY,
	TEGRABL_BINARY_COPY_RECOVERY,
	TEGRABL_BINARY_COPY_MAX /* cardinality */
};

/**
 *@brief Binary information table
 */
struct tegrabl_binary_info {
	char *partition_name;
	void *load_address;
};

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
	enum tegrabl_binary_type bin_type, void **load_address,
	uint32_t *binary_length, enum tegrabl_binary_copy binary_copy);


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
tegrabl_error_t tegrabl_load_binary(enum tegrabl_binary_type bin_type,
	void **load_address, uint32_t *binary_length);

/**
 * @brief Updates the location of recovery image blob downloaded
 * in recovery for flashing or rcm boot.
 *
 * @param blob New location of blob.
 */
void tegrabl_loader_set_blob_address(void *blob);

#endif /* INCLUDED_TEGRABL_PARTITION_LOADER_H */
