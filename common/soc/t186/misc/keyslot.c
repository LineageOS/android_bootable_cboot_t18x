/*
 * Copyright (c) 2016, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_KEYSLOT
#include "build_config.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <tegrabl_se.h>
#include <tegrabl_debug.h>
#include <tegrabl_malloc.h>
#include <tegrabl_fuse.h>
#include <tegrabl_error.h>
#include <tegrabl_soc_misc.h>

static uint8_t sample_text[SE_AES_BLOCK_LENGTH] = {
        0x00, 0x01, 0x02, 0x03,
        0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b,
        0x0c, 0x0d, 0x0e, 0x0f
};

static uint8_t cipher_test[SE_AES_BLOCK_LENGTH] = {
        0x7A, 0xCA, 0x0F, 0xD9,
        0xBC, 0xD6, 0xEC, 0x7C,
        0x9F, 0x97, 0x46, 0x66,
        0x16, 0xE6, 0xA2, 0x82
};

bool tegrabl_keyslot_check_if_key_is_nonzero(uint8_t keyslot)
{
	bool fuse_status = false;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct se_aes_input_params input_params = {0};
	struct se_aes_context context = {0};
	uint8_t *input_data = NULL;
	uint8_t *input_iv = NULL;

	input_data = tegrabl_alloc(TEGRABL_HEAP_DMA, SE_AES_BLOCK_LENGTH);
	if (input_data == NULL) {
		pr_error("Unable to allocate memory for input_data\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}
	memcpy(input_data , sample_text, SE_AES_BLOCK_LENGTH);


	input_iv = tegrabl_alloc(TEGRABL_HEAP_DMA, SE_AES_BLOCK_LENGTH);
	if (input_iv == NULL) {
		pr_error("Unable to allocate memory for input_iv\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	memset(input_iv , 0, SE_AES_BLOCK_LENGTH);
	/* fill the context */
	context.keyslot = keyslot;
	context.is_encrypt = true;
	context.is_hash = false;
	context.total_size = SE_AES_BLOCK_LENGTH;
	context.iv_encrypt = input_iv;

	/* fill the input params */
	input_params.src = (void *)input_data;
	input_params.dst =  input_params.src;
	input_params.input_size = context.total_size;
	input_params.size_left = context.total_size;

	error = tegrabl_se_aes_process_block(&input_params, &context);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Failed to encrypt input data\n");
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}
	/* compare if the encrypted data is same as that of cipher text */
	if (memcmp(input_data, cipher_test, SE_AES_BLOCK_LENGTH) != 0) {
		fuse_status = true;
	}
fail:
	if (input_data != NULL) {
		tegrabl_free(input_data);
	}
	if (input_iv != NULL) {
		tegrabl_free(input_iv);
	}

	return fuse_status;
}


