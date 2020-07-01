/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef TEGRABL_BRBIT_H
#define TEGRABL_BRBIT_H

#include <stddef.h>
#include <stdint.h>
#include <tegrabl_error.h>

/**
 * @brief Defines the set of data that can be retrieved or set
 * for Bootrom BIT (Boot Information Table).
 */
enum tegrabl_brbit_data_type {
	TEGRABL_BRBIT_DATA_BRBIT,
	TEGRABL_BRBIT_DATA_BOOTROM_VERSION,
	TEGRABL_BRBIT_DATA_BOOT_DATA_VERSION,
	TEGRABL_BRBIT_DATA_RCM_VERSION,
	TEGRABL_BRBIT_DATA_BOOT_TYPE,
	TEGRABL_BRBIT_DATA_PRIMARY_DEVICE,
	TEGRABL_BRBIT_DATA_SECONDARY_DEVICE,
	TEGRABL_BRBIT_DATA_OSC_FREQUENCY,
	TEGRABL_BRBIT_DATA_IS_VALID_BCT,
	TEGRABL_BRBIT_DATA_ACTIVE_BCT_BLOCK,
	TEGRABL_BRBIT_DATA_ACTIVE_BCT_SECTOR,
	TEGRABL_BRBIT_DATA_BCT_SIZE,
	TEGRABL_BRBIT_DATA_ACTIVE_BCT_PTR,
	TEGRABL_BRBIT_DATA_BL_STATUS,
	TEGRABL_BRBIT_DATA_SAFE_START_ADDRESS,
	TEGRABL_BRBIT_DATA_MAX,
};

/**
 * @brief Function to retrieve the offset and size of a field in BR-BIT.
 *
 * @param type Field type.
 * @param instance Index of field.
 * @param offset Will be set to offset of input field.
 * @param size Will be set to size of input field.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_brbit_get_offset_size(enum tegrabl_brbit_data_type type,
		uint32_t instance, uint32_t *offset, uint32_t *size);

/**
 * @brief Function to retrieve the content of a particular field in
 * BR-BIT.
 *
 * @param type Field type.
 * @param instance Index of field.
 * @param buffer If *buffer is null then *buffer will point to correct offset in
 * BR-BIT. Else data will be copied in *buffer address.
 * @param buffer_size Size of the input buffer.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_brbit_get_data(enum tegrabl_brbit_data_type type,
		uint32_t instance, void **buffer, uint32_t *buffer_size);

/**
 * @brief Function to set the content of a particular field in BR-BIT.
 *
 * @param type Field type.
 * @param instance Index of field.
 * @param buffer Buffer containing data.
 * @param buffer_size Size of the input buffer.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_brbit_set_data(enum tegrabl_brbit_data_type type,
		uint32_t instance, void *buffer, uint32_t buffer_size);

#endif /* TEGRABL_BRBIT_H */

