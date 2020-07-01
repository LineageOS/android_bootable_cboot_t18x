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

#define MODULE TEGRABL_ERR_AUTH
#define NVBOOT_TARGET_FPGA 0

#include "build_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_malloc.h>
#include <tegrabl_crypto.h>
#include <tegrabl_fuse.h>
#include <tegrabl_se_keystore.h>
#include <tegrabl_auth.h>
#include <tegrabl_sigheader.h>
#include <tegrabl_page_allocator.h>
#include <tegrabl_binary_types.h>
#include <tegrabl_rpb.h>
#include <nvboot_warm_boot_0.h>
#include <nvboot_bct.h>

#define BR_BCT_PUBKEY_ADDRESS 0x4004E80C

struct tegrabl_verify_context {
	enum tegrabl_binary_type bin_type;
	void *in_buffer;
	void *signed_section;
	uint32_t signed_section_size;
	void *signature;
	struct tegrabl_crypto_aes_context crypto_aes_context;
	struct tegrabl_crypto_rsa_pss_context crypto_rsa_pss_context;
	union tegrabl_crypto_context *crypto_contexts[2];
};

static tegrabl_error_t tegrabl_auth_signed_section(
		struct tegrabl_verify_context *context, uint32_t secure_mode)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_rpb_handle *rpb = NULL;

	switch (context->bin_type) {
	case TEGRABL_BINARY_SC7_RESUME_FW:
	{
		NvBootWb0RecoveryHeader *header = NULL;
		header = (NvBootWb0RecoveryHeader *)context->in_buffer;

		if (header == NULL) {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}

		if (header->LengthInsecure == 0ULL) {
			pr_debug("SC7 length is zero\n");
			error = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 0);
			goto fail;
		}

		context->signed_section = &header->RandomAesBlock;
		context->signed_section_size = header->LengthInsecure -
			offsetof(NvBootWb0RecoveryHeader, RandomAesBlock);

		pr_debug("%p sc7 signed section of size %d @ %p\n",
				header, context->signed_section_size, context->signed_section);
		if (secure_mode == FUSE_BOOT_SECURITY_AESCMAC) {
			context->signature = header->Signatures.AesCmacHash.Hash;
		} else {
			context->signature =
				header->Signatures.RsaSsaPssSig.RsaSsaPssSigNvU8.RsaSsaPssSig;
		}
		break;
	}
	case TEGRABL_BINARY_BR_BCT:
	{
		NvBootConfigTable *bct = (NvBootConfigTable *)context->in_buffer;
		if (bct == NULL) {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}

		if (bct->BctSize != sizeof(NvBootConfigTable)) {
			pr_error("BCT size mismatch\n");
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}

		context->signed_section = &bct->RandomAesBlock;
		context->signed_section_size = bct->BctSize -
			offsetof(NvBootConfigTable, RandomAesBlock);

		pr_debug("%p br-bct signed section of size %d @ %p\n",
				bct, context->signed_section_size, context->signed_section);
		if (secure_mode == FUSE_BOOT_SECURITY_AESCMAC) {
			context->signature = bct->Signatures.AesCmacHash.Hash;
		} else {
			context->signature =
				bct->Signatures.RsaSsaPssSig.RsaSsaPssSigNvU8.RsaSsaPssSig;
		}
		break;
	}
	case TEGRABL_BINARY_RPB:
	{
		rpb = (struct tegrabl_rpb_handle *)(context->in_buffer);
		context->signed_section = (void *)rpb;
		context->signed_section_size = RPB_SIG_OFF;

		/* RPB is always PKC signed */
		context->signature = (void *)(rpb->signature);
		break;
	}
	default:
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}

fail:
	return error;
}

