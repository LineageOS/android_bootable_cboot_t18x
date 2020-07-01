/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

#ifndef TEGRABL_SE_H
#define TEGRABL_SE_H

#include <stdint.h>
#include <tegrabl_error.h>

#define SE_AES_BLOCK_LENGTH	16
#define SELECT_EXPONENT 0
#define SELECT_MODULUS 1

/*
 * @brief Defines AES operating modes
 */
enum {
	SE_OPR_MODE_AES_CBC,
	SE_OPR_MODE_AES_CMAC_HASH,
	SE_OPR_MODE_MAX,
};

/*
 * @brief Defines SHA modes
 */
enum {
	SE_SHAMODE_SHA1,
	SE_SHAMODE_SHA224 = 4,
	SE_SHAMODE_SHA256,
	SE_SHAMODE_SHA384,
	SE_SHAMODE_SHA512,
};

/*
 * @brief params for SHA process block operation
 */
struct se_sha_input_params {
	uintptr_t block_addr;
	uint32_t block_size;
	uint32_t size_left;
	uintptr_t hash_addr;
};

/*
 * @brief context returned by SHA init
 */
struct se_sha_context {
	uint32_t input_size;
	uint8_t hash_algorithm;
};

/*
 * @brief Context returned by AES init operation
 */
struct se_aes_context {
	bool is_decrypt;
	bool is_encrypt;
	bool is_hash;
	uint32_t total_size;
	uint8_t keyslot;
	uint8_t keysize;
	uint8_t *iv_encrypt;
	uint8_t *phash;
	uint8_t *pk1;
	uint8_t *pk2;
	uint8_t *pkey;
};

/*
 * @brief Input params for AES process block operation
 */
struct se_aes_input_params {
	uint8_t *src;
	uint8_t *dst;
	uint32_t input_size;
	uint32_t size_left;
};

#if defined(CONFIG_ENABLE_SE)
/*
 * @brief writes given key into specified rsa keyslot
 *
 * @param pkey pointer to public/private key or modulus
 * @param rsa_key_size_bits length of the key in bits
 * @param rsa_keyslot rsa keyslot to be used
 * @param exp_mod_sel use SELECT_EXPONENT to write exponent or
 *			SELECT_MODULUS to write modulus.
 *
 * @return error out if any
 */
tegrabl_error_t tegrabl_se_rsa_write_key(
	uint32_t *pkey, uint32_t rsa_key_size_bits,
	uint8_t rsa_keyslot, uint8_t exp_mod_sel);

/*
 * @brief perform rsa modular exponentiation
 *
 * @param rsa_keyslot rsa keyslot to be used
 * @param rsa_key_size_bits length of the public/private in bits
 * @param rsa_expsize_bits length of the exponent in bits
 * @param pinput_message pointer to input data to be signed
 * @param poutput_destination pointer to output destination
 *
 * @return error out if any
 */
tegrabl_error_t tegrabl_se_rsa_modular_exp(
	uint8_t rsa_keyslot, uint32_t rsa_key_size_bits,
	uint32_t rsa_expsize_bits,
	uint8_t *pinput_message, uint8_t *poutput_destination);


/*
 * @brief perform SHA hash on given input
 *
 * @param input_params structure se_sha_input_params
 * @param context structure se_sha_context
 *
 * @return error out if any
 */
tegrabl_error_t tegrabl_se_sha_process_block(
	struct se_sha_input_params *input_params,
	struct se_sha_context *context);

/*
 * @brief WAR to perform SHA hash if input size is larger than 16MB
 *
 * @param input structure se_sha_input_params
 * @param context structure se_sha_context
 *
 * @return error out if any
 */
tegrabl_error_t tegrabl_se_sha_process_payload(
	struct se_sha_input_params *input_params,
	struct se_sha_context *context);

/*
 * @brief dummy function
 */
void tegrabl_se_sha_close(void);

/*
 * @brief Initialise SE clocks and reset
 */
void tegrabl_se_init(void);

/*
 * @brief Perform specified AES operation on input data
 *
 * @param input_params structure se_aes_input_params
 * @param context pointer to structure se_aes_context
 *
 * @return error out if any
 */
tegrabl_error_t tegrabl_se_aes_process_block(
	struct se_aes_input_params *input_params,
	struct se_aes_context *context);

/*
 * @brief dummy function
 */
void tegrabl_se_aes_close(void);

/*
 * @brief Perform rsa-pss verification when sha hash is provided
 *
 * @param rsa_keyslot rsa keyslot to be used
 * @param rsa_keysize_bits length of the key in bits
 * @param pmessage_hash pointer to SHA256 hash on input message
 * @param pinput_signature pointer to input signature to be verified
 * @param hash_algorithm SHA algorithm to be used
 * @param slen length of the input signature
 *
 * @return error out if any
 */
tegrabl_error_t tegrabl_se_rsa_pss_verify(
	uint8_t rsa_keyslot, uint32_t rsa_keysize_bits, uint8_t *pmessage_hash,
	uint32_t *pinput_signature, uint8_t hash_algorithm, int8_t slen);

