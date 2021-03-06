/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

#define MODULE TEGRABL_ERR_AUTH

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
#include <tegrabl_prevent_rollback.h>
#include <tegrabl_brbct.h>
#include <tegrabl_soc_misc.h>
#include <tegrabl_se.h>

#define ONE_KB 1024
#define BR_BCT_ECCPUBKEY_ADDRESS 0x4004EA0C

#define AES_KEYSLOT_SBK 14

/* Crypto APIs are costly. Calling crypto API twice for small binaries
 * takes more time than calling once and copying binary by removing
 * header. If binary size is less than below threshold then use
 * later approach (validate in single go and copy actual binary to
 * destination location by removing header).
 */
#define FULL_BINARY_VERIFY_THRESHOLD (25U * 1024U)

/**
 * @brief Converts mode from header to auth mode
 *
 * @param mode Mode from header.
 *
 * @return Correspoding auth mode.
 */
static inline uint32_t tegrabl_auth_mode(tegrabl_signingtype_t mode)
{
	return 1UL << (uint32_t)mode;
}

/**
 * @brief Checks for magic in input buffer.
 *
 * @param buff Input buffer
 *
 * @return true if buffer contains header else false.
 */
static inline bool tegrabl_auth_check_sigheader(uint8_t *buff)
{
	if ((strncmp((const char *)buff, "GSHV", 4) == 0)
		|| (strncmp((const char *)buff, "NVDA", 4) == 0)) {
		return true;
	}

	return false;
}

/**
 * @brief Fills AES key slot as per the type of binary.
 *
 * @param bin_type Type of binary
 * @param aes_params AES params to be filled with key slot.
 *
 * @return TEGRABL_NO_ERROR if valid binary else TEGRABL_ERR_INVALID.
 */
static tegrabl_error_t tegrabl_auth_fill_aes_key(
	uint32_t bin_type, struct se_aes_context *aes_params)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	switch (bin_type) {
	case TEGRABL_BINARY_EARLY_SPEFW:
		aes_params->keyslot = 8;
		break;
	case TEGRABL_BINARY_SCE:
		aes_params->keyslot = 6;
		break;
	case TEGRABL_BINARY_BPMP_FW:
	case TEGRABL_BINARY_BPMP_FW_DTB:
		aes_params->keyslot = 9;
		break;
	case TEGRABL_BINARY_APE:
		aes_params->keyslot = 7;
		break;
	case TEGRABL_BINARY_MTS_PREBOOT:
	case TEGRABL_BINARY_DMCE:
		aes_params->keyslot = 10;
		break;
	default:
		pr_debug("Invalid bin type %d\n", bin_type);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 5);
		break;
	}

	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	pr_debug("aes key slot %d\n", aes_params->keyslot);
fail:
	return err;
}

/**
 * @brief Initializes the location of RSA key for specified binary type.
 *
 * @param bin_type Type of binary.
 * @param rsa_pss_context crytpo RSA context to be filled with location of key.
 *
 * @return TEGRABL_NO_ERROR if valid binary else TEGRABL_ERR_INVALID and
 * TEGRABL_ERR_NOT_INITIALIZED if keystore is not initialized.
 */
static tegrabl_error_t tegrabl_auth_fill_rsa_pss_key(
	uint32_t bin_type, struct tegrabl_crypto_rsa_pss_context *rsa_pss_context)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	static struct tegrabl_pubkey *pubkey;

	if (pubkey == NULL) {
		pubkey = tegrabl_keystore_get();
	}

	if (pubkey == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 0);
		pr_debug("Keystore is not initialized\n");
		goto fail;
	}

	switch (bin_type) {
	case TEGRABL_BINARY_BPMP_FW:
	case TEGRABL_BINARY_BPMP_FW_DTB:
		rsa_pss_context->key = (uint32_t *)(pubkey->bpmp_fw_pub_rsa_key);
		break;
	case TEGRABL_BINARY_EARLY_SPEFW:
		rsa_pss_context->key = (uint32_t *)pubkey->spe_fw_pub_rsa_key;
		break;
	case TEGRABL_BINARY_APE:
		rsa_pss_context->key = (uint32_t *)pubkey->ape_fw_pub_rsa_key;
		break;
	case TEGRABL_BINARY_SCE:
		rsa_pss_context->key = (uint32_t *)pubkey->scecpe_fw_pub_rsa_key;
		break;
	case TEGRABL_BINARY_MTS_PREBOOT:
	case TEGRABL_BINARY_DMCE:
		rsa_pss_context->key = (uint32_t *)pubkey->mts_pub_rsa_key;
		break;
	default:
		pr_debug("Invalid bin type %d\n", bin_type);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 7);
		break;
	}

	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	pr_debug("Rsa Key @%p\n", rsa_pss_context->key);