static tegrabl_error_t tegrabl_auth_init_crypto_context(
		struct tegrabl_verify_context *context, uint32_t secure_mode)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_crypto_aes_context *crypto_aes_context = NULL;
	struct tegrabl_crypto_rsa_pss_context *crypto_rsa_pss_context = NULL;
	struct se_aes_context *se_aes_context = NULL;

	pr_debug("Filling crypto context\n");
	switch (secure_mode) {
	case FUSE_BOOT_SECURITY_AESCMAC:
	{
		crypto_aes_context = &context->crypto_aes_context;
		se_aes_context = &crypto_aes_context->se_context;

		crypto_aes_context->is_verify = true;
		se_aes_context->keysize = TEGRABL_CRYPTO_AES_KEYSIZE_128;
		se_aes_context->is_decrypt = false;
		se_aes_context->keyslot = 14;
		se_aes_context->is_hash = true;
		se_aes_context->total_size = context->signed_section_size;

		pr_debug("Initializing AES context\n");
		error = tegrabl_crypto_init(TEGRABL_CRYPTO_AES,
				(union tegrabl_crypto_context *)crypto_aes_context);
		if (error != TEGRABL_NO_ERROR)
			TEGRABL_SET_HIGHEST_MODULE(error);

		crypto_aes_context->in_hash = context->signature;
		context->crypto_contexts[0] =
			(union tegrabl_crypto_context *)crypto_aes_context;
		break;
	}
	case FUSE_BOOT_SECURITY_RSA_ENCRYPTION:
	{
		if (context->bin_type == TEGRABL_BINARY_BR_BCT) {
			crypto_aes_context = &context->crypto_aes_context;
			se_aes_context = &crypto_aes_context->se_context;

			crypto_aes_context->is_verify = false;
			se_aes_context->keysize = TEGRABL_CRYPTO_AES_KEYSIZE_128;
			se_aes_context->is_decrypt = true;
			se_aes_context->keyslot = 14;
			se_aes_context->is_hash = false;
			se_aes_context->total_size = context->signed_section_size;

			pr_debug("Initializing AES context\n");
			error = tegrabl_crypto_init(TEGRABL_CRYPTO_AES,
					(union tegrabl_crypto_context *)crypto_aes_context);
			if (error != TEGRABL_NO_ERROR)
				TEGRABL_SET_HIGHEST_MODULE(error);

			context->crypto_contexts[1] =
				(union tegrabl_crypto_context *)crypto_aes_context;
		}

		/* Fall through */
	}
	case FUSE_BOOT_SECURITY_RSA:
	{
		crypto_rsa_pss_context = &context->crypto_rsa_pss_context;

		crypto_rsa_pss_context->key_size = RSA_2048_KEY_SIZE_BITS;
		crypto_rsa_pss_context->se_context.input_size =
			context->signed_section_size;

		/* fixme - remove the need of filling this. Currently BR is
		 * clearing the oem rsa key slot. if it is set by mb1 based on op mode
		 * we need not write rsa key everytime
		 */
		/* fixme - use bct library instead of hard coding */
		pr_debug("Filling rsa key\n");
		crypto_rsa_pss_context->key = (uint32_t *)BR_BCT_PUBKEY_ADDRESS;
		pr_debug("Initializing RSA context\n");
		error = tegrabl_crypto_init(TEGRABL_CRYPTO_RSA_PSS,
				(union tegrabl_crypto_context *)crypto_rsa_pss_context);

		crypto_rsa_pss_context->signature = (void *)context->signature;
		context->crypto_contexts[0] =
			(union tegrabl_crypto_context *)crypto_rsa_pss_context;

		break;
	}
	default:
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		goto fail;
	}

fail:
	return error;
}

tegrabl_error_t tegrabl_verify_binary(uint32_t bin_type,
		void *bin)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_verify_context context = { 0 };
	static uint32_t secure_mode;

	if (bin == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	context.bin_type = bin_type;
	context.in_buffer = bin;

	if (secure_mode == 0U) {
		pr_debug("Fetching security mode\n");
		error = tegrabl_fuse_read(FUSE_BOOT_SECURITY_INFO,
				&secure_mode, sizeof(secure_mode));
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}
	}

	pr_debug("Fetching signed section\n");
	error = tegrabl_auth_signed_section(&context, secure_mode);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	error = tegrabl_auth_init_crypto_context(&context, secure_mode);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	pr_debug("processing buffer @ %p of size %d bytes\n",
			context.signed_section, context.signed_section_size);
	error = tegrabl_crypto_process_block(context.crypto_contexts[0],
				context.signed_section, context.signed_section_size,
				context.signed_section);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	if (context.crypto_contexts[1]) {
		error = tegrabl_crypto_process_block(context.crypto_contexts[1],
					context.signed_section, context.signed_section_size,
					context.signed_section);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}
	}

	error = tegrabl_crypto_finalize(context.crypto_contexts[0]);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

