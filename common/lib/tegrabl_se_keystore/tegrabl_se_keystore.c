/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

#define MODULE TEGRABL_ERR_SE_CRYPTO

#include "build_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_sigheader.h>
#include <tegrabl_page_allocator.h>
#include <tegrabl_se_keystore.h>
#include <tegrabl_page_allocator_pool_map.h>

static struct tegrabl_pubkey *keystore;

tegrabl_error_t tegrabl_keystore_relocate_to_sdram(
	struct tegrabl_pubkey **pubkey)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	void *pubkey_dram;

	if (*pubkey == NULL) {
		tegrabl_log_printf(TEGRABL_LOG_ERROR, "Invalid Keystore\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto done;
	}

	pubkey_dram = (void *)(uintptr_t)tegrabl_page_alloc(TEGRABL_MEMORY_DRAM,
			sizeof(struct tegrabl_pubkey), 0, 0, TEGRABL_MEMORY_START);
	if (pubkey_dram == NULL) {
		tegrabl_log_printf(TEGRABL_LOG_ERROR,
			"Failed to make space for Keystore\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 2);
		goto done;
	}

	memcpy(pubkey_dram, (void *)*pubkey, sizeof(struct tegrabl_pubkey));
	*pubkey = (struct tegrabl_pubkey *)pubkey_dram;
	pr_debug("KeyStore relocated to %p\n", pubkey_dram);

done:
	return err;
}

tegrabl_error_t tegrabl_keystore_init(void *load_address)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (load_address == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
		goto done;
	}
	keystore = (struct tegrabl_pubkey *)load_address;

done:
	return err;
}

struct tegrabl_pubkey *tegrabl_keystore_get(void)
{
	return keystore;
}

