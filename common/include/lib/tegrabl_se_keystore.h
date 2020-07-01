/*
 * Copyright (c) 2015-2016, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef TEGRABL_SE_KEYSTORE_H
#define TEGRABL_SE_KEYSTORE_H

#include <stddef.h>
#include <stdint.h>
#include <tegrabl_error.h>

#define RSA_2048_KEY_SIZE	256

/*
 * @brief structure which holds public key store
 * which is passed-on by bootrom.
 */
struct tegrabl_pubkey {
	uint32_t bpmp_fw_pub_rsa_key[RSA_2048_KEY_SIZE / 4];
	uint32_t spe_fw_pub_rsa_key[RSA_2048_KEY_SIZE / 4];
	uint32_t ape_fw_pub_rsa_key[RSA_2048_KEY_SIZE / 4];
	uint32_t scecpe_fw_pub_rsa_key[RSA_2048_KEY_SIZE / 4];
	uint32_t mts_pub_rsa_key[RSA_2048_KEY_SIZE / 4];
	uint32_t mb1_pub_rsa_key[RSA_2048_KEY_SIZE / 4];
};

/*
 * @brief relocates public keystore form SYSRAM to SDRAM
 *
 * @param pubkey pointer which holds the SYSRAM public keystore structure
 * It also contains SDRAM public keystore structure after relocation.
 *
 * @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_keystore_relocate_to_sdram(
	struct tegrabl_pubkey **pubkey);

/**
 * @brief Initialize Public Keystore
 *
 * @param load_address Address where Public Keystore is loaded.
 *
 * @return TEGRABL_NO_ERROR in case of success, TEGRABL_ERR_INVALID if
 * load_address is NULL
 */
tegrabl_error_t tegrabl_keystore_init(void *load_address);

/**
 * @brief Get the Public Keystore pointer
 *
 * @return Pointer to Public Keystore, NULL if Public Keystore not initialized
 */
struct tegrabl_pubkey *tegrabl_keystore_get(void);

#endif /* TEGRABL_SE_KEYSTORE_H */