fail:
	return err;
}

/**
 * @brief Updates allowed modes as per binary type.
 *
 * @param auth Handle which keeps the information about binary
 * and header.
 */
static inline void tegrabl_update_allowed_modes(
		struct tegrabl_auth_handle *auth)
{
	uint32_t allowed_modes = 0;

	/* Define allowed crypto modes for binaries */
	switch (auth->bin_type) {
	case TEGRABL_BINARY_MTS_PREBOOT:
	case TEGRABL_BINARY_DMCE:
		allowed_modes |= tegrabl_auth_mode(TEGRABL_SIGNINGTYPE_NVIDIA_RSA);
		break;
	default:
		break;
	}

	if (auth->allow_oem_modes == true) {
		allowed_modes |= tegrabl_auth_mode(TEGRABL_SIGNINGTYPE_SBK) |
							tegrabl_auth_mode(TEGRABL_SIGNINGTYPE_OEM_RSA) |
							tegrabl_auth_mode(TEGRABL_SIGNINGTYPE_OEM_RSA_SBK) |
							tegrabl_auth_mode(TEGRABL_SIGNINGTYPE_OEM_ECC);
	}

	auth->allowed_modes = allowed_modes;
	pr_debug("Allowed modes 0x%08x\n", auth->allowed_modes);
}


tegrabl_error_t tegrabl_auth_initiate(uint32_t bin_type,
		void *dest_addr, uint32_t dest_size, struct tegrabl_auth_handle *auth)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if ((dest_addr == NULL) || (dest_size == 0U) || (auth == NULL) ||
		(dest_size < sizeof(struct tegrabl_sigheader))) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	memset(auth, 0x0, sizeof(struct tegrabl_auth_handle));

	auth->bin_type = bin_type;
	auth->dest_location = dest_addr;
	auth->dest_size = dest_size;
	auth->safe_dest_location = dest_addr;
	auth->allow_oem_modes = true;

	auth->check_nvidia_header = ((bin_type ==
										(uint32_t)TEGRABL_BINARY_MTS_PREBOOT) ||
								 (bin_type == (uint32_t)TEGRABL_BINARY_DMCE));

	pr_debug("Initiating authentication of binary %d @%p of size %d\n",
			bin_type, dest_addr, dest_size);

fail:
	return err;
}

/**
 * @brief Initializes the authentication handle as per the header
 * found.
 *
 * @param auth authentication handle
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
static tegrabl_error_t tegrabl_auth_process_header(
		struct tegrabl_auth_handle *auth,
		void *buffer,
		struct tegrabl_auth_header_info *header_info)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_sigheader *header = NULL;
	tegrabl_signingtype_t sign_type = TEGRABL_SIGNINGTYPE_NONE;
	uint32_t bin_type = 0;
	static uint32_t secure_mode;
	struct tegrabl_crypto_aes_context *crypto_aes_context = NULL;
	struct tegrabl_crypto_rsa_pss_context *crypto_rsa_pss_context = NULL;
#if defined(CONFIG_ENABLE_ECDSA)
	struct tegrabl_crypto_ecdsa_context *crypto_ecdsa_context = NULL;
#endif
	if (secure_mode == 0U) {

		err = tegrabl_fuse_read(FUSE_TYPE_BOOT_SECURITY_INFO,
			&secure_mode, sizeof(secure_mode));
		pr_debug("fuse_read=0x%x\n", secure_mode);
		if (err != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(err);
			goto fail;
		}
	}

	header = (struct tegrabl_sigheader *)buffer;

	if (header->signtype == TEGRABL_SIGNINGTYPE_NVIDIA_RSA) {
		sign_type = TEGRABL_SIGNINGTYPE_NVIDIA_RSA;
	} else {
		if (secure_mode == FUSE_BOOT_SECURITY_AESCMAC) {
			sign_type = TEGRABL_SIGNINGTYPE_ZERO_SBK;
		} else if (secure_mode == FUSE_BOOT_SECURITY_RSA) {
			sign_type = TEGRABL_SIGNINGTYPE_OEM_RSA;
		} else if (secure_mode == FUSE_BOOT_SECURITY_RSA_ENCRYPTION) {
#if defined(CONFIG_OS_IS_L4T)
			pr_info("cboot: Info: Handle RSA_SBK as RSA.");
			secure_mode = FUSE_BOOT_SECURITY_RSA;
			sign_type = TEGRABL_SIGNINGTYPE_OEM_RSA;
#else
			sign_type = TEGRABL_SIGNINGTYPE_OEM_RSA_SBK;
#endif
		} else if (secure_mode == FUSE_BOOT_SECURITY_ECC) {
				sign_type = TEGRABL_SIGNINGTYPE_OEM_ECC;
		} else {
			pr_critical("unsupported secure mode %d\n", secure_mode);
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}
	}

	bin_type = auth->bin_type;

	pr_debug("Processing generic header version %d\n", header->headerversion);

	pr_debug("Binary size %d, sign type %d, secure_mode %d\n",
			header->binarylength, header->signtype, secure_mode);

	if (auth->dest_size < header->binarylength) {
		pr_error("Binary size in header exceeds allowed size\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);
		goto fail;
	}

	if ((auth->binary_size != 0U) && (auth->binary_size < (header->binarylength + HEADER_SIZE))) {
		pr_error("Binary size in inner header cannot exceed ");
		pr_error("binary size on outer header\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

#if defined(CONFIG_ENABLE_ROLLBACK_PREVENTION)
	/* Check the rollback level in header to see if this binary has a rollback
	 * attempt, if any rollback attempt, should hang the DUT instead of reboot
	 */
	err = tegrabl_check_binary_rollback(bin_type, header->fw_ratchet_level);
	if (err != TEGRABL_NO_ERROR) {
		while (1)
			;
	}