fail:
	tegrabl_crypto_close(context.crypto_contexts[0]);

	if (context.crypto_contexts[1])
		tegrabl_crypto_close(context.crypto_contexts[1]);

	return error;
}

tegrabl_error_t tegrabl_verify_cmachash(void *buffer,
		uint32_t buffer_size, void *cmachash)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_crypto_aes_context crypto_aes_context = { 0 };
	static uint8_t zero_key[CMAC_HASH_SIZE_BYTES] = {0};
	struct se_aes_context *se_aes_context = NULL;

	if ((buffer == NULL) || (buffer_size == 0) || (cmachash == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if ((buffer_size % SE_AES_BLOCK_LENGTH) != 0) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	se_aes_context = &crypto_aes_context.se_context;

	crypto_aes_context.is_verify = true;
	se_aes_context->keysize = TEGRABL_CRYPTO_AES_KEYSIZE_128;
	se_aes_context->is_decrypt = false;
	se_aes_context->is_encrypt = false;
	se_aes_context->keyslot = 1;
	se_aes_context->is_hash = true;
	se_aes_context->total_size = buffer_size;
	se_aes_context->pkey = zero_key;

	pr_debug("Initializing AES context\n");
	error = tegrabl_crypto_init(TEGRABL_CRYPTO_AES,
			(void *)&crypto_aes_context);
	if (error != TEGRABL_NO_ERROR)
		TEGRABL_SET_HIGHEST_MODULE(error);

	crypto_aes_context.in_hash = cmachash;

	pr_debug("processing buffer @ %p of size %d bytes\n",
			buffer, buffer_size);
	error = tegrabl_crypto_process_block((void *)&crypto_aes_context,
				buffer, buffer_size, buffer);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	error = tegrabl_crypto_finalize((void *)&crypto_aes_context);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

fail:
	tegrabl_crypto_close((void *)&crypto_aes_context);

	return error;
}

tegrabl_error_t tegrabl_cipher_binary(void *buffer,
		uint32_t buffer_size, void *output_buffer, bool is_decrypt)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_crypto_aes_context crypto_aes_context = { 0 };
	struct se_aes_context *se_aes_context = NULL;

	if ((buffer == NULL) || (buffer_size == 0U) || (output_buffer == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if ((buffer_size % SE_AES_BLOCK_LENGTH) != 0) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	se_aes_context = &crypto_aes_context.se_context;

	crypto_aes_context.is_verify = false;
	se_aes_context->keysize = TEGRABL_CRYPTO_AES_KEYSIZE_128;
	/* set decryption flag based on is_decrypt param */
	if (is_decrypt == true) {
		se_aes_context->is_decrypt = true;
	} else {
		se_aes_context->is_encrypt = true;
	}
	/* encryption/decryption using SBK key slot*/
	se_aes_context->keyslot = 14;
	se_aes_context->is_hash = false;
	se_aes_context->total_size = buffer_size;

	pr_debug("Initializing AES context\n");
	error = tegrabl_crypto_init(TEGRABL_CRYPTO_AES,
			(void *)&crypto_aes_context);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	pr_debug("processing buffer @ %p of size %d bytes\n",
			buffer, buffer_size);
	error = tegrabl_crypto_process_block((void *)&crypto_aes_context,
				buffer, buffer_size, output_buffer);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	error = tegrabl_crypto_finalize((void *)&crypto_aes_context);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

fail:
	tegrabl_crypto_close((void *)&crypto_aes_context);

	return error;
}