/*
 * @brief Perform rsa-pss verification when sha hash is not provided
 *	but input message is provided
 *
 * @param rsa_keyslot rsa keyslot to be used
 * @param rsa_keysize_bits length of the key in bits
 * @param pinput_signature pointer to input signature to be verified
 * @param hash_algorithm SHA algorithm to be used
 * @param slen length of the input signature
 * @param pinput_message pointer to input message to be hashed
 * @param input_message_length_bytes length of input message
 *
 * @return error out if any
 */
tegrabl_error_t tegrabl_se_hash_and_rsa_pss_verify(
	uint8_t rsa_keyslot, uint32_t rsa_keysize_bits,
	uint32_t *pinput_signature, uint8_t hash_algorithm, int8_t slen,
	uint32_t *pinput_message, uint32_t input_message_length_bytes);

/*
 * @brief Perform se key slot clearing
 *
 * @param keyslot keyslot to be cleared
 * @return TEGRABL_NO_ERROR if success, specific error if fails
 */
tegrabl_error_t tegrabl_se_clear_aes_keyslot(
	uint8_t keyslot);

/*
 * @brief Perform pka1 key slot write operations
 *
 * @param keyslot keyslot to be written
 * @param data data to be written to key slot
 */
void tegrabl_pka1_write_keyslot(uint32_t *data, uint32_t keyslot);

#else

static inline tegrabl_error_t tegrabl_se_rsa_write_key(
	uint32_t *pkey, uint32_t rsa_key_size_bits,
	uint8_t rsa_keyslot, uint8_t exp_mod_sel)
{
	TEGRABL_UNUSED(pkey);
	TEGRABL_UNUSED(rsa_key_size_bits);
	TEGRABL_UNUSED(rsa_keyslot);
	TEGRABL_UNUSED(exp_mod_sel);

	return TEGRABL_NO_ERROR;
}

static inline tegrabl_error_t tegrabl_se_rsa_modular_exp(
	uint8_t rsa_keyslot, uint32_t rsa_key_size_bits,
	uint32_t rsa_expsize_bits,
	uint8_t *pinput_message, uint8_t *poutput_destination)
{
	TEGRABL_UNUSED(rsa_keyslot);
	TEGRABL_UNUSED(rsa_key_size_bits);
	TEGRABL_UNUSED(rsa_expsize_bits);
	TEGRABL_UNUSED(pinput_message);
	TEGRABL_UNUSED(poutput_destination);

	return TEGRABL_NO_ERROR;
}

static inline tegrabl_error_t tegrabl_se_sha_process_block(
	struct se_sha_input_params *input_params,
	struct se_sha_context *context)
{
	TEGRABL_UNUSED(input_params);
	TEGRABL_UNUSED(context);

	return TEGRABL_NO_ERROR;
}

static inline void tegrabl_se_sha_close(void)
{
}

static inline void tegrabl_se_init(void)
{
}

static inline tegrabl_error_t tegrabl_se_aes_process_block(
	struct se_aes_input_params *input_params,
	struct se_aes_context *context)
{
	TEGRABL_UNUSED(input_params);
	TEGRABL_UNUSED(context);

	return TEGRABL_NO_ERROR;
}

static inline void tegrabl_se_aes_close(void)
{
}

static inline tegrabl_error_t tegrabl_se_rsa_pss_verify(
	uint8_t rsa_keyslot, uint32_t rsa_keysize_bits, uint8_t *pmessage_hash,
	uint32_t *pinput_signature, uint8_t hash_algorithm, int8_t slen)
{
	TEGRABL_UNUSED(rsa_keyslot);
	TEGRABL_UNUSED(rsa_keysize_bits);
	TEGRABL_UNUSED(pmessage_hash);
	TEGRABL_UNUSED(pinput_signature);
	TEGRABL_UNUSED(hash_algorithm);
	TEGRABL_UNUSED(slen);

	return TEGRABL_NO_ERROR;
}

static inline tegrabl_error_t tegrabl_se_hash_and_rsa_pss_verify(
	uint8_t rsa_keyslot, uint32_t rsa_keysize_bits,
	uint32_t *pinput_signature, uint8_t hash_algorithm, int8_t slen,
	uint32_t *pinput_message, uint32_t input_message_length_bytes)
{
	TEGRABL_UNUSED(rsa_keyslot);
	TEGRABL_UNUSED(rsa_keysize_bits);
	TEGRABL_UNUSED(pinput_signature);
	TEGRABL_UNUSED(hash_algorithm);
	TEGRABL_UNUSED(slen);
	TEGRABL_UNUSED(pinput_message);
	TEGRABL_UNUSED(input_message_length_bytes);

	return TEGRABL_NO_ERROR;
}

static inline tegrabl_error_t tegrabl_se_clear_aes_keyslot(uint8_t keyslot)
{
	TEGRABL_UNUSED(keyslot);

	return TEGRABL_NO_ERROR;
}

static inline void tegrabl_pka1_write_keyslot(uint32_t *data, uint32_t keyslot)
{
	TEGRABL_UNUSED(data);
	TEGRABL_UNUSED(keyslot);
}
#endif /* CONFIG_ENABLE_SE */

#endif