#endif

	tegrabl_update_allowed_modes(auth);

	if ((tegrabl_auth_mode(sign_type) & auth->allowed_modes) == 0U) {
		pr_error("Invalid mode in header 0x%08x, allowed 0x%08x\n",
				tegrabl_auth_mode(sign_type), auth->allowed_modes);
		err = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 0);
		goto fail;
	}

	auth->allow_oem_modes = false;

	header_info->mode = sign_type;
	auth->binary_size = header->binarylength;
	header_info->binary_size = header->binarylength;

	header_info->validation_size = header->binarylength + SIGNED_SECTION_LEN;
	crypto_aes_context = &header_info->aes_context;
	crypto_rsa_pss_context = &header_info->rsa_pss_context;
#if defined(CONFIG_ENABLE_ECDSA)
	crypto_ecdsa_context = &header_info->ecdsa_context;
#endif

	switch (sign_type) {
	case TEGRABL_SIGNINGTYPE_SBK:
	{
		struct se_aes_context *se_aes_context = NULL;

		crypto_aes_context->in_hash = header->signatures.cryptohash;
		crypto_aes_context->is_verify = true;

		se_aes_context = &crypto_aes_context->se_context;
		se_aes_context->keysize = (uint8_t)TEGRABL_CRYPTO_AES_KEYSIZE_128;
		se_aes_context->is_decrypt = false;
		se_aes_context->keyslot = 14;
		se_aes_context->is_hash = true;
		se_aes_context->total_size = header_info->validation_size;

		pr_debug("Initializing AES context\n");
		err = tegrabl_crypto_init(TEGRABL_CRYPTO_AES,
				(union tegrabl_crypto_context *)crypto_aes_context);
		break;
	}
	case TEGRABL_SIGNINGTYPE_NVIDIA_RSA:
	{
		struct se_aes_context *se_aes_context = NULL;

		crypto_aes_context->in_hash = NULL;
		crypto_aes_context->is_verify = false;
		se_aes_context = &crypto_aes_context->se_context;

		se_aes_context->is_decrypt = true;
		se_aes_context->is_hash = false;
		se_aes_context->total_size = header_info->validation_size -
			SIGNED_SECTION_LEN;

		pr_debug("Filling aes key\n");
		err = tegrabl_auth_fill_aes_key(bin_type, se_aes_context);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}

		pr_debug("Initializing AES context\n");
		err = tegrabl_crypto_init(TEGRABL_CRYPTO_AES,
				(union tegrabl_crypto_context *)crypto_aes_context);
		if (err != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(err);
			goto fail;
		}

		crypto_rsa_pss_context->key_size = RSA_2048_KEY_SIZE_BITS;
		crypto_rsa_pss_context->se_context.input_size =
			header_info->validation_size;

		pr_debug("Filling rsa key\n");
		err = tegrabl_auth_fill_rsa_pss_key(bin_type, crypto_rsa_pss_context);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
		crypto_rsa_pss_context->signature =
			(void *)header->signatures.rsapsssig;
		pr_debug("Initializing RSA context\n");
		err = tegrabl_crypto_init(TEGRABL_CRYPTO_RSA_PSS,
				(union tegrabl_crypto_context *)crypto_rsa_pss_context);
		break;
	}
	case TEGRABL_SIGNINGTYPE_OEM_RSA:
	{
		crypto_rsa_pss_context->key_size = RSA_2048_KEY_SIZE_BITS;
		crypto_rsa_pss_context->se_context.input_size =
			header_info->validation_size;
		crypto_rsa_pss_context->signature =
			(void *)header->signatures.rsapsssig;

		/* fixme - remove the need of filling this. Currently BR is
		 * clearing the oem rsa key slot. if it is set by mb1 based on op mode
		 * we need not write rsa key everytime
		 */
		pr_debug("Filling rsa key\n");
		crypto_rsa_pss_context->key =
									(uint32_t *)tegrabl_brbct_pubkey_rsa_get();
		pr_debug("Initializing RSA context\n");
		err = tegrabl_crypto_init(TEGRABL_CRYPTO_RSA_PSS,
				(union tegrabl_crypto_context *)crypto_rsa_pss_context);

		break;
	}
	case TEGRABL_SIGNINGTYPE_OEM_RSA_SBK:
	{
		struct se_aes_context *se_aes_context = NULL;

		if (secure_mode != FUSE_BOOT_SECURITY_RSA_ENCRYPTION) {
			pr_debug("Sigtype %d and secure_mode %d incompatible\n",
					sign_type, secure_mode);
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}
		crypto_aes_context->in_hash = NULL;
		crypto_aes_context->is_verify = false;
		se_aes_context = &crypto_aes_context->se_context;

		se_aes_context->is_decrypt = true;
		se_aes_context->is_hash = false;
		se_aes_context->keyslot = 14;
		se_aes_context->total_size = header_info->validation_size -
			SIGNED_SECTION_LEN;

		pr_debug("Initializing AES context\n");
		err = tegrabl_crypto_init(TEGRABL_CRYPTO_AES,
				(union tegrabl_crypto_context *)crypto_aes_context);
		if (err != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(err);
			goto fail;
		}

		crypto_rsa_pss_context->key_size = RSA_2048_KEY_SIZE_BITS;
		crypto_rsa_pss_context->se_context.input_size =
			header_info->validation_size;
		crypto_rsa_pss_context->signature = (void *)header->signatures.rsapsssig;

		/* fixme - remove the need of filling this. Currently BR is
		 * clearing the oem rsa key slot. if it is set by mb1 based on op mode
		 * we need not write rsa key everytime
		 */
		pr_debug("Filling rsa key\n");
		crypto_rsa_pss_context->key =
									(uint32_t *)tegrabl_brbct_pubkey_rsa_get();

		pr_debug("Initializing RSA context\n");
		err = tegrabl_crypto_init(TEGRABL_CRYPTO_RSA_PSS,
				(union tegrabl_crypto_context *)crypto_rsa_pss_context);
		break;
	}
