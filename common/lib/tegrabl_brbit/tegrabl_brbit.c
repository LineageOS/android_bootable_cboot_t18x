/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_BRBIT

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <tegrabl_utils.h>
#include <tegrabl_debug.h>
#include <tegrabl_brbit.h>
#include <tegrabl_brbit_core.h>

static uint8_t *brptr;

/**
 * @brief Gets the location of BR-BIT and verifies for
 * authenticity and sets brptr.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
static tegrabl_error_t tegrabl_brbit_open(void)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (brptr)
		goto fail;

	brptr = (uint8_t *)tegrabl_brbit_location();
	if (brptr == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 0);
		pr_error("Failed to get the address of bit\n");
		goto fail;
	}

	pr_info("bit @ %p\n", brptr);
	if (!tegrabl_brbit_verify(brptr)) {
		brptr = NULL;
		error = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 0);
		pr_error("Failed to verify bit\n");
		goto fail;
	}

fail:
	return error;
}

tegrabl_error_t tegrabl_brbit_get_data(enum tegrabl_brbit_data_type type,
		uint32_t instance, void **buffer, uint32_t *buffer_size)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t offset = 0;
	uint32_t size = 0;

	if (buffer == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if ((*buffer != NULL) && (buffer_size == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	error = tegrabl_brbit_open();
	if (TEGRABL_NO_ERROR != error) {
		pr_error("Failed to open BR BIT\n");
		goto fail;
	}

	error = tegrabl_brbit_get_offset_size(type, instance, &offset,
			&size);
	if (TEGRABL_NO_ERROR != error) {
		pr_error("Failed to get offset and size\n");
		goto fail;
	}

	/* If input buffer is given then do memcopy else
	 * update the location pointed by input buffer.
	 */
	if (*buffer) {
		if (*buffer_size < size) {
			error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);
			goto fail;
		}
		memcpy(*buffer, brptr + offset, size);
	} else {
		*buffer = brptr + offset;
		*buffer_size = size;
	}

fail:
	return error;
}

tegrabl_error_t tegrabl_brbit_set_data(enum tegrabl_brbit_data_type type,
		uint32_t instance, void *buffer, uint32_t buffer_size)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t offset = 0;
	uint32_t size = 0;

	if ((buffer == NULL) || (buffer_size == 0U)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto fail;
	}

	error = tegrabl_brbit_open();
	if (TEGRABL_NO_ERROR != error) {
		pr_error("Failed to open BR BIT\n");
		goto fail;
	}

	error = tegrabl_brbit_get_offset_size(type, instance, &offset,
			&size);
	if (TEGRABL_NO_ERROR != error) {
		pr_error("Failed to get offset and size\n");
		goto fail;
	}

	if (buffer_size > size) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 1);
		goto fail;
	}

	memcpy(brptr + offset, buffer, buffer_size);

fail:
	return error;
}