#if defined(CONFIG_ENABLE_ECDSA)
	case TEGRABL_SIGNINGTYPE_OEM_ECC:
	{
		crypto_ecdsa_context->se_context.input_size =
			header_info->validation_size;
		crypto_ecdsa_context->signature =
			(void *)&header->signatures.ecdsasig;

		/* fixme - use bct library instead of hard coding */
		pr_debug("Filling ECC key\n");
		crypto_ecdsa_context->key =
			(struct tegrabl_ecdsa_key *)BR_BCT_ECCPUBKEY_ADDRESS;
		pr_debug("Initializing ECDSA context\n");
		err = tegrabl_crypto_init(TEGRABL_CRYPTO_ECC,
				(union tegrabl_crypto_context *)crypto_ecdsa_context);

		break;
	}
#endif
	default:
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		break;
	}

fail:
	return err;
}

/**
 * @brief Applies se operations as per the information in header
 * on input buffer.
 *
 * @param header Information about header
 * @param buffer Buffer to be processed
 * @param buffer_size Size of the buffer.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
static tegrabl_error_t tegrabl_auth_subprocess(
		struct tegrabl_auth_header_info *header, void *buffer,
		uint32_t buffer_size, void *dest)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t *buf = (uint8_t *)buffer;
	bool has_aes = false;
	bool has_rsa = false;
	bool has_ecc = false;

	buffer_size = MIN(buffer_size,
			header->validation_size - header->processed_size);

	pr_debug("Processed size %d, validation size %d, buffer size %d",
			header->processed_size, header->validation_size, buffer_size);

	if (buffer_size == 0U) {
		pr_debug("All data processed for %d header\n",
				header->mode);
		goto fail;
	}

	has_rsa = (TEGRABL_SIGNINGTYPE_NVIDIA_RSA == header->mode) ||
			   (TEGRABL_SIGNINGTYPE_OEM_RSA == header->mode) ||
			   (TEGRABL_SIGNINGTYPE_OEM_RSA_SBK == header->mode);
	if (has_rsa) {
		struct tegrabl_crypto_rsa_pss_context *rsa_pss_context;
		rsa_pss_context = &header->rsa_pss_context;
		pr_debug("Applying sha algorithm on buffer of size %d @%p\n",
				buffer_size, buf);
		err = tegrabl_crypto_process_block(
				(union tegrabl_crypto_context *)rsa_pss_context, buf,
				buffer_size, dest);
		if (err != TEGRABL_NO_ERROR) {
			pr_debug("Failed sha processing\n");
			TEGRABL_SET_HIGHEST_MODULE(err);
			goto fail;
		}

		if (header->processed_size == 0U) {
			buf += SIGNED_SECTION_LEN;
			buffer_size -= SIGNED_SECTION_LEN;
			header->processed_size += SIGNED_SECTION_LEN;
		}
	}

	has_aes = (TEGRABL_SIGNINGTYPE_SBK == header->mode) ||
			   (TEGRABL_SIGNINGTYPE_NVIDIA_RSA == header->mode) ||
			   (TEGRABL_SIGNINGTYPE_OEM_RSA_SBK == header->mode);
	if (has_aes) {
		struct tegrabl_crypto_aes_context *aes_context = NULL;
		aes_context = &header->aes_context;

		pr_debug("Applying aes algorithm on buffer of size %d @%p\n",
				buffer_size, buf);

		err = tegrabl_crypto_process_block(
				(union tegrabl_crypto_context *)aes_context, buf,
				buffer_size, dest);
		if (err != TEGRABL_NO_ERROR) {
			pr_debug("Failed aes processing\n");
			TEGRABL_SET_HIGHEST_MODULE(err);
			goto fail;
		}
	}
#if defined(CONFIG_ENABLE_ECDSA)
	has_ecc = (TEGRABL_SIGNINGTYPE_OEM_ECC == header->mode);
	if (has_ecc) {
		struct tegrabl_crypto_ecdsa_context *ecdsa_context;
		ecdsa_context = &header->ecdsa_context;
		pr_debug("Applying sha algorithm on buffer of size %d @%p\n",
				buffer_size, buf);
		err = tegrabl_crypto_process_block(
			(union tegrabl_crypto_context *)ecdsa_context, buf,
				buffer_size, dest);
		if (err != TEGRABL_NO_ERROR) {
			pr_debug("Failed sha processing\n");
			TEGRABL_SET_HIGHEST_MODULE(err);
			goto fail;
		}

		if (header->processed_size == 0U) {
			buf += SIGNED_SECTION_LEN;
			buffer_size -= SIGNED_SECTION_LEN;
			header->processed_size += SIGNED_SECTION_LEN;
		}
	}
#endif
	/* unsupported header->mode error */
	if (!(has_aes || has_rsa || has_ecc)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	header->processed_size += buffer_size;

fail:
	return err;
}

#if defined(CONFIG_OS_IS_L4T)
static tegrabl_error_t tegrabl_decrypt_block(void *buffer, uint32_t buffer_size, uint8_t keyslot)
{
	tegrabl_error_t err;
	uint32_t fuse;

	pr_debug("%s: buffer=%p size=%u\n", __func__, buffer, buffer_size);
	err = tegrabl_fuse_read(FUSE_TYPE_BOOT_SECURITY_INFO, &fuse, sizeof(fuse));
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: failed to read fuse\n", __func__);
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	if (fuse != FUSE_BOOT_SECURITY_RSA_ENCRYPTION) {
		pr_warn("%s: fuse (0x%x) is not burnt to do encryption (0x%x); skip decryption.\n", __func__, fuse,
				FUSE_BOOT_SECURITY_RSA_ENCRYPTION);
		goto fail;
	}

	if (tegrabl_keyslot_check_if_key_is_nonzero(keyslot)) {
		pr_debug("keyslot %u is not zero; do decryption.\n", keyslot);
	} else {
		pr_warn("fuse is burnt to do encryption, but keyslot %u is zero; skip decryption.\n", keyslot);
		goto fail;
	}

	/* use tegrabl_cipher_binary() to decrypt buffer */
	err = tegrabl_cipher_binary(buffer, buffer_size, buffer, true);
	pr_debug("tegrabl_cipher_binary() returns %d\n", err);

fail:
	return err;
}
#endif	/* CONFIG_OS_IS_L4T */


tegrabl_error_t tegrabl_auth_process_block(struct tegrabl_auth_handle *auth,
		void *buffer, uint32_t buffer_size, bool check_header)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	void *dest_addr = NULL;
	void *safe_dest_location = NULL;
	uint32_t cur_header = 0;
	bool found_header = false;
	struct tegrabl_auth_header_info *header_info = NULL;
	uint32_t num_headers = 0;

	pr_debug("Processing block of size %d @%p\n", buffer_size, buffer);
	if ((auth == NULL) || (buffer == NULL) || (buffer_size == 0UL)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	dest_addr = auth->dest_location;
	safe_dest_location = auth->safe_dest_location;
	cur_header = auth->cur_header;
	header_info = &auth->headers[cur_header];
	num_headers = auth->num_headers;

	if ((dest_addr == NULL) || (safe_dest_location == NULL) ||
		(auth->dest_size < sizeof(struct tegrabl_sigheader))) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if (check_header) {
		pr_debug("Checking for header\n");

		if (buffer_size < sizeof(struct tegrabl_sigheader)) {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}

		if (tegrabl_auth_check_sigheader(buffer)) {
			found_header = true;

			if (num_headers >= 1U) {
				header_info++;
				cur_header++;
			}

			num_headers++;
			pr_debug("Found header no %d\n", num_headers);
			err = tegrabl_auth_process_header(auth, buffer,
					header_info);
			if (err != TEGRABL_NO_ERROR) {
				goto fail;
			}

			pr_debug("Done process header\n");

#if !defined(CONFIG_SIMULATION)
			if (header_info->binary_size < ONE_KB) {
				err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
				pr_error("binary size is less than 1kb\n");
				goto fail;
			}
#endif

			if (header_info->validation_size < FULL_BINARY_VERIFY_THRESHOLD &&
				header_info->binary_size > (buffer_size - HEADER_SIZE)) {
				memcpy(safe_dest_location, buffer, buffer_size);
				auth->remaining_size = header_info->binary_size -
					(buffer_size - HEADER_SIZE);
				auth->short_binary = true;
				goto done;
			}

			buffer = (void *)((uintptr_t)buffer + HEADER_SIZE -
							SIGNED_SECTION_LEN);
			buffer_size -= (HEADER_SIZE - SIGNED_SECTION_LEN);

			pr_debug("Applying auth subprocess\n");
			err = tegrabl_auth_subprocess(header_info, buffer,
					buffer_size, dest_addr);
			if (err != TEGRABL_NO_ERROR) {
				goto fail;
			}

			buffer_size = MIN(buffer_size, header_info->validation_size);
			auth->remaining_size = header_info->validation_size - buffer_size;

			buffer_size -= SIGNED_SECTION_LEN;
			buffer = (void *)((uintptr_t)buffer + SIGNED_SECTION_LEN);
			auth->processed_size = 0;
		} else if (num_headers == 0U) {
			pr_debug("No headers are found\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
			goto fail;
		} else {
			/* No Action Required */
		}
	}

	if (found_header) {
		if (safe_dest_location != buffer) {
			if ((buffer_size != 0U) &&
				(header_info->mode != TEGRABL_SIGNINGTYPE_NVIDIA_RSA) &&
				(header_info->mode != TEGRABL_SIGNINGTYPE_OEM_RSA_SBK)) {
				memcpy(safe_dest_location, buffer, buffer_size);
			}
		}
		goto done;
	}

	pr_debug("Applying operations in header %d on buffer @%p of size %d\n",
				cur_header + 1, buffer, buffer_size);

	/* this is needed for validation of small binaries like eks */
	/* todo : optimize and remove memcpy */
	if (header_info->mode == TEGRABL_SIGNINGTYPE_OEM_RSA_SBK) {
		memcpy(safe_dest_location, buffer, buffer_size);
	}

	if (!auth->short_binary) {
		err = tegrabl_auth_subprocess(header_info, buffer, buffer_size,
				safe_dest_location);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

	if (safe_dest_location != buffer && header_info->mode != TEGRABL_SIGNINGTYPE_OEM_RSA_SBK &&
		header_info->mode != TEGRABL_SIGNINGTYPE_NVIDIA_RSA) {
		pr_debug("Copying from %p to %p\n", buffer, safe_dest_location);
		memcpy(safe_dest_location, buffer, buffer_size);
	}

	if (auth->short_binary && (auth->remaining_size <= buffer_size)) {
		err = tegrabl_auth_subprocess(header_info,
				(void *)((uintptr_t)dest_addr + HEADER_SIZE -
					SIGNED_SECTION_LEN),
				auth->processed_size + buffer_size, dest_addr);

		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
		if (header_info->mode != TEGRABL_SIGNINGTYPE_NVIDIA_RSA &&
			header_info->mode != TEGRABL_SIGNINGTYPE_OEM_RSA_SBK) {
			memmove(dest_addr,
				(void *)((uintptr_t)dest_addr + HEADER_SIZE),
				header_info->binary_size);
		}
	}

	auth->remaining_size = auth->remaining_size - buffer_size;

done:
	auth->processed_size += buffer_size;
	auth->safe_dest_location = (void *)((uintptr_t)auth->dest_location +
			auth->processed_size);
	auth->cur_header = cur_header;
	auth->num_headers = num_headers;

	pr_debug("Remaining size %d\n", auth->remaining_size);
	pr_debug("Processed size %d\n", auth->processed_size);
	pr_debug("Safe destination location %p\n", auth->safe_dest_location);

fail:
	return err;
}

uint32_t tegrabl_auth_header_size(void)
{
	return HEADER_SIZE;
}

uint32_t tegrabl_auth_get_size_from_header(
		struct tegrabl_auth_handle *auth, uint32_t header_num)
{
	uint32_t size = 0;

	if (auth == NULL) {
		goto fail;
	}

	if (header_num > auth->num_headers) {
		goto fail;
	}

	size = auth->headers[header_num].binary_size;

fail:
	return size;
}

tegrabl_error_t tegrabl_auth_finalize(struct tegrabl_auth_handle *auth)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t i = 0;
	struct tegrabl_auth_header_info *headers = NULL;

	pr_debug("Verifying signature/hash of binary\n");
	if (auth == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	headers = auth->headers;
	for (i = 0; i < auth->num_headers; i++) {
		switch (headers[i].mode) {
		case TEGRABL_SIGNINGTYPE_NVIDIA_RSA:
		case TEGRABL_SIGNINGTYPE_OEM_RSA:
		case TEGRABL_SIGNINGTYPE_OEM_RSA_SBK:
		{
			struct tegrabl_crypto_rsa_pss_context *rsa_pss_context = NULL;
			auth->check_nvidia_header = (auth->check_nvidia_header == true) &&
										 !(headers[i].mode == TEGRABL_SIGNINGTYPE_NVIDIA_RSA);
			rsa_pss_context = &headers[i].rsa_pss_context;
			pr_debug("Finalizing sha operation\n");
			err = tegrabl_crypto_finalize(
					(union tegrabl_crypto_context *)rsa_pss_context);
			if (err != TEGRABL_NO_ERROR) {
				TEGRABL_SET_HIGHEST_MODULE(err);
				pr_error("SHA verification failed for header %d\n", i + 1U);
				goto fail;
			}
			break;
		}
#if defined(CONFIG_ENABLE_ECDSA)
		case TEGRABL_SIGNINGTYPE_OEM_ECC:
		{
			struct tegrabl_crypto_ecdsa_context *ecdsa_context = NULL;
			ecdsa_context = &headers[i].ecdsa_context;
			pr_debug("Finalizing sha operation\n");
			err = tegrabl_crypto_finalize(
				(union tegrabl_crypto_context *)ecdsa_context);
			if (err != TEGRABL_NO_ERROR) {
				TEGRABL_SET_HIGHEST_MODULE(err);
				pr_error("SHA verification failed for header %d\n", i + 1U);
				goto fail;
			}
			break;
		}
#endif
		case TEGRABL_SIGNINGTYPE_SBK:
		{
			struct tegrabl_crypto_aes_context *aes_context = NULL;
			aes_context = &headers[i].aes_context;
			pr_debug("Finalizing AES operation\n");
			err = tegrabl_crypto_finalize(
					(union tegrabl_crypto_context *)aes_context);
			if (err != TEGRABL_NO_ERROR) {
				TEGRABL_SET_HIGHEST_MODULE(err);
				pr_error("AES verification failed for header %d\n", i + 1U);
				goto fail;
			}
			break;
		}
		default:
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			break;
		}
	}

	if (auth->check_nvidia_header == true) {
		pr_debug("Binary is not Nvidia signed\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

fail:
	return err;
}

tegrabl_error_t tegrabl_auth_end(struct tegrabl_auth_handle *auth)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t i = 0;
	struct tegrabl_auth_header_info *headers = NULL;

	if (auth == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	pr_debug("Releasing resources\n");
	headers = auth->headers;

	for (i = 0; i < auth->num_headers; i++) {
		if (headers[i].mode == TEGRABL_SIGNINGTYPE_NVIDIA_RSA ||
			headers[i].mode == TEGRABL_SIGNINGTYPE_OEM_RSA ||
			headers[i].mode == TEGRABL_SIGNINGTYPE_OEM_RSA_SBK) {

			struct tegrabl_crypto_rsa_pss_context *rsa_pss_context = NULL;
			rsa_pss_context = &headers[i].rsa_pss_context;
			pr_debug("Releasing sha resources\n");
			err = tegrabl_crypto_close(
					(union tegrabl_crypto_context *)rsa_pss_context);
		}
		if (headers[i].mode == TEGRABL_SIGNINGTYPE_NVIDIA_RSA ||
			headers[i].mode == TEGRABL_SIGNINGTYPE_SBK ||
			headers[i].mode == TEGRABL_SIGNINGTYPE_OEM_RSA_SBK) {

			struct tegrabl_crypto_aes_context *aes_context = NULL;
			aes_context = &headers[i].aes_context;
			pr_debug("Releasing aes resources\n");
			err = tegrabl_crypto_close(
					(union tegrabl_crypto_context *)aes_context);
		}

#if defined(CONFIG_ENABLE_ECDSA)
		if (headers[i].mode == TEGRABL_SIGNINGTYPE_NVIDIA_RSA ||
			headers[i].mode == TEGRABL_SIGNINGTYPE_OEM_ECC) {

			struct tegrabl_crypto_ecdsa_context *ecdsa_context = NULL;
			ecdsa_context = &headers[i].ecdsa_context;
			pr_debug("Releasing sha resources\n");
			err = tegrabl_crypto_close(
				(union tegrabl_crypto_context *)ecdsa_context);
		}
#endif

	}

fail:
	return err;
}

tegrabl_error_t tegrabl_auth_set_new_destination(
		struct tegrabl_auth_handle *auth, void *new_addr, bool move)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if ((auth == NULL) || (new_addr == NULL)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}


	if (move) {
		memmove(new_addr, auth->dest_location, auth->processed_size);
		auth->safe_dest_location = (void *)((uintptr_t)new_addr +
				auth->processed_size);

	} else {
		auth->safe_dest_location = new_addr;
		auth->processed_size = 0;
	}

	auth->dest_location = new_addr;

fail:
	return err;
}

#if defined(CONFIG_ENABLE_SECURE_BOOT)
tegrabl_error_t tegrabl_auth_payload(tegrabl_binary_type_t bin_type,
			char *name, void *payload, uint32_t max_size)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_auth_handle auth = {0};
#if defined(CONFIG_OS_IS_L4T)
	uint32_t binary_len;
#endif

	pr_info("T18x: Authenticate %s (bin_type %u), max size 0x%x\n", name,
			bin_type, max_size);

	/* validate bin_type type */
	switch (bin_type) {
	case TEGRABL_BINARY_KERNEL:
	case TEGRABL_BINARY_KERNEL_DTB:
#if defined(CONFIG_ENABLE_L4T_RECOVERY)
	case TEGRABL_BINARY_RECOVERY_IMG:
	case TEGRABL_BINARY_RECOVERY_DTB:
#endif
		break;
	default:
		pr_info("Error: Unsupported partition %s (bin_type %d)\n", name,
				(int)bin_type);
		goto fail;
	}

#if defined(CONFIG_OS_IS_L4T)
	/* Save the binary length in the payload's header */
	binary_len = tegrabl_auth_get_binary_len(payload);
	if (binary_len == 0) {
		pr_error("binary has 0 length\n");
		err = TEGRABL_ERR_INVALID;
		goto fail;
	}
#endif	/* CONFIG_OS_IS_L4T */

	/* Initiate authentication of binary */
	err = tegrabl_auth_initiate(bin_type, payload, max_size, &auth);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	err = tegrabl_auth_process_block(&auth, payload, max_size, true);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	/* Verify signature/hash */
	err = tegrabl_auth_finalize(&auth);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

#if defined(CONFIG_OS_IS_L4T)
	/* Try to decrypt the buffer */
	/* Note: after tegrabl_auth_process_block(), payload is now pointed to the actual binary */
	pr_info("Decrypt the buffer ... ");
	err = tegrabl_decrypt_block(payload, binary_len, AES_KEYSLOT_SBK);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("\nFailed to decrypt the buffer (err=%u)\n", err);
	} else {
		pr_info("done\n");
	}
#endif	/* CONFIG_OS_IS_L4T */

fail:
	/* End of authentication process */
	tegrabl_auth_end(&auth);

	return err;
}

uint32_t tegrabl_sigheader_size(void)
{
	return sizeof(struct tegrabl_sigheader);
}

uint32_t tegrabl_auth_get_binary_len(void *bin_load_addr)
{
	uint32_t bin_len;
	struct tegrabl_sigheader *header = (struct tegrabl_sigheader *)bin_load_addr;

	if (tegrabl_auth_check_sigheader(bin_load_addr)) {
		pr_trace("Binary len: %u\n", header->binarylength);
		bin_len = header->binarylength;
	} else {

		bin_len = 0;
	}

	return bin_len;
}

/* Final step: clear the SBK keyslot */
tegrabl_error_t tegrabl_auth_complete(void)
{
	tegrabl_error_t err;

	err = tegrabl_se_clear_aes_keyslot(AES_KEYSLOT_SBK);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: failed to clear keyslot %u (err=%d)\n", __func__, AES_KEYSLOT_SBK, err);
	}

	return err;
}

#endif /* defined(CONFIG_ENABLE_SECURE_BOOT) */
