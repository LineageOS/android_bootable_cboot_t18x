/*
 * Copyright (c) 2015-2019, NVIDIA CORPORATION.  All Rights Reserved.
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
#include <tegrabl_addressmap.h>
#include <tegrabl_drf.h>
#include <tegrabl_se.h>
#include <tegrabl_debug.h>
#include <tegrabl_io.h>
#include <tegrabl_malloc.h>
#include <tegrabl_se_defs.h>
#include <tegrabl_se_helper.h>
#include <tegrabl_dmamap.h>
#include <tegrabl_timer.h>
#include <arse0.h>

#define SUBKEY_CACHE_SIZE 2U
#define SHA_INPUT_BLOCK_SZ (8UL * 1024UL * 1024UL)
#define TEGRABL_CRYPTO_SHA_MIN_BUF_SIZE 64
#define SE_SHA_MAX_INPUT_SIZE	(ROUND_DOWN_POW2( \
			SE0_SHA_OUT_ADDR_HI_0_SZ_FIELD, \
			TEGRABL_CRYPTO_SHA_MIN_BUF_SIZE))
#define SE_AES_MAX_INPUT_SIZE	(SE_AES_BLOCK_LENGTH * \
		SE0_AES0_CRYPTO_LAST_BLOCK_0_WRITE_MASK)

/**
 * @brief Defines the aux info enums for keyslot errors
 */
#define AUX_INFO_INVALID_AES_KEYSLOT 0x30
#define AUX_INFO_INVALID_RSA_KEYSLOT 0x31

/**
 * @brief Defines the structure to store the subkeys
 * for a slot.
 *
 * @var keyslot used to specify AES keyslot
 * @var pk1 used to store CMAC subkey k1
 * @var Pk2 used to store CMAC subkey k2
 */
struct tegrabl_se_aes_subkey_cache {
	uint32_t keyslot;
	uint8_t pk1[SE_AES_BLOCK_LENGTH];
	uint8_t pk2[SE_AES_BLOCK_LENGTH];
};

static struct tegrabl_se_aes_subkey_cache aes_subkeys[SUBKEY_CACHE_SIZE];

static inline bool tegrabl_se_is_dst_valid(uint8_t dst)
{
	if ((dst == SE0_AES0_CONFIG_0_DST_MEMORY)   ||
		(dst == SE0_AES0_CONFIG_0_DST_HASH_REG) ||
		(dst == SE0_AES0_CONFIG_0_DST_KEYTABLE) ||
		(dst == SE0_AES0_CONFIG_0_DST_SRK)      ||
		(dst == SE0_AES0_CONFIG_0_DST_RSA_REG)) {

		return true;
	}

	return false;
}

/* Reads SE0 register */
static uint32_t tegrabl_get_se0_reg(uint32_t reg)
{
	return NV_READ32((uint32_t)NV_ADDRESS_MAP_SE0_BASE + reg);
}

/* Writes data into SE0 register */
static void tegrabl_set_se0_reg(uint32_t reg, uint32_t data)
{
	NV_WRITE32((uint32_t)NV_ADDRESS_MAP_SE0_BASE + reg, data);
}

/* Acquire SE0 h/w mutex */
static void tegrabl_get_se0_mutex(void)
{
	uint32_t se_config_reg;
	uint32_t status = SE0_MUTEX_REQUEST_RELEASE_0_RESET_VAL;

	while(status != SE0_MUTEX_REQUEST_RELEASE_0_LOCK_TRUE) {
		se_config_reg = tegrabl_get_se0_reg(SE0_MUTEX_REQUEST_RELEASE_0);
		status = NV_DRF_VAL(SE0_MUTEX, REQUEST_RELEASE, LOCK, se_config_reg);
	}
}

/* Release SE0 h/w mutex */
static void tegrabl_release_se0_mutex(void)
{
	tegrabl_set_se0_reg(SE0_MUTEX_REQUEST_RELEASE_0,
						SE0_MUTEX_REQUEST_RELEASE_0_LOCK_TRUE);
}

/* Start specific SE0 operation */
static tegrabl_error_t tegrabl_start_se0_operation(
	uint8_t se_engine_index, bool last_buf)
{
	uint32_t se_config_reg = 0;
	uint32_t last_buf32;
	/*Type casting the variable last_buf*/
	last_buf32 = (last_buf) ? 1UL : 0UL;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	se_config_reg |= (last_buf32 << SE_UNIT_OPERATION_PKT_LASTBUF_SHIFT) &
		((uint32_t)SE_UNIT_OPERATION_PKT_LASTBUF_FIELD);
	se_config_reg |= SE_UNIT_OPERATION_PKT_OP_START <<
		SE_UNIT_OPERATION_PKT_OP_SHIFT;

	switch (se_engine_index) {
	case ARSE_ENG_IDX_AES0:
		tegrabl_set_se0_reg(SE0_AES0_OPERATION_0, se_config_reg);
		break;
	case ARSE_ENG_IDX_SHA:
		tegrabl_set_se0_reg(SE0_SHA_OPERATION_0, se_config_reg);
		break;
	case ARSE_ENG_IDX_PKA0:
		tegrabl_set_se0_reg(SE0_RSA_OPERATION_0, se_config_reg);
		break;
	default:
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		break;
	}

	if (err != TEGRABL_NO_ERROR) {
		pr_debug("Error = %d in tegrabl_start_se0_operation\n", err);
	}
	return err;
}

/* Check if SE0 engine status is Busy or not */
static tegrabl_error_t tegrabl_is_se0_engine_busy(
	uint8_t se_engine_index, bool *engine_status)
{
	uint32_t se_config_offset = 0;
	uint32_t se_config_reg = 0;
	uint8_t status = 0;
	bool flag = false;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (engine_status == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	switch (se_engine_index) {
	case ARSE_ENG_IDX_AES0:
		se_config_offset = SE0_AES0_STATUS_0;
		break;
	case ARSE_ENG_IDX_SHA:
		se_config_offset = SE0_SHA_STATUS_0;
		break;
	case ARSE_ENG_IDX_PKA0:
		se_config_offset = SE0_RSA_STATUS_0;
		break;
	default:
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		flag = true;
		break;
	}
	if (flag) {
		goto fail;
	}

	se_config_reg = tegrabl_get_se0_reg(se_config_offset);

	/* SE0_*_STATUS_0_STATE_* are the same for each STATUS register,
	 * using AES0 status' state.
	 */
	status = NV_DRF_VAL(SE0_AES0, STATUS, STATE, se_config_reg);
	if (status == SE0_OP_STATUS_BUSY) {
		*engine_status = true;
	} else {
		*engine_status = false;
	}
fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("Error = %d, in tegrabl_is_se0_engine_busy\n", err);
	}
	return err;
}

/* Convert given rsa keysize into the way RSA engine expects */
static uint32_t tegrabl_convert_rsa_keysize(uint32_t rsa_keysize_bits)
{
	if (rsa_keysize_bits == 2048UL) {
		return  SE0_RSA_KEY_SIZE_0_VAL_WIDTH_2048;
	} else if (rsa_keysize_bits == 1536UL) {
		return  SE0_RSA_KEY_SIZE_0_VAL_WIDTH_1536;
	} else if (rsa_keysize_bits == 1024UL) {
		return  SE0_RSA_KEY_SIZE_0_VAL_WIDTH_1024;
	} else if (rsa_keysize_bits == 512UL) {
		return SE0_RSA_KEY_SIZE_0_VAL_WIDTH_512;
	} else {
		return 0;
	}
}

tegrabl_error_t tegrabl_se_rsa_write_key(
	uint32_t *pkey, uint32_t rsa_key_size_bits,
	uint8_t rsa_keyslot, uint8_t exp_mod_sel)
{
	uint32_t rsa_keytable_addr = 0;
	uint32_t rsa_keypkt = 0;
	uint32_t i = 0;
	uint32_t keypkt = (uint32_t)SE_RSA_KEY_PKT_WORD_ADDR_FIELD;
	uint32_t keytable = (uint32_t)SE0_RSA_KEYTABLE_ADDR_0_PKT_FIELD;

	if (rsa_keyslot >= SE_RSA_MAX_KEYSLOTS) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}
	if (rsa_key_size_bits > RSA_MAX_EXPONENT_SIZE_BITS) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	tegrabl_get_se0_mutex();

	/* Create SE_RSA_KEY_PKT
	 * Set EXPMOD_SEL to either MODULUS or EXPONENT
	 * Set WORD_ADDR to one of the 64 exponent/modulus words for access
	 * INPUT_MODE field is removed for T18x.
	 */
	rsa_keypkt = ( \
			((uint32_t)rsa_keyslot << SE_RSA_KEY_PKT_KEY_SLOT_SHIFT) | \
			((uint32_t)exp_mod_sel << SE_RSA_KEY_PKT_EXPMOD_SEL_SHIFT) );

	rsa_keytable_addr |= rsa_keypkt;

	for (i = 0; i < (rsa_key_size_bits / 32UL); i++) {
		/* Set WORD_ADDR field */
		rsa_keypkt &= ~(keypkt);
		rsa_keypkt |= (i << SE_RSA_KEY_PKT_WORD_ADDR_SHIFT);

		rsa_keytable_addr &= ~(keytable);
		rsa_keytable_addr |= rsa_keypkt;

		/* Start Rsa key slot write. */
		tegrabl_set_se0_reg(SE0_RSA_KEYTABLE_ADDR_0, rsa_keytable_addr);

		tegrabl_set_se0_reg(SE0_RSA_KEYTABLE_DATA_0, (pkey == NULL) ? 0UL : pkey[i]);
	}

	tegrabl_release_se0_mutex();
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_se_rsa_modular_exp(
	uint8_t rsa_keyslot, uint32_t rsa_key_size_bits,
	uint32_t rsa_expsize_bits,
	uint8_t *pinput_message, uint8_t *poutput_destination)
{
	uint32_t se_config_reg = 0;
	dma_addr_t dma_input_message_addr = 0;
	dma_addr_t dma_output_destination = 0;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	bool engine_busy = true;

	if ((pinput_message == NULL) || (poutput_destination == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (rsa_keyslot >= SE_RSA_MAX_KEYSLOTS) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if ((rsa_key_size_bits > RSA_MAX_EXPONENT_SIZE_BITS) ||
		(rsa_expsize_bits > RSA_MAX_EXPONENT_SIZE_BITS)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	tegrabl_get_se0_mutex();

	/* Program size of key to be written into SE_RSA_EXP_SIZE or SE_RSA_KEY_SIZE
	 * SE_RSA_*_SIZE is specified in units of 4-byte / 32-bits blocks.
	 */
	tegrabl_set_se0_reg(SE0_RSA_EXP_SIZE_0, rsa_expsize_bits / 32UL);
	tegrabl_set_se0_reg(SE0_RSA_KEY_SIZE_0, tegrabl_convert_rsa_keysize(
						rsa_key_size_bits));

	dma_input_message_addr = tegrabl_dma_map_buffer(TEGRABL_MODULE_SE,
		0, (void *)pinput_message, rsa_key_size_bits / 8UL,
		TEGRABL_DMA_TO_DEVICE);
	/* Program RSA task input message. */
	tegrabl_set_se0_reg(SE0_RSA_IN_ADDR_0, (uint32_t)dma_input_message_addr);

	/* Program input message buffer size into SE0_RSA_IN_ADDR_HI_0_SZ and
	 * 8-bit MSB of 40b dma addr into MSB field.
	 */
	se_config_reg = 0;
	se_config_reg = NV_FLD_SET_DRF_NUM(SE0_RSA, IN_ADDR_HI, SZ,
									   rsa_key_size_bits / 8UL, se_config_reg);
	se_config_reg = NV_FLD_SET_DRF_NUM(SE0_RSA, IN_ADDR_HI, MSB,
		(dma_input_message_addr >> 32), se_config_reg);
	tegrabl_set_se0_reg(SE0_RSA_IN_ADDR_HI_0, se_config_reg);


	/**
	 * 1. Set SE to RSA modular exponentiation operation
	 * SE0_RSA_CONFIG.DEC_ALG = NOP
	 * SE0_RSA_CONFIG.ENC_ALG = RSA
	 * SE0_RSA_CONFIG.DEC_MODE = DEFAULT (use SE_MODE_PKT)
	 * SE0_RSA_CONFIG.ENC_MODE = Don't care (using SW_DEFAULT value)
	 * SE0_RSA_CONFIG.DST = RSA_REG or MEMORY depending on OutputDestination
	 *
	 */
	/* Can initialize this to zero because we touch all of the fields in
	 * SE_CONFIG
	 */
	se_config_reg = 0;
	se_config_reg = NV_FLD_SET_DRF_DEF(SE0, RSA_CONFIG, DST, MEMORY, se_config_reg);

	dma_output_destination = tegrabl_dma_map_buffer(TEGRABL_MODULE_SE, 0,
											(void *)poutput_destination,
											rsa_key_size_bits / 8UL,
											TEGRABL_DMA_FROM_DEVICE);
	/* Program RSA task output address. */
	tegrabl_set_se0_reg(SE0_RSA_OUT_ADDR_0, (uint32_t)dma_output_destination);
	/* Program input message buffer size into SE0_RSA_IN_ADDR_HI_0_SZ and
	 * 8-bit MSB of 40b dma addr into MSB field.
	 */
	se_config_reg = 0;
	se_config_reg = NV_FLD_SET_DRF_NUM(SE0_RSA, OUT_ADDR_HI, SZ, rsa_key_size_bits / 8UL, se_config_reg);
	se_config_reg = NV_FLD_SET_DRF_NUM(SE0_RSA, OUT_ADDR_HI, MSB,
		(dma_output_destination >> 32), se_config_reg);
	tegrabl_set_se0_reg(SE0_RSA_OUT_ADDR_HI_0, se_config_reg);

	se_config_reg = NV_FLD_SET_DRF_DEF(SE0, RSA_CONFIG, DEC_ALG, NOP, se_config_reg);
	se_config_reg = NV_FLD_SET_DRF_DEF(SE0, RSA_CONFIG, ENC_ALG, RSA, se_config_reg);
	se_config_reg = NV_FLD_SET_DRF_DEF(SE0, RSA_CONFIG, DEC_MODE, DEFAULT, se_config_reg);
	se_config_reg = NV_FLD_SET_DRF_DEF(SE0, RSA_CONFIG, ENC_MODE, DEFAULT, se_config_reg);

	/* Write to SE_RSA_CONFIG register */
	tegrabl_set_se0_reg(SE0_RSA_CONFIG_0, se_config_reg);

	/*
	 * Set key slot number
	 * SE_RSA_TASK_CONFIG Fields:
	 * KEY_SLOT = rsa_keyslot
	 *
	 */
	se_config_reg = 0;
	se_config_reg = NV_FLD_SET_DRF_NUM(SE0, RSA_TASK_CONFIG, KEY_SLOT,
									   rsa_keyslot, se_config_reg);
	tegrabl_set_se0_reg(SE0_RSA_TASK_CONFIG_0, se_config_reg);

	/**
	 * Issue operation start.
	 */
	err = tegrabl_start_se0_operation(ARSE_ENG_IDX_PKA0, true);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	/* Poll for BUSY */
	while (engine_busy) {
		err = tegrabl_is_se0_engine_busy(ARSE_ENG_IDX_PKA0, &engine_busy);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

fail:
	/* Unmap DMA buffers */
	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SE,
							 0, (void *)pinput_message, rsa_key_size_bits / 8UL,
							 TEGRABL_DMA_TO_DEVICE);

	if (poutput_destination != NULL) {
		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SE,
								 0, (void *)poutput_destination,
								 rsa_key_size_bits / 8UL,
								 TEGRABL_DMA_FROM_DEVICE);
	}

	tegrabl_release_se0_mutex();
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("Error = %d in tegrabl_se_rsa_modular_exp\n", err);
	}

	return err;
}

static tegrabl_error_t _tegrabl_se_sha_process_block(
	struct se_sha_input_params *input_params,
	struct se_sha_context *context)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	uint32_t se_config_reg = 0;
	uint32_t size_left = 0;
	uint32_t hash_size = 0;
	uintptr_t phash_result = 0;
	uint32_t block_size = 0;
	uint32_t input_size_bits0 = 0;
	uint32_t input_size_bits1 = 0;
	uint32_t hash_algorithm = 0;
	uint64_t input_size_bits = 0;
	uint64_t size_left_bits = 0;
	uint32_t size_left_bits0 = 0;
	uint32_t size_left_bits1 = 0;
	uintptr_t block_addr = 0;
	dma_addr_t dma_block_addr = 0;
	dma_addr_t dma_hash_result = 0;
	bool engine_busy = true;
	bool flag = false;

	if ((input_params == NULL) || (context == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	size_left = input_params->size_left;
	phash_result = input_params->hash_addr;
	size_left_bits = size_left * 8ULL;
	size_left_bits0 = (uint32_t)size_left_bits;
	size_left_bits1 = (uint32_t)(size_left_bits >> 32);
	input_size_bits = (uint64_t)context->input_size * 8U;
	input_size_bits0 = (uint32_t)input_size_bits;
	input_size_bits1 = (uint32_t)(input_size_bits >> 32);
	hash_algorithm = context->hash_algorithm;
	block_size = input_params->block_size;
	block_addr = input_params->block_addr;

	if ((input_size_bits == 0UL) || (phash_result == 0UL) ||
		(block_size == 0UL) || (block_addr == 0UL) ||
		(block_size > SE0_SHA_IN_ADDR_HI_0_SZ_FIELD)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	/* Calculate output size */
	switch (hash_algorithm) {
	case SE_MODE_PKT_SHAMODE_SHA1:
		hash_size = ARSE_SHA1_HASH_SIZE / 8;
		break;
	case SE_MODE_PKT_SHAMODE_SHA224:
		hash_size = ARSE_SHA224_HASH_SIZE / 8;
		break;
	case SE_MODE_PKT_SHAMODE_SHA256:
		hash_size = ARSE_SHA256_HASH_SIZE / 8;
		break;
	case SE_MODE_PKT_SHAMODE_SHA384:
		hash_size = ARSE_SHA384_HASH_SIZE / 8;
		break;
	case SE_MODE_PKT_SHAMODE_SHA512:
		hash_size = ARSE_SHA512_HASH_SIZE / 8;
		break;
	default:
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		flag = true;
		break;
	}
	if (flag) {
		goto fail;
	}

	tegrabl_get_se0_mutex();

	se_config_reg = NV_FLD_SET_DRF_NUM(
		SE0, SHA_CONFIG, ENC_MODE, hash_algorithm, se_config_reg);

	se_config_reg = NV_FLD_SET_DRF_DEF(SE0, SHA_CONFIG, DST, HASH_REG,
		se_config_reg);
	se_config_reg = NV_FLD_SET_DRF_DEF(SE0, SHA_CONFIG, DEC_ALG, NOP,
		se_config_reg);
	se_config_reg = NV_FLD_SET_DRF_DEF(SE0, SHA_CONFIG, ENC_ALG, SHA,
		se_config_reg);

	/* Write to SE0_SHA_CONFIG register. */
	tegrabl_set_se0_reg(SE0_SHA_CONFIG_0, se_config_reg);

	/* Set up total message length (SE_SHA_MSG_LENGTH is specified in bits). */
	tegrabl_set_se0_reg(SE0_SHA_MSG_LENGTH_0, input_size_bits0);
	tegrabl_set_se0_reg(SE0_SHA_MSG_LENGTH_1, input_size_bits1);
	/* Zero out MSG_LENGTH2-3 and MSG_LEFT2-3, since the maximum size */
	/* handled by the BL is <= 4GB. */
	tegrabl_set_se0_reg(SE0_SHA_MSG_LENGTH_2, 0);
	tegrabl_set_se0_reg(SE0_SHA_MSG_LENGTH_3, 0);

	tegrabl_set_se0_reg(SE0_SHA_MSG_LEFT_0, size_left_bits0);
	tegrabl_set_se0_reg(SE0_SHA_MSG_LEFT_1, size_left_bits1);
	tegrabl_set_se0_reg(SE0_SHA_MSG_LEFT_2, 0);
	tegrabl_set_se0_reg(SE0_SHA_MSG_LEFT_3, 0);

	if (context->input_size == size_left) {
		se_config_reg = NV_DRF_DEF(SE0, SHA_TASK_CONFIG, HW_INIT_HASH, ENABLE);
	}
	else {
		se_config_reg = NV_DRF_DEF(SE0, SHA_TASK_CONFIG, HW_INIT_HASH, DISABLE);
	}

	tegrabl_set_se0_reg(SE0_SHA_TASK_CONFIG_0, se_config_reg);

	dma_block_addr = tegrabl_dma_map_buffer(TEGRABL_MODULE_SE,
		0, (void *)block_addr, block_size, TEGRABL_DMA_TO_DEVICE);
	/* Program input address and HI register. */
	tegrabl_set_se0_reg(SE0_SHA_IN_ADDR_0, (uint32_t)dma_block_addr);

	/* Program input message buffer size into SE0_SHA_IN_ADDR_HI_0_SZ and
	 * 8-bit MSB of 40b dma addr into MSB field.
	 */
	se_config_reg = 0;
	se_config_reg = NV_FLD_SET_DRF_NUM(SE0_SHA, IN_ADDR_HI, SZ,
									   block_size, se_config_reg);
	se_config_reg = NV_FLD_SET_DRF_NUM(SE0_SHA, IN_ADDR_HI, MSB,
		(dma_block_addr >> 32), se_config_reg);
	tegrabl_set_se0_reg(SE0_SHA_IN_ADDR_HI_0, se_config_reg);

	se_config_reg = tegrabl_get_se0_reg(SE0_SHA_CONFIG_0);

	se_config_reg = NV_FLD_SET_DRF_DEF(SE0, SHA_CONFIG, DST, MEMORY,
									   se_config_reg);
	tegrabl_set_se0_reg(SE0_SHA_CONFIG_0, se_config_reg);

	dma_hash_result = tegrabl_dma_map_buffer(TEGRABL_MODULE_SE,
		0, (void *)phash_result, hash_size,
		TEGRABL_DMA_FROM_DEVICE);
	tegrabl_set_se0_reg(SE0_SHA_OUT_ADDR_0, (uint32_t )dma_hash_result);
	/* Program output message buffer size into SE0_SHA_OUT_ADDR_HI_0_SZ and
	 * 8-bit MSB of 40b dma addr into MSB field
	 */
	se_config_reg = 0;
	se_config_reg = NV_FLD_SET_DRF_NUM(SE0_SHA, OUT_ADDR_HI, SZ, hash_size,
									   se_config_reg);
	se_config_reg = NV_FLD_SET_DRF_NUM(SE0_SHA, OUT_ADDR_HI, MSB,
		(dma_hash_result >> 32), se_config_reg);
	tegrabl_set_se0_reg(SE0_SHA_OUT_ADDR_HI_0, se_config_reg);

	/**
	 * Issue START command and true for last chunk.
	 */
	if (size_left == block_size) {
		err = tegrabl_start_se0_operation(ARSE_ENG_IDX_SHA, true);
	} else {
		err = tegrabl_start_se0_operation(ARSE_ENG_IDX_SHA, false);
	}
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Poll for BUSY */
	while (engine_busy) {
		err = tegrabl_is_se0_engine_busy(ARSE_ENG_IDX_SHA, &engine_busy);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

fail:
	/* Unmap DMA buffers */
	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SE,
		0, (void *)block_addr, block_size, TEGRABL_DMA_TO_DEVICE);

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SE,
		0, (void *)phash_result, hash_size,
		TEGRABL_DMA_FROM_DEVICE);

	tegrabl_release_se0_mutex();

	if (err != TEGRABL_NO_ERROR) {
		pr_debug("Error = %d in tegrabl_se_sha_process_block\n", err);
	}
	return err;
}

tegrabl_error_t tegrabl_se_sha_process_block(
	struct se_sha_input_params *input_params,
	struct se_sha_context *context)
{
	uint32_t size_to_process;
	uintptr_t buffer;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if ((input_params == NULL) || (context == NULL))
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);

	buffer = input_params->block_addr;
	size_to_process = input_params->block_size;

	/* Process the input in multiples of 16 MB, limited by
	 * the SE0_SHA_OUT_ADDR_HI_0_SZ_FIELD field. */
	do {
		uint32_t size;

		size = (size_to_process > SE_SHA_MAX_INPUT_SIZE) ?
			SE_SHA_MAX_INPUT_SIZE : size_to_process;
		input_params->block_addr = buffer;
		input_params->block_size = size;

		err = _tegrabl_se_sha_process_block(input_params, context);
		if (err != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(err);
			goto fail;
		}

		input_params->size_left -= size;
		size_to_process -= size;
		buffer += size;
	} while (size_to_process > 0);

fail:
	return err;
}

tegrabl_error_t tegrabl_se_sha_process_payload(
	struct se_sha_input_params *input_params,
	struct se_sha_context *context)
{
	size_t size_left;
	uintptr_t end_addr;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	if (input_params == NULL || context == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	/* Store the total payload size and end address */
	size_left = input_params->block_size;
	end_addr = input_params->block_addr + size_left;

	while (size_left > SHA_INPUT_BLOCK_SZ) {
		input_params->block_addr = end_addr - size_left;
		input_params->block_size = SHA_INPUT_BLOCK_SZ;
		input_params->size_left = (uint32_t)size_left;

		ret = tegrabl_se_sha_process_block(input_params, context);
		if (ret != TEGRABL_NO_ERROR) {
			return ret;
		}

		size_left -= SHA_INPUT_BLOCK_SZ;
	}

	input_params->block_addr = end_addr - size_left;
	input_params->block_size = (uint32_t)size_left;
	input_params->size_left = (uint32_t)size_left;

	ret = tegrabl_se_sha_process_block(input_params, context);

	return ret;
}

void tegrabl_se_sha_close(void)
{
	return;
}


/*
 * @brief Generate the correct value of SE_CRYPTO_KEYIV_PKT for SE
 *			AES key operations.
 *
 * @param keyslot SE AES key slot number.
 * @param keyiv_sel Select a Key or IV for the operation. Use
 *			SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_*.
 * @param keyword Select the word number to access in the AES key for the
 *			current operation. Only used for non-IV operations.
 * @param iv_sel SE_CRYPTO_KEYIV_PKT_IV_SEL value for IV operations. Ignored
 *			for regular AES key operations.
 * @param iv_word SE_CRYPTO_KEYIV_PKT_IV_WORD value for IV operations. Ignored
 *			for regular AES key operations.
 * @param  se_keyiv_pkt value of SE_CRYPTO_KEYIV_PKT expected
 *
 * @return TEGRABL_NO_ERROR if success, error if fails
 */
static tegrabl_error_t se_create_keyiv_pkt(
	uint8_t keyslot, uint8_t keyiv_sel, uint8_t keyword,
	uint8_t iv_sel, uint8_t iv_word, uint32_t *se_keyiv_pkt)
{
	*se_keyiv_pkt = 0;
	if (keyiv_sel == SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_KEY) {
		/* IV_SEL and IV_WORD aren't use in the key case, and overlap with
		 * KEY_WORD in SE_CRYPTO_KEYIV_PKT.
		 */
		*se_keyiv_pkt = (
			((uint32_t)keyslot << SE_CRYPTO_KEYIV_PKT_KEY_INDEX_SHIFT) |
			((uint32_t)keyiv_sel << SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_SHIFT) |
			((uint32_t)keyword << SE_CRYPTO_KEYIV_PKT_KEY_WORD_SHIFT)
			);

	} else if (keyiv_sel == SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_IV) {
		/* IV_SEL and IV_WORD overlap with KEY_WORD in SE_CRYPTO_KEYIV_PKT.
		 * KEY_WORD isn't touched here.
		 */
		*se_keyiv_pkt = (
			((uint32_t)keyslot << SE_CRYPTO_KEYIV_PKT_KEY_INDEX_SHIFT) |
			((uint32_t)keyiv_sel << SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_SHIFT) |
			((uint32_t)iv_sel << SE_CRYPTO_KEYIV_PKT_IV_SEL_SHIFT) |
			((uint32_t)iv_word << SE_CRYPTO_KEYIV_PKT_IV_WORD_SHIFT)
			);
	} else {
		/* No else condition. Error checks above should take care of any
		* fuzzed input for keyiv_sel.
		*/
	}

	return TEGRABL_NO_ERROR;
}

/* Setup the operating mode registers for AES CBC
 * encryption/decryption, CMAC Hash and RNG
 */
static tegrabl_error_t tegrabl_se_setup_op_mode(uint8_t mode,
												bool is_encrypt,
												bool use_orig_iv,
												uint8_t dst,
												uint8_t keyslot,
												uint8_t keysize)
{
	uint32_t se_config_reg = 0;
  	uint32_t config_reg = (uint32_t)SE0_AES0_CRYPTO_CONFIG_0_KEY_INDEX_FIELD;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((mode >= SE_OPR_MODE_MAX) ||
		(keyslot >= SE_AES_MAX_KEYSLOTS)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if (keysize > SE_MODE_PKT_AESMODE_KEY256) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	switch (mode) {

	case SE_OPR_MODE_AES_CBC:
	case SE_OPR_MODE_AES_CMAC_HASH:

		if (dst != SE0_AES0_CONFIG_0_DST_MEMORY) {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}

		se_config_reg = 0;

		if (is_encrypt) {
			se_config_reg = NV_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, ENC_ALG,
											   AES_ENC, se_config_reg);
			se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, CONFIG, ENC_MODE,
											   keysize, se_config_reg);
			se_config_reg = NV_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DEC_ALG,
											   NOP, se_config_reg);
		} else {
			se_config_reg = NV_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DEC_ALG,
											   AES_DEC, se_config_reg);
			se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, CONFIG, DEC_MODE,
											   keysize, se_config_reg);
			se_config_reg = NV_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, ENC_ALG,
											   NOP, se_config_reg);
		}

		se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, CONFIG, DST, dst,
										   se_config_reg);
		tegrabl_set_se0_reg(SE0_AES0_CONFIG_0, se_config_reg);

		se_config_reg = 0;
		/* HASH_ENB is set at the end of the function if
		 * we are doing AES-CMAC.
		 */
		se_config_reg = NV_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, HASH_ENB,
										   DISABLE, se_config_reg);
		/* Explicitly set INPUT_SEL to MEMORY for both
		 * is_encrypt/Decrypt cases.
		 */
		se_config_reg = NV_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, INPUT_SEL,
										   MEMORY, se_config_reg);
		if (is_encrypt) {
			se_config_reg = NV_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
											   XOR_POS, TOP, se_config_reg);
			se_config_reg = NV_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, MEMIF,
											   AHB, se_config_reg);
			se_config_reg = NV_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
											   VCTRAM_SEL, INIT_AESOUT,
											   se_config_reg);
			se_config_reg = NV_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
											   CORE_SEL, ENCRYPT,
											   se_config_reg);
		} else {
			se_config_reg = NV_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, XOR_POS,
											   BOTTOM, se_config_reg);
			se_config_reg = NV_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, MEMIF,
											   AHB, se_config_reg);
			se_config_reg = NV_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
											   VCTRAM_SEL, INIT_PREV_MEMORY,
											   se_config_reg);
			se_config_reg = NV_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
											   CORE_SEL, DECRYPT,
											   se_config_reg);
		}
		if (use_orig_iv) {
			se_config_reg = NV_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
											   IV_SELECT, ORIGINAL,
											   se_config_reg);
		} else {
			se_config_reg = NV_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
											   IV_SELECT, UPDATED,
											   se_config_reg);
		}
		se_config_reg &= ~(config_reg);
		se_config_reg |= ((uint32_t)keyslot << SE0_AES0_CRYPTO_CONFIG_0_KEY_INDEX_SHIFT);
		tegrabl_set_se0_reg(SE0_AES0_CRYPTO_CONFIG_0, se_config_reg);

		if (mode == SE_OPR_MODE_AES_CMAC_HASH) {
			se_config_reg = tegrabl_get_se0_reg(SE0_AES0_CRYPTO_CONFIG_0);
			se_config_reg = NV_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
											   HASH_ENB, ENABLE, se_config_reg);
			tegrabl_set_se0_reg(SE0_AES0_CRYPTO_CONFIG_0, se_config_reg);
		}

		break;

	case SE_OPR_MODE_AES_RNG:

		se_config_reg =
			NV_DRF_DEF(SE0_AES0, CONFIG, ENC_ALG, RNG)      |
			NV_DRF_DEF(SE0_AES0, CONFIG, DEC_ALG, NOP)      |
			NV_DRF_NUM(SE0_AES0, CONFIG, ENC_MODE, keysize) |
			NV_DRF_NUM(SE0_AES0, CONFIG, DST, dst);
		tegrabl_set_se0_reg(SE0_AES0_CONFIG_0, se_config_reg);

		se_config_reg = NV_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, INPUT_SEL, RANDOM);

		if (dst == SE0_AES0_CONFIG_0_DST_MEMORY) {

			se_config_reg =
				NV_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, XOR_POS, BYPASS,
								   se_config_reg);
			se_config_reg =
				NV_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, HASH_ENB, DISABLE,
								   se_config_reg);
		}

		tegrabl_set_se0_reg(SE0_AES0_CRYPTO_CONFIG_0, se_config_reg);

		break;

	default:
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		break;
	}

fail:
	return error;
}

/*
 * @brief Read an AES Key or IV to an AES key slot in the SE.
 *
 * @param keyslot SE key slot number.
 * @param keysize AES keysize. Use SE_MODE_PKT_AESMODE to specify keysize.
 *				keysize must be 128-bit if IV is selected.
 * @param keytype Specify if this is a key, original IV or updated IV.
 *				Use SE_CRYPTO_KEYIV_PKT
 *				to specify the type. WORD_QUAD_KEYS_0_3 and
 *				WORD_QUAD_KEYS_4_7 for keys,
 *				SE_CRYPTO_KEYIV_PKT_WORD_QUAD_ORIGINAL_IVS for original IV,
 *				SE_CRYPTO_KEYIV_PKT_WORD_QUAD_UPDATED_IVS for updated IV.
 * @param keydata Pointer to key data. Must be valid memory location.
 *
 * @return TEGRABL_NO_ERROR in case of no error.
 *	returns specific error in case of any error.
 *
 */
static tegrabl_error_t tegrabl_se_aes_read_key_iv(
	uint8_t keyslot, uint8_t keysize,
	uint8_t keytype, uint32_t *keydata)
{
	uint32_t se_keytable_addr = 0;
	uint8_t keyiv_sel = 0;
	uint32_t keytype_32 = (uint32_t)keytype;
	uint8_t iv_sel = 0;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	bool flag = false;

	switch (keytype_32) {
	case SE_CRYPTO_KEYIV_PKT_WORD_QUAD_KEYS_0_3:
	case SE_CRYPTO_KEYIV_PKT_WORD_QUAD_KEYS_4_7:
		keyiv_sel = (uint8_t)SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_KEY;
		iv_sel = 0; /* Don't care in this case. */
		break;
	case SE_CRYPTO_KEYIV_PKT_WORD_QUAD_ORIGINAL_IVS:
		keyiv_sel = (uint8_t)SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_IV;
		iv_sel = (uint8_t)SE_CRYPTO_KEYIV_PKT_IV_SEL_ORIGINAL;
		keysize = (uint8_t)SE_MODE_PKT_AESMODE_KEY128; /* Force 128-bit key size */
		break;
	case SE_CRYPTO_KEYIV_PKT_WORD_QUAD_UPDATED_IVS:
		keyiv_sel = (uint8_t)SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_IV;
		iv_sel = (uint8_t)SE_CRYPTO_KEYIV_PKT_IV_SEL_UPDATED;
		keysize = (uint8_t)SE_MODE_PKT_AESMODE_KEY128; /* Force 128-bit key size */
		break;
	default:
		flag = true;
		break;
	}
	if (flag) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	tegrabl_get_se0_mutex();

	/**
	 * first, SE_CRYPTO_KEYTABLE_ADDR has to be written with the keyslot
	 * configuration for the particular KEY_WORD/IV_WORD you are reading
	 * from. The data to be read from the keyslot KEY_WORD/IV_WORD is then
	 * read from SE_CRYPTOI_KEYTABLE_DATA.
	 */
	err = se_create_keyiv_pkt(
		keyslot, keyiv_sel, 0, iv_sel, 0, &se_keytable_addr);

	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_ADDR_0, se_keytable_addr);
	*keydata++ = tegrabl_get_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_DATA_0);

	err = se_create_keyiv_pkt(
		keyslot, keyiv_sel, 1, iv_sel, 1, &se_keytable_addr);

	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_ADDR_0, se_keytable_addr);
	*keydata++ = tegrabl_get_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_DATA_0);

	err = se_create_keyiv_pkt(
		keyslot, keyiv_sel, 2, iv_sel, 2, &se_keytable_addr);

	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_ADDR_0, se_keytable_addr);
	*keydata++ = tegrabl_get_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_DATA_0);

	err = se_create_keyiv_pkt(
		keyslot, keyiv_sel, 3, iv_sel, 3, &se_keytable_addr);

	if (err != TEGRABL_NO_ERROR) {
		goto fail;
    }
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_ADDR_0, se_keytable_addr);
	*keydata++ = tegrabl_get_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_DATA_0);

	/* if writing a 128-bit key or IV, return. Writes to IV must specify 128-bit
	 * key size.
	 */
	if (keysize == SE_MODE_PKT_AESMODE_KEY128) {
		tegrabl_release_se0_mutex();
		return TEGRABL_NO_ERROR;
	}

	err = se_create_keyiv_pkt(
		keyslot, keyiv_sel, 4, iv_sel, 0, &se_keytable_addr);

	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_ADDR_0, se_keytable_addr);
	*keydata++ = tegrabl_get_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_DATA_0);

	err = se_create_keyiv_pkt(
		keyslot, keyiv_sel, 5, iv_sel, 0, &se_keytable_addr);

	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_ADDR_0, se_keytable_addr);
	*keydata++ = tegrabl_get_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_DATA_0);

	if (keysize == SE_MODE_PKT_AESMODE_KEY192) {
		tegrabl_release_se0_mutex();
		return TEGRABL_NO_ERROR;
	}
	/* Must be a 256-bit key now. */

	err = se_create_keyiv_pkt(
		keyslot, keyiv_sel, 6, iv_sel, 0, &se_keytable_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_ADDR_0, se_keytable_addr);
	*keydata++ = tegrabl_get_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_DATA_0);

	err = se_create_keyiv_pkt(
		keyslot, keyiv_sel, 7, iv_sel, 0, &se_keytable_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_ADDR_0, se_keytable_addr);
	*keydata++ = tegrabl_get_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_DATA_0);

fail:
	tegrabl_release_se0_mutex();
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("Error = %d, in tegrabl_se_aes_read_key_iv\n", err);
	}
	return err;
}

/**
 * @brief Write an AES Key or IV to an AES key slot in the SE.
 *	keysize must be 128-bit for IVs.
 *	If keydata = 0, this function will set the
 *	particular keyslot or IV to all zeroes.
 *
 * @param keyslot SE key slot number.
 * @param keysize AES keysize. Use SE_MODE_PKT_AESMODE to specify keysize.
 *				(keysize can only be 128-bit if IV is selected).
 * @param keytype Specify if this is a key, original IV or updated IV.
 *				Use SE_CRYPTO_KEYIV_PKT to specify the type.
 *				WORD_QUAD_KEYS_0_3 and WORD_QUAD_KEYS_4_7 for keys,
 *				SE_CRYPTO_KEYIV_PKT_WORD_QUAD_ORIGINAL_IVS for original IV,
 *				SE_CRYPTO_KEYIV_PKT_WORD_QUAD_UPDATED_IVS for updated IV.
 * @param keydata Pointer to key data. Must be valid memory location with valid
 *				data. If keydata == 0, this function will set the particular
 *				keyslot to all zeroes.
 *
 * @return TEGRABL_NO_ERROR in case of no error.
 *	returns specific error in case of any error.
 *
 */
static tegrabl_error_t tegrabl_se_aes_write_key_iv(
	uint8_t keyslot, uint8_t keysize,
	uint8_t keytype, uint32_t *keydata)
{
	uint32_t   se_keytable_addr = 0;
	uint32_t keytype_32 = (uint32_t)keytype;
	uint8_t keyiv_sel = 0;
	uint8_t iv_sel = 0;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	bool flag = false;

	switch (keytype_32) {
	case SE_CRYPTO_KEYIV_PKT_WORD_QUAD_KEYS_0_3:
	case SE_CRYPTO_KEYIV_PKT_WORD_QUAD_KEYS_4_7:
		keyiv_sel = (uint8_t)SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_KEY;
		iv_sel = 0; /* Don't care in this case. */
		break;
	case SE_CRYPTO_KEYIV_PKT_WORD_QUAD_ORIGINAL_IVS:
		keyiv_sel = (uint8_t)SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_IV;
		iv_sel = (uint8_t)SE_CRYPTO_KEYIV_PKT_IV_SEL_ORIGINAL;
		keysize = (uint8_t)SE_MODE_PKT_AESMODE_KEY128; /* Force 128-bit key size */
		break;
	case SE_CRYPTO_KEYIV_PKT_WORD_QUAD_UPDATED_IVS:
		keyiv_sel = (uint8_t)SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_IV;
		iv_sel = (uint8_t)SE_CRYPTO_KEYIV_PKT_IV_SEL_UPDATED;
		keysize = (uint8_t)SE_MODE_PKT_AESMODE_KEY128; /* Force 128-bit key size */
		break;
	default:
		flag = true;
		break;
	}
	if (flag) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	tegrabl_get_se0_mutex();

	/**
	 * first, SE_CRYPTO_KEYTABLE_ADDR has to be written with the keyslot
	 * configuration for the particular KEY_WORD/IV_WORD you are writing to.
	 * The data to be written to the keyslot KEY_WORD/IV_WORD is then
	 * written to SE_CRYPTOI_KEYTABLE_DATA.
	 */
	err = se_create_keyiv_pkt(
		keyslot, keyiv_sel, 0, iv_sel, 0, &se_keytable_addr);

	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_ADDR_0, se_keytable_addr);
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_DATA_0,
						(keydata == NULL) ? 0UL : *keydata++);

	err = se_create_keyiv_pkt(
		keyslot, keyiv_sel, 1, iv_sel, 1, &se_keytable_addr);

	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_ADDR_0, se_keytable_addr);
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_DATA_0,
						(keydata == 0UL) ? 0UL : *keydata++);

	err = se_create_keyiv_pkt(
		keyslot, keyiv_sel, 2, iv_sel, 2, &se_keytable_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_ADDR_0, se_keytable_addr);
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_DATA_0,
						(keydata == 0UL) ? 0UL : *keydata++);

	err = se_create_keyiv_pkt(
		keyslot, keyiv_sel, 3, iv_sel, 3, &se_keytable_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_ADDR_0, se_keytable_addr);
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_DATA_0,
						(keydata == 0UL) ? 0UL : *keydata++);

	/* if writing a 128-bit key or IV, return. Writes to IV must specify 128-bit
	 * key size.
	 */
	if (keysize == SE_MODE_PKT_AESMODE_KEY128) {
		tegrabl_release_se0_mutex();
		return TEGRABL_NO_ERROR;
	}

	err = se_create_keyiv_pkt(
		keyslot, keyiv_sel, 4, iv_sel, 0, &se_keytable_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_ADDR_0, se_keytable_addr);
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_DATA_0,
						(keydata == 0UL) ? 0UL : *keydata++);

	err = se_create_keyiv_pkt(
		keyslot, keyiv_sel, 5, iv_sel, 0, &se_keytable_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_ADDR_0, se_keytable_addr);
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_DATA_0,
						(keydata == 0UL) ? 0UL : *keydata++);

	if (keysize == SE_MODE_PKT_AESMODE_KEY192) {
		tegrabl_release_se0_mutex();
		return TEGRABL_NO_ERROR;
	}

	/* Must be a 256-bit key now. */
	err = se_create_keyiv_pkt(
		keyslot, keyiv_sel, 6, iv_sel, 0, &se_keytable_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_ADDR_0, se_keytable_addr);
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_DATA_0,
						(keydata == 0UL) ? 0UL : *keydata++);

	err = se_create_keyiv_pkt(
		keyslot, keyiv_sel, 7, iv_sel, 0, &se_keytable_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_ADDR_0, se_keytable_addr);
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_KEYTABLE_DATA_0,
						(keydata == 0UL) ? 0UL : *keydata++);

fail:
	tegrabl_release_se0_mutex();
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("Error = %d, in tegrabl_se_aes_write_key_iv\n", err);
	}
	return err;
}

/* Encrypt/Decrypt given input data */
static tegrabl_error_t tegrabl_se_aes_encrypt_decrypt (
	uint8_t keyslot,
	uint8_t keysize,
	bool first,
	uint32_t num_blocks,
	uint8_t *src,
	uint8_t *dst,
	bool is_encrypt)
{
	uint32_t se_config_reg = 0;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	dma_addr_t dma_src_addr = 0;
	dma_addr_t dma_dst_addr = 0;
	bool engine_busy = true;

	tegrabl_get_se0_mutex();

	/* Setup SE engine parameters for AES encrypt operation. */
	ret = tegrabl_se_setup_op_mode(
		SE_OPR_MODE_AES_CBC, is_encrypt, first,
		(uint8_t)SE0_AES0_CONFIG_0_DST_MEMORY, keyslot, keysize);
	if (ret != TEGRABL_NO_ERROR) {
		goto fail;
	}

	dma_src_addr = tegrabl_dma_map_buffer(TEGRABL_MODULE_SE,
		0, (void *)src, num_blocks * SE_AES_BLOCK_LENGTH,
		TEGRABL_DMA_TO_DEVICE);
	/* Program input message address. */
	tegrabl_set_se0_reg(SE0_AES0_IN_ADDR_0, (uint32_t)dma_src_addr);

	/* Program input message buffer size into SE0_AES0_IN_ADDR_HI_0_SZ and
	 * 8-bit MSB of 40b dma addr into MSB field
	 */
	se_config_reg = 0;
	se_config_reg = NV_FLD_SET_DRF_NUM(
		SE0_AES0, IN_ADDR_HI, SZ, num_blocks * SE_AES_BLOCK_LENGTH,
		se_config_reg);
	se_config_reg = NV_FLD_SET_DRF_NUM(
		SE0_AES0, IN_ADDR_HI, MSB, (dma_src_addr >> 32), se_config_reg);
	tegrabl_set_se0_reg(SE0_AES0_IN_ADDR_HI_0, se_config_reg);

	dma_dst_addr = tegrabl_dma_map_buffer(TEGRABL_MODULE_SE,
		0, (void *)dst, num_blocks * SE_AES_BLOCK_LENGTH,
		TEGRABL_DMA_FROM_DEVICE);
	/* Program output address. */
	tegrabl_set_se0_reg(SE0_AES0_OUT_ADDR_0, (uint32_t)dma_dst_addr);
	se_config_reg = 0;
	se_config_reg = NV_FLD_SET_DRF_NUM(
		SE0_AES0, OUT_ADDR_HI, SZ, num_blocks * SE_AES_BLOCK_LENGTH,
		se_config_reg);
	se_config_reg = NV_FLD_SET_DRF_NUM(
		SE0_AES0, OUT_ADDR_HI, MSB, (dma_dst_addr >> 32), se_config_reg);
	tegrabl_set_se0_reg(SE0_AES0_OUT_ADDR_HI_0, se_config_reg);

	/*
	 * The SE_CRYPTO_LAST_BLOCK_0 value is calculated by the following formula
	 * given in the SE IAS section 3.2.3.1 AES Input Data Size.
	 * Input Bytes = 16 bytes * (1 + SE_CRYPTO_LAST_BLOCK)
	 * num_blocks*16 = 16 * (1 + SE_CRYPTO_LAST_BLOCK)
	 * num_blocks - 1 = SE_CRYPTO_LAST_BLOCK
	 */
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_LAST_BLOCK_0, num_blocks - 1UL);

	/**
	 * Issue START command in SE_OPERATION.OP
	 */
	ret = tegrabl_start_se0_operation(ARSE_ENG_IDX_AES0, true);
	if (ret != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Poll for OP_DONE. */
	while (engine_busy) {
		ret = tegrabl_is_se0_engine_busy(ARSE_ENG_IDX_AES0, &engine_busy);
		if (ret != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

fail:
	/* Unmap DMA buffers */
	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SE,
		0, (void *)src, num_blocks * SE_AES_BLOCK_LENGTH,
		TEGRABL_DMA_TO_DEVICE);

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SE,
		0, (void *)dst, num_blocks * SE_AES_BLOCK_LENGTH,
		TEGRABL_DMA_FROM_DEVICE);

	tegrabl_release_se0_mutex();
	if (ret != TEGRABL_NO_ERROR) {
		pr_debug("Error = %d in tegrabl_se_aes_encrypt_decrypt\n", ret);
	}
	return ret;
}

/* CMAC subkey generation. This is step1 in CMAC Hash */
static tegrabl_error_t se_aes_cmac_generate_subkey(
	uint8_t keyslot, uint8_t keysize, uint8_t *pk1, uint8_t *pk2)
{
	uint32_t se_config_reg = 0;
	bool msbL = false;
	uint8_t *pk = NULL;
	uint32_t i = 0;
	static uint8_t *zero;
	static uint8_t *L;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	dma_addr_t dma_zero_addr = 0;
	dma_addr_t dma_l_addr = 0;
	bool engine_busy = true;

	if ((zero == NULL) || (L == NULL)) {
		zero = tegrabl_alloc(TEGRABL_HEAP_DMA, 2U * SE_AES_BLOCK_LENGTH);
		if (zero == NULL) {
			ret = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			goto fail;
		}
		L = zero + SE_AES_BLOCK_LENGTH;
	}
	memset(zero, 0, SE_AES_BLOCK_LENGTH);

	tegrabl_get_se0_mutex();

	/* Set up SE engine for AES encrypt + CMAC hash. */
	ret = tegrabl_se_setup_op_mode(
		SE_OPR_MODE_AES_CMAC_HASH, true, true,
		(uint8_t)SE0_AES0_CONFIG_0_DST_MEMORY, keyslot, keysize);
	if (ret != TEGRABL_NO_ERROR) {
		goto fail;
	}

	dma_zero_addr = tegrabl_dma_map_buffer(TEGRABL_MODULE_SE,
		0, (void *)zero, SE_AES_BLOCK_LENGTH,
		TEGRABL_DMA_TO_DEVICE);
	/* Program input message address. */
	tegrabl_set_se0_reg(SE0_AES0_IN_ADDR_0, (uint32_t)dma_zero_addr);

	/* Program input message buffer size into SE0_AES0_IN_ADDR_HI_0_SZ and
	 * 8-bit MSB of 40b dma addr into MSB field.
	 */
	se_config_reg = 0;
	se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, IN_ADDR_HI, SZ,
									   SE_AES_BLOCK_LENGTH, se_config_reg);
	se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, IN_ADDR_HI, MSB,
		(dma_zero_addr >> 32), se_config_reg);
	tegrabl_set_se0_reg(SE0_AES0_IN_ADDR_HI_0, se_config_reg);

	dma_l_addr = tegrabl_dma_map_buffer(TEGRABL_MODULE_SE,
		0, (void *)L, SE_AES_BLOCK_LENGTH,
		TEGRABL_DMA_FROM_DEVICE);
	/* Program output address of L buffer. */
	tegrabl_set_se0_reg(SE0_AES0_OUT_ADDR_0, (uint32_t)dma_l_addr);
	se_config_reg = 0;
	se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, OUT_ADDR_HI, SZ,
									   SE_AES_BLOCK_LENGTH, se_config_reg);
	se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, OUT_ADDR_HI, MSB,
		(dma_l_addr >> 32), se_config_reg);
	tegrabl_set_se0_reg(SE0_AES0_OUT_ADDR_HI_0, se_config_reg);

	/*
	 * Input Bytes = 16 bytes * (1 + SE0_AES0_CRYPTO_LAST_BLOCK)
	 * 16 = 16 * (1 + SE0_AES_CRYPTO_LAST_BLOCK)
	 * SE0_AES0_CRYPTO_LAST_BLOCK = 0
	 */
	tegrabl_set_se0_reg(SE0_AES0_CRYPTO_LAST_BLOCK_0, 0);

	/* Releae mutex before key slot operations. */
	tegrabl_release_se0_mutex();

	/* Initialize key slot OriginalIv[127:0] to zero */
	ret = tegrabl_se_aes_write_key_iv(
		keyslot, keysize, (uint8_t)SE_CRYPTO_KEYIV_PKT_WORD_QUAD_ORIGINAL_IVS,
		(uint32_t *)zero);
	if (ret != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Initialize key slot UpdatedIV[127:0] to zero */
	ret = tegrabl_se_aes_write_key_iv(
		keyslot, keysize, (uint8_t)SE_CRYPTO_KEYIV_PKT_WORD_QUAD_UPDATED_IVS,
		(uint32_t *)zero);

	/* Get mutex for key operation. */
	tegrabl_get_se0_mutex();

	/*
	 * Issue START command in SE0_AES0_SE_OPERATION.OP
	 */
	ret = tegrabl_start_se0_operation(ARSE_ENG_IDX_AES0, true);

	if (ret != TEGRABL_NO_ERROR) {
		goto fail;
	}
	/* Poll for IDLE. */
	while (engine_busy) {
		ret = tegrabl_is_se0_engine_busy(ARSE_ENG_IDX_AES0, &engine_busy);

		if (ret != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

	/* Unmap DMA buffers */
	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SE,
		0, (void *)zero, SE_AES_BLOCK_LENGTH,
		TEGRABL_DMA_TO_DEVICE);

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SE,
		0, (void *)L, SE_AES_BLOCK_LENGTH,
		TEGRABL_DMA_FROM_DEVICE);

	/* Release Se0 mutex */
	tegrabl_release_se0_mutex();

	/*
	 * L := AES-{128,192,256}(K, const_Zero);
	 * if MSB(L) is equal to 0,
	 * then K1 := L << 1;
	 * else K1 := (L << 1) XOR const_Rb;
	 */
	msbL = tegrabl_se_is_msb_set(L[0]);
	for (i = 0; i < SE_AES_BLOCK_LENGTH; i++) {
		pk1[i] = L[i];
	}
	ret = tegrabl_se_left_shift_one_bit((uint8_t *)pk1, SE_AES_BLOCK_LENGTH);

	if (ret != TEGRABL_NO_ERROR) {
		goto fail;
	}

	pk = (uint8_t *) pk1;
	if (msbL) {
		*(pk + SE_AES_BLOCK_LENGTH - 1) ^= AES_CMAC_CONST_RB;
	}

	/*
	 * if MSB(K1) is equal to 0
	 * then K2 := K1 << 1;
	 * else K2 := (K1 << 1) XOR const_Rb;
	 */
	for (i = 0; i < SE_AES_BLOCK_LENGTH; i++) {
		pk2[i] = pk1[i];
	}
	msbL = tegrabl_se_is_msb_set((uint8_t) *pk2);
	ret = tegrabl_se_left_shift_one_bit((uint8_t *)pk2, SE_AES_BLOCK_LENGTH);

	if (ret != TEGRABL_NO_ERROR) {
		goto fail;
	}

	pk = (uint8_t *) pk2;

	if (msbL != false) {
		*(pk + SE_AES_BLOCK_LENGTH - 1) ^= AES_CMAC_CONST_RB;
	}

fail:
	if (ret != TEGRABL_NO_ERROR) {
		pr_debug("Error = %d, in se_aes_cmac_generate_subkey\n", ret);
	}
	return ret;
}

/* Compute CMAC Hash on input data  using keyslot and subkeys */
static tegrabl_error_t se_aes_cmac_hash_blocks(
	uint8_t *pk1, uint8_t *pk2, uint8_t *pinput_message, uint8_t *phash,
	uint8_t keyslot, uint8_t keysize, uint32_t num_blocks, bool is_first,
	bool is_last)
{
	const uint32_t byte_offset_to_last_block = (num_blocks - 1UL) *
		SE_AES_BLOCK_LENGTH;
	uint32_t i = 0;
	uint32_t se_config_reg = 0;
	uint8_t *pinput_message_last_block_r5;
	static uint8_t *zero;
	static uint8_t *last_block;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	dma_addr_t dma_input_message = 0;
	dma_addr_t dma_hash_addr = 0;
	dma_addr_t dma_last_block = 0;
	bool engine_busy = true;

	TEGRABL_UNUSED(pk2);

	pinput_message_last_block_r5 = (uint8_t *) (pinput_message +
		byte_offset_to_last_block);

	if ((zero == NULL) || (last_block == NULL)) {
		zero = tegrabl_alloc(TEGRABL_HEAP_DMA, 2U * SE_AES_BLOCK_LENGTH);
		if (zero == NULL) {
			ret = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			goto fail;
		}

		last_block = zero + SE_AES_BLOCK_LENGTH;
	}
	memset(zero, 0, SE_AES_BLOCK_LENGTH);

	if (is_first) {
		/* Clear IVs of SE keyslot. */
		/* Initialize key slot OriginalIv[127:0] to zero */
		ret = tegrabl_se_aes_write_key_iv(
			keyslot, keysize, (uint8_t)SE_CRYPTO_KEYIV_PKT_WORD_QUAD_ORIGINAL_IVS,
			(uint32_t *)zero);
		if (ret != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* Initialize key slot UpdatedIV[127:0] to zero */
		ret = tegrabl_se_aes_write_key_iv(
			keyslot, keysize, (uint8_t)SE_CRYPTO_KEYIV_PKT_WORD_QUAD_UPDATED_IVS,
			(uint32_t *)zero);
		if (ret != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

	if (is_last) {
		if (num_blocks > 1UL) {
			/* Check if SE is idle. */
			engine_busy = true;
			while (engine_busy) {
				ret = tegrabl_is_se0_engine_busy(ARSE_ENG_IDX_AES0,
					&engine_busy);
				if (ret != TEGRABL_NO_ERROR)
					goto fail;
			}

			tegrabl_get_se0_mutex();

			/* Hash the input data for blocks zero to NumBLocks - 1. */
			ret = tegrabl_se_setup_op_mode(SE_OPR_MODE_AES_CMAC_HASH, true, false,
							(uint8_t)SE0_AES0_CONFIG_0_DST_MEMORY, keyslot, keysize);
			if (ret != TEGRABL_NO_ERROR)
				goto fail;
			dma_input_message = tegrabl_dma_map_buffer(TEGRABL_MODULE_SE,
				0, (void *)pinput_message, byte_offset_to_last_block,
				TEGRABL_DMA_TO_DEVICE);
			/* Program input message address. */
			tegrabl_set_se0_reg(SE0_AES0_IN_ADDR_0,
								(uint32_t)dma_input_message);

			/* Program input message buffer size into
			 * SE0_AES0_IN_ADDR_HI_0_SZ and 8-bit MSB of 40b
			 * dma addr into MSB field.
			 */
			se_config_reg = 0;
			se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, IN_ADDR_HI, SZ,
											   byte_offset_to_last_block,
											   se_config_reg);
			se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, IN_ADDR_HI, MSB,
				(dma_input_message >> 32), se_config_reg);
			tegrabl_set_se0_reg(SE0_AES0_IN_ADDR_HI_0, se_config_reg);

			dma_hash_addr = tegrabl_dma_map_buffer(TEGRABL_MODULE_SE,
				0, (void *)phash, SE_AES_BLOCK_LENGTH,
				TEGRABL_DMA_FROM_DEVICE);
			/* Set output destination to phash */
			/* This is the output of the 0 to N-1 blocks of the hash. */
			tegrabl_set_se0_reg(SE0_AES0_OUT_ADDR_0,
								(uint32_t)dma_hash_addr);

			/* Set ADDR_HI registers. */
			se_config_reg = 0;
			se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, OUT_ADDR_HI, SZ,
											   SE_AES_BLOCK_LENGTH,
											   se_config_reg);
			se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, OUT_ADDR_HI, MSB,
											   (dma_hash_addr >> 32),
											   se_config_reg);
			tegrabl_set_se0_reg(SE0_AES0_OUT_ADDR_HI_0, se_config_reg);

			/* num_blocks - 1 = SE_CRYPTO_LAST_BLOCK
			 * Since we want to encrypt num_blocks - 1 blocks,
			 * it should be num_blocks - 1 - 1.
			 */
			tegrabl_set_se0_reg(SE0_AES0_CRYPTO_LAST_BLOCK_0,
								(num_blocks - 1UL) - 1UL);

			/**
			 * Issue START command in SE0_AES0_OPERATION.OP.
			 * LASTBUF = false
			 * since this is not the last AES-CMAC operation.
			 * Reference: 4.2.2.4 SW LL in AES CMAC section of SE IAS.
			 */
			ret = tegrabl_start_se0_operation(ARSE_ENG_IDX_AES0, false);
			if (ret != TEGRABL_NO_ERROR) {
				goto fail;
			}

			/* Poll for IDLE. */
			engine_busy = true;
			while (engine_busy) {
				ret = tegrabl_is_se0_engine_busy(ARSE_ENG_IDX_AES0,
					&engine_busy);
				if (ret != TEGRABL_NO_ERROR)
					goto fail;
			}

			/* Unmap DMA buffers */
			tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SE,
				0, (void *)pinput_message, byte_offset_to_last_block,
				TEGRABL_DMA_TO_DEVICE);

			tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SE,
				0, (void *)phash, SE_AES_BLOCK_LENGTH,
				TEGRABL_DMA_FROM_DEVICE);

			tegrabl_release_se0_mutex();

			/* Write the output of the 0 to N-1 blocks back to the
			 * UPDATED_IV before proceeding with the last block.
			 */
			ret = tegrabl_se_aes_write_key_iv(
				keyslot, (uint8_t)SE_MODE_PKT_AESMODE_KEY128,
				(uint8_t)SE_CRYPTO_KEYIV_PKT_WORD_QUAD_UPDATED_IVS,
				(uint32_t *)phash);
			if (ret != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}

		/* Process the last block with the correct subkey K1 or K2. */
		for (i = 0; i < SE_AES_BLOCK_LENGTH; i++) {
			last_block[i] = *(pk1 + i) ^
				*(pinput_message_last_block_r5 + i);
		}

		tegrabl_get_se0_mutex();

		/* Setup SE engine parameters.
		 * Set output destination to MEMORY.
		 * Set IV to UPDATED_IV because we are continuing a previous AES
		 * operation.
		 */
		ret = tegrabl_se_setup_op_mode(SE_OPR_MODE_AES_CMAC_HASH, true, false,
						(uint8_t)SE0_AES0_CONFIG_0_DST_MEMORY, keyslot, keysize);
		if (ret != TEGRABL_NO_ERROR) {
			goto fail;
		}

		dma_last_block = tegrabl_dma_map_buffer(TEGRABL_MODULE_SE,
			0, (void *)last_block, SE_AES_BLOCK_LENGTH,
			TEGRABL_DMA_TO_DEVICE);
		/* Program input message address. */
		tegrabl_set_se0_reg(SE0_AES0_IN_ADDR_0,
							(uint32_t)dma_last_block);

		/* Program input message buffer size into
		 * SE0_AES0_IN_ADDR_HI_0_SZ and 8-bit MSB of 40b
		 * dma addr into MSB field.
		 */
		se_config_reg = 0;
		se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, IN_ADDR_HI, SZ,
										   SE_AES_BLOCK_LENGTH,
										   se_config_reg);
		se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, IN_ADDR_HI, MSB,
			(dma_last_block >> 32), se_config_reg);
		tegrabl_set_se0_reg(SE0_AES0_IN_ADDR_HI_0, se_config_reg);

		/* num_blocks - 1 = SE_CRYPTO_LAST_BLOCK
		 * Since we are only encrypting one block,
		 * set SE_CRYPTO_LAST_BLOCK_0 to 0.
		 */
		tegrabl_set_se0_reg(SE0_AES0_CRYPTO_LAST_BLOCK_0, 0);

		dma_hash_addr = tegrabl_dma_map_buffer(TEGRABL_MODULE_SE,
			0, (void *)phash, SE_AES_BLOCK_LENGTH,
			TEGRABL_DMA_FROM_DEVICE);
		/* Set output destination to phash */
		tegrabl_set_se0_reg(SE0_AES0_OUT_ADDR_0,
							(uint32_t)dma_hash_addr);

		/* Set ADDR_HI registers. */
		se_config_reg = 0;
		se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, OUT_ADDR_HI, SZ,
										   SE_AES_BLOCK_LENGTH,
										   se_config_reg);
		se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, OUT_ADDR_HI, MSB,
			(dma_hash_addr >> 32), se_config_reg);
		tegrabl_set_se0_reg(SE0_AES0_OUT_ADDR_HI_0, se_config_reg);

		/**
		 * Issue START command in SE0_AES0_OPERATION.OP
		 */
		ret = tegrabl_start_se0_operation(ARSE_ENG_IDX_AES0, true);
		if (ret != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* Poll for IDLE. */
		engine_busy = true;
		while (engine_busy) {
			ret = tegrabl_is_se0_engine_busy(ARSE_ENG_IDX_AES0,
				&engine_busy);
			if (ret != TEGRABL_NO_ERROR)
				goto fail;
		}
		/* Unmap DMA buffers */
		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SE,
			0, (void *)last_block, SE_AES_BLOCK_LENGTH,
			TEGRABL_DMA_TO_DEVICE);

		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SE,
			0, (void *)phash, SE_AES_BLOCK_LENGTH,
			TEGRABL_DMA_FROM_DEVICE);

		tegrabl_release_se0_mutex();
	} else {
		/* Check if SE is busy, wait if so. */
		engine_busy = true;
		while (engine_busy) {
			ret = tegrabl_is_se0_engine_busy(ARSE_ENG_IDX_AES0,
				&engine_busy);
			if (ret != TEGRABL_NO_ERROR)
				goto fail;
		}

		tegrabl_get_se0_mutex();
		/* Hash the input data for blocks zero to NumBLocks.
		 * Setup SE engine parameters.
		 * Use the SE_HASH_RESULT_* output registers to
		 * store the intermediate result.
		 * Set IV to UPDATED_IV because we are continuing a previous AES
		 * operation.
		 */
		ret = tegrabl_se_setup_op_mode(
			SE_OPR_MODE_AES_CMAC_HASH, true, false,
			(uint8_t)SE0_AES0_CONFIG_0_DST_MEMORY, keyslot, keysize);
		if (ret != TEGRABL_NO_ERROR) {
			goto fail;
		}
		dma_input_message = tegrabl_dma_map_buffer(TEGRABL_MODULE_SE,
			0, (void *)pinput_message, num_blocks * SE_AES_BLOCK_LENGTH,
			TEGRABL_DMA_TO_DEVICE);
		/* Program input message address. */
		tegrabl_set_se0_reg(SE0_AES0_IN_ADDR_0,
							(uint32_t)dma_input_message);

		/* Program input message buffer size into
		 * SE0_AES0_IN_ADDR_HI_0_SZ and 8-bit MSB of 40b
		 * dma addr into MSB field.
		 */
		se_config_reg = 0;
		se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, IN_ADDR_HI, SZ,
										   num_blocks * SE_AES_BLOCK_LENGTH,
										   se_config_reg);
		se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, IN_ADDR_HI, MSB,
			(dma_input_message >> 32), se_config_reg);
		tegrabl_set_se0_reg(SE0_AES0_IN_ADDR_HI_0, se_config_reg);

		dma_hash_addr = tegrabl_dma_map_buffer(TEGRABL_MODULE_SE,
			0, (void *)phash, SE_AES_BLOCK_LENGTH,
			TEGRABL_DMA_FROM_DEVICE);
		/* Set output destination to phash */
		tegrabl_set_se0_reg(SE0_AES0_OUT_ADDR_0,
							(uint32_t)dma_hash_addr);

		/* Set ADDR_HI registers. */
		se_config_reg = 0;
		se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, OUT_ADDR_HI, SZ,
										   SE_AES_BLOCK_LENGTH,
										   se_config_reg);
		se_config_reg = NV_FLD_SET_DRF_NUM(SE0_AES0, OUT_ADDR_HI, MSB,
			(dma_hash_addr >> 32), se_config_reg);
		tegrabl_set_se0_reg(SE0_AES0_OUT_ADDR_HI_0, se_config_reg);

		/* num_blocks - 1 = SE_CRYPTO_LAST_BLOCK */
		tegrabl_set_se0_reg(SE0_AES0_CRYPTO_LAST_BLOCK_0, num_blocks - 1UL);

		/**
		 * Issue START command in SE0_AES0_OPERATION.OP.
		 * LastBuff is false.
		 */
		ret = tegrabl_start_se0_operation(ARSE_ENG_IDX_AES0, false);
		if (ret != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* Block while SE processes this chunk, then release mutex. */
		engine_busy = true;
		while (engine_busy) {
			ret = tegrabl_is_se0_engine_busy(ARSE_ENG_IDX_AES0,
				&engine_busy);
			if (ret != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}
		/* Unmap DMA buffers */
		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SE,
			0, (void *)pinput_message, num_blocks * SE_AES_BLOCK_LENGTH,
			TEGRABL_DMA_TO_DEVICE);

		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SE,
			0, (void *)phash, SE_AES_BLOCK_LENGTH,
			TEGRABL_DMA_FROM_DEVICE);

		tegrabl_release_se0_mutex();
	}

fail:
	if (ret != TEGRABL_NO_ERROR) {
		pr_debug("Error = %d, in se_aes_cmac_hash_blocks\n", ret);
	}
	return ret;
}

static tegrabl_error_t _tegrabl_se_aes_process_block(
	struct se_aes_input_params *input_params,
	struct se_aes_context *context)
{
	uint32_t num_blocks;
	bool is_first = false;
	bool is_last = false;
	uint8_t keytype = (uint8_t)SE_CRYPTO_KEYIV_PKT_WORD_QUAD_KEYS_0_3;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	static uint8_t subkey_cache_index;

	if ((context == NULL) || (input_params == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (!((context->is_decrypt == true) || (context->is_encrypt == true)) &&
										(context->is_hash == false)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if ((context->is_decrypt == true) && (context->is_encrypt == true)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if ((context->total_size == 0UL) ||
		((context->total_size % SE_AES_BLOCK_LENGTH) != 0UL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (context->keyslot >= SE_AES_MAX_KEYSLOTS) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (context->keysize > SE_MODE_PKT_AESMODE_KEY256) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (context->is_hash) {
		if ((context->pk1 == NULL) ||
			(context->pk2 == NULL)) {
			return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		}
		if (context->phash == NULL) {
			return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		}
	}

	if (input_params->src == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (((input_params->input_size % SE_AES_BLOCK_LENGTH) != 0UL) ||
		((input_params->size_left % SE_AES_BLOCK_LENGTH) != 0UL) ||
		(input_params->input_size > SE_AES_MAX_INPUT_SIZE)) {
			return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (input_params->dst == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	num_blocks = input_params->input_size / SE_AES_BLOCK_LENGTH;

	is_first = (context->total_size == input_params->size_left);
	is_last = (input_params->input_size == input_params->size_left);

	if (is_first) {
		if (context->pkey != NULL) {
			ret = tegrabl_se_aes_write_key_iv(
				context->keyslot, context->keysize, keytype,
				(uint32_t *)context->pkey);
			if (ret != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}

		if (context->is_hash) {
			if (aes_subkeys[0].keyslot == context->keyslot) {
				memcpy(context->pk1, aes_subkeys[0].pk1, SE_AES_BLOCK_LENGTH);
				memcpy(context->pk2, aes_subkeys[0].pk2, SE_AES_BLOCK_LENGTH);
			} else if (aes_subkeys[1].keyslot == context->keyslot) {
				memcpy(context->pk1, aes_subkeys[1].pk1, SE_AES_BLOCK_LENGTH);
				memcpy(context->pk2, aes_subkeys[1].pk2, SE_AES_BLOCK_LENGTH);
			} else {
				ret = se_aes_cmac_generate_subkey(context->keyslot,
						context->keysize, context->pk1, context->pk2);
				if (ret != TEGRABL_NO_ERROR) {
					goto fail;
				}
				memcpy(aes_subkeys[subkey_cache_index].pk1, context->pk1,
					SE_AES_BLOCK_LENGTH);
				memcpy(aes_subkeys[subkey_cache_index].pk2, context->pk2,
					SE_AES_BLOCK_LENGTH);
				aes_subkeys[subkey_cache_index].keyslot = context->keyslot;
				subkey_cache_index = (uint8_t)((subkey_cache_index + 1U) %
										SUBKEY_CACHE_SIZE);
			}
		}
	}

	if (context->is_hash) {
		if (!is_first){
			ret = tegrabl_se_aes_write_key_iv(
				context->keyslot, context->keysize,
				(uint8_t)SE_CRYPTO_KEYIV_PKT_WORD_QUAD_UPDATED_IVS,
				(uint32_t *)context->phash);
			if (ret != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}

		ret = se_aes_cmac_hash_blocks(
			context->pk1, context->pk2, input_params->src,
			context->phash, context->keyslot, context->keysize,
			num_blocks, is_first, is_last);
		if (ret != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

	if ((context->is_decrypt == true) || (context->is_encrypt == true)) {
		if (is_first) {
			ret = tegrabl_se_aes_write_key_iv(
				context->keyslot, context->keysize,
				(uint8_t)SE_CRYPTO_KEYIV_PKT_WORD_QUAD_ORIGINAL_IVS,
				(uint32_t *)context->iv_encrypt);
			if (ret != TEGRABL_NO_ERROR) {
				goto fail;
			}
		} else {
			ret = tegrabl_se_aes_write_key_iv(
				context->keyslot, context->keysize,
				(uint8_t)SE_CRYPTO_KEYIV_PKT_WORD_QUAD_UPDATED_IVS,
				(uint32_t *)context->iv_encrypt);
			if (ret != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}

		if (context->is_encrypt) {
			ret = tegrabl_se_aes_encrypt_decrypt(
				context->keyslot, context->keysize, is_first, num_blocks,
				input_params->src, input_params->dst, true);
		} else {
			ret = tegrabl_se_aes_encrypt_decrypt(
				context->keyslot, context->keysize, is_first, num_blocks,
				input_params->src, input_params->dst, false);
		}
		if (ret != TEGRABL_NO_ERROR) {
			goto fail;
		}

		ret = tegrabl_se_aes_read_key_iv(
			context->keyslot, context->keysize,
			(uint8_t)SE_CRYPTO_KEYIV_PKT_WORD_QUAD_UPDATED_IVS,
			(uint32_t *)context->iv_encrypt);
		if (ret != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}
fail:
	return ret;
}

tegrabl_error_t tegrabl_se_aes_process_block(
	struct se_aes_input_params *input_params,
	struct se_aes_context *context)
{
	uint8_t *buffer;
	uint32_t size_to_process;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if ((context == NULL) || (input_params == NULL))
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);

	buffer = input_params->src;
	size_to_process = input_params->input_size;

	/* Process the input in multiples of 16 MB, limited by
	 * the SE0_AES0_CRYPTO_LAST_BLOCK_0_WRITE_MASK field. */
	do {
		uint32_t size;

		size = (size_to_process > SE_AES_MAX_INPUT_SIZE) ?
			SE_AES_MAX_INPUT_SIZE : size_to_process;
		input_params->src = buffer;
		input_params->input_size = size;

		err = _tegrabl_se_aes_process_block(input_params, context);
		if (err != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(err);
			goto fail;
		}
		input_params->size_left -= size;
		size_to_process -= size;
		buffer += size;
	} while (size_to_process > 0);

fail:
	return err;
}

void tegrabl_se_aes_close(void)
{
	return;
}

/* Mask generation function using SHA */
static tegrabl_error_t tegrabl_se_mask_generation(
	uint8_t *mgf_seed, uint32_t mask_len, uint8_t *db_mask_buffer,
	uint8_t hash_algorithm, uint32_t hlen)
{
	uint32_t counter = 0;
	struct se_sha_input_params input;
	struct se_sha_context context;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	static uint8_t *buff;

	if (buff == NULL) {
		buff = tegrabl_alloc(TEGRABL_HEAP_DMA, hlen + 4UL);
		if (buff == NULL) {
			ret = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			goto fail;
		}
	}
	memcpy(buff, mgf_seed, hlen);
	input.block_addr = (uintptr_t)buff;
	input.block_size = hlen + 4UL;
	input.size_left = hlen + 4UL;

	buff[hlen + 0UL] = 0;
	buff[hlen + 1UL] = 0;
	buff[hlen + 2UL] = 0;
	buff[hlen + 3UL] = 0;

	context.input_size = hlen + 4UL;
	context.hash_algorithm = hash_algorithm;

	for (counter = 0; counter <= (NV_ICEIL(mask_len, hlen) - 1UL); counter++) {
		buff[hlen + 3UL] = (uint8_t)counter;
		input.hash_addr = (uintptr_t)&db_mask_buffer[counter * hlen];
		ret = _tegrabl_se_sha_process_block(&input, &context);
		if (ret != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

fail:
	return ret;
}

tegrabl_error_t tegrabl_se_rsa_pss_verify(
	uint8_t rsa_keyslot, uint32_t rsa_keysize_bits, uint8_t *pmessage_hash,
	uint32_t *pinput_signature, uint8_t hash_algorithm, uint32_t slen)
{
	uint32_t *pmr_r5 = NULL;
	uint32_t *pmr_se = NULL;
	uint8_t *em = NULL;
	uint8_t *H = NULL;
	uint8_t *masked_db = NULL;
	uint32_t masked_dblen = 0;
	uint32_t hlen = 0;
	const uint32_t em_len = NV_ICEIL(rsa_keysize_bits - 1UL, 8UL);
	uint8_t lowest_bits = 0;
	uint8_t first_octet_masked_db = 0;
	static uint32_t *message_representative;
	static uint8_t *db_mask;
	static uint8_t *db;
	static uint8_t *m_prime;
	struct se_sha_input_params input;
	struct se_sha_context context;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	uint32_t i = 0;
	static uint8_t *buff;
	static uint32_t buff_size;
	static uint32_t message_representative_size;
	static uint32_t db_mask_size;
	static uint32_t db_size;
	static uint32_t m_prime_size;
	uint32_t em_minus_hlen_minus_1 = 0;
	uint32_t hash_algorithm_32 = (uint32_t)hash_algorithm;
	bool flag = false;

	if ((rsa_keyslot >= SE_RSA_MAX_KEYSLOTS) ||
		(rsa_keysize_bits > RSA_MAX_EXPONENT_SIZE_BITS) ||
		(pmessage_hash == NULL) ||
		(pinput_signature == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (!((slen == ((uint32_t)(ARSE_SHA1_HASH_SIZE) / 8U)) ||
		(slen == ((uint32_t)(ARSE_SHA224_HASH_SIZE) / 8U)) ||
		(slen == ((uint32_t)(ARSE_SHA256_HASH_SIZE) / 8U)) ||
		(slen == ((uint32_t)(ARSE_SHA384_HASH_SIZE) / 8U)) ||
		(slen == ((uint32_t)(ARSE_SHA512_HASH_SIZE) / 8U)))) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
	}

	switch (hash_algorithm_32) {
	case SE_MODE_PKT_SHAMODE_SHA1:
		hlen =  ARSE_SHA1_HASH_SIZE / 8;
		break;
	case SE_MODE_PKT_SHAMODE_SHA224:
		hlen = ARSE_SHA224_HASH_SIZE / 8;
		break;
	case SE_MODE_PKT_SHAMODE_SHA256:
		hlen = ARSE_SHA256_HASH_SIZE / 8;
		break;
	case SE_MODE_PKT_SHAMODE_SHA384:
		hlen = ARSE_SHA384_HASH_SIZE / 8;
		break;
	case SE_MODE_PKT_SHAMODE_SHA512:
		hlen = ARSE_SHA512_HASH_SIZE / 8;
		break;
	default:
		flag = true;
		break;
	}
	if (flag) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
	}

	if (buff_size == 0U) {
		message_representative_size =
			(RSA_MAX_EXPONENT_SIZE_BITS / 8U) * 4U;
		buff_size += message_representative_size;
		db_mask_size = (rsa_keysize_bits / 8UL) - hlen - 1UL;
		buff_size += db_mask_size;
		m_prime_size = 8 + (ARSE_SHA512_HASH_SIZE / 8) +
			(ARSE_SHA512_HASH_SIZE / 8);
		buff_size += m_prime_size;
		buff = tegrabl_alloc(TEGRABL_HEAP_DMA, buff_size);
		if (buff == NULL) {
			ret = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			goto fail;
		}
	}

	if (message_representative == NULL) {
		message_representative = (uint32_t *)buff;
	}

	if (db_mask == NULL) {
		db_mask = (uint8_t *)((uintptr_t)buff +
				message_representative_size);
	}

	if (m_prime == NULL) {
		m_prime = buff + message_representative_size +
			db_mask_size;
	}

	pmr_se = &message_representative[0];
	pmr_r5 = &message_representative[0];

	if (db == NULL) {
		db_size = (rsa_keysize_bits / 8UL) - hlen - 1UL;
		db = tegrabl_alloc(TEGRABL_HEAP_DEFAULT, db_size);
	}

	/**
	 *  1. Length checking: If the length of the signature S is not k octets,
	 *  where k is the length in octets of the RSA modulus n, output "invalid
	 *  signature".
	 *
	 *  The length of the signature S is assumed to be of the correct length.
	 *  The onus is on the calling function.
	 */

	/**
	 *  2. a) Convert the signature S to an integer signature representative:
	 *		s = OS2IP (S).
	 *
	 *  This is not necessary since the integer signature representative
	 *  is already in an octet byte stream.
	 */

	/**
	 *  2. b) Apply the RSAVP1 verification primitive to the
	 *		RSA public key (n, e)
	 *		and the signature representative s to produce an integer message
	 *		representative m:
	 *			  m = RSAVP1 ((n, e), s)
	 *			  m = s^e mod n
	 *
	 *		If RSAVO1 output "signature representative out of range", output
	 *		"invalid signature" and stop.
	 *
	 */

	/* Calculate m = s^e mod n */
	ret = tegrabl_se_rsa_modular_exp(
		rsa_keyslot, rsa_keysize_bits, 32,
		(uint8_t *)pinput_signature, (uint8_t *)pmr_se);

	if (ret != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* After the RSAVP1 step, the message representative m is stored in
	 * in ascending order in a byte array, i.e. the 0xbc trailer field is the
	 * first value of the array, when it is the "last" vlaue in the spec.
	 * Reversing the byte order in the array will match the endianness
	 * of the PKCS #1 spec and make for code that directly matches the spec.
	 */
	ret = tegrabl_se_reverse_list((uint8_t *)pmr_r5, em_len);
	if (ret != TEGRABL_NO_ERROR) {
		return ret;
	}
	/**
	 * 2. c) Convert the message representative m to an encoded message em of
	 *	   length em_len = Ceiling( (modBits - 1) / 8) octets, where modBits
	 *	   is the length in bits of the RSA modulus n:
	 *		  em = I2OSP(m, em_len)
	 *
	 */
	em = (uint8_t *) pmr_r5;

	/**
	 * 3. emSA-PSS verification: Apply the emSA-PSS verification operation
	 *	to the message M and the encoded message em to determine whether
	 *	they are consistent:
	 *	  Result = emSA-PSS-VERIFY(M, em, modBits -1)
	 *
	 *	if(Result == "consistent")
	 *		  output "Valid Signature"
	 *	else
	 *		  output "Invlaid Signature"
	 */


	/* Step 1. If the length of M is greater than the input limitation of for
	 * the hash function, output "inconsistent" and stop.
	 *
	 * Step 2. Let mHash = Hash(M), an octet string of length hlen.
	 *
	 * Step 3. If em_len < hlen + slen + 2, output "inconsistent" and stop.
	 * em_len < hlen + slen + 2
	 * Ceiling(emBits/8) < hlen + slen + 2
	 * Ceiling(modBits-1/8) < hlen + slen + 2
	 * 256 octets < 32 octets + 32 octets + 2
	 * (assuming SHA256 as the hash function)
	 * Salt length is equal to hash length
	 */
	if (em_len < (hlen + slen + 2UL)) {
		ret = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 0);
		goto fail;
	}

	/* Step 4. If the rightmost octet of em does not have hexadecimal
	 * value 0xbc, output "inconsistent" and stop.
	 */
	if (em[em_len - 1UL] != 0xbcUL) {
		ret = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 1);
		goto fail;
	}

	/* Step 5. Let masked_db be the leftmost em_len - hlen - 1 octets
	 * of em, and let H be the next hlen octets.
	 */
	em_minus_hlen_minus_1 = em_len - hlen - 1UL;
	masked_dblen = em_minus_hlen_minus_1;
	masked_db = em;
	H = em + masked_dblen;

	/* Step 6. If the leftmost 8em_len - emBits bits of the leftmost
	 * octet in masked_db are not all equal to zero, output "inconsistent"
	 * and stop.
	 * 8em_len - emBits = 8*256 - 2047 = 1 (assuming 2048 key size and sha256)
	 */
	lowest_bits = (uint8_t)((8UL * em_len) - (rsa_keysize_bits - 1UL));
	first_octet_masked_db = masked_db[0] & (0xFFU << (8U - lowest_bits));
	if (first_octet_masked_db > 0U) {
		ret = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 2);
		goto fail;
	}

	/* Step 7. Let dbMask = MGF(H, em_len - hlen - 1). */

	ret = tegrabl_se_mask_generation(
		H, em_minus_hlen_minus_1, db_mask, hash_algorithm, hlen);

	if (ret != TEGRABL_NO_ERROR) {
		ret = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 3);
		goto fail;
	}

	/* Step 8. Let DB = masked_db XOR dbMask */
	for (i = 0; i < masked_dblen; i++) {
		db[i] = masked_db[i] ^ db_mask[i];
	}

	/* Step 9. Set the leftmost 8em_len - emBits bits of the leftmost
	 * octet in DB to zero.
	 */
	db[0] &= ~(0xFFU << (8U - lowest_bits));

	/* Step 10. If the em_len - hlen - slen - 2 leftmost octets of DB are not
	 * zero or if the octet at position em_len - hlen - slen - 1 (the leftmost
	 * or lower position is "position 1") does not have hexadecimal value
	 * 0x01, output "inconsistent" and stop.
	 */

	for (i = 0; (db[i] == 0U) && (i < (em_len - hlen - slen - 2UL)); i++) {
		if (db[i] != 0U) {
			ret = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 4);
			goto fail;
		}
	}
	/* if octet at position em_len - hlen - slen - 1
	 * e.g. 256 - 32 - 32 - 1 = 191th position
	 * position 191 is 190th element of the array, so subtract by 1 more.
	 */
	if (db[em_minus_hlen_minus_1 - slen - 1UL] != 0x1UL) {
		ret = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 5);
		goto fail;
	}

	/* Step 11. Let salt be the last slen octets of DB. */

	/* Step 12. Let M' = 0x 00 00 00 00 00 00 00 00 || mHash || salt;
	 * Set eight initial octets to 0.
	 */
	for (i = 0; i < 8UL; i++) {
		m_prime[i] = 0;
	}
	/* Copy salt to M_Prime. Note: DB is an octet string of length
	 * em_len - hlen - 1. Subtract slen from DB length to get salt location.
	 */
	for (i = 0; i < hlen; i++) {
		m_prime[i + 8UL] = pmessage_hash[i];
		m_prime[i + 8UL + hlen] = db[em_minus_hlen_minus_1 - slen + i];
	}

	/* Step 13. Let H' = Hash(M') */
	context.input_size = 8UL + hlen + slen;
	context.hash_algorithm = hash_algorithm;
	input.block_addr = (uintptr_t)m_prime;
	input.block_size = 8UL + hlen + slen;
	input.size_left = 8UL + hlen + slen;
	input.hash_addr = (uintptr_t)m_prime;

	ret = _tegrabl_se_sha_process_block(&input, &context);

	if (ret != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Step 14. If H = H' output "consistent".
	 * Otherwise, output "inconsistent".
	 */
	if (memcmp(H, (uint8_t *)m_prime, hlen) != 0) {
		ret = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 6);
		goto fail;
	}

fail:
	return ret;
}

tegrabl_error_t tegrabl_se_hash_and_rsa_pss_verify(
	uint8_t rsa_keyslot, uint32_t rsa_keysize_bits,
	uint32_t *pinput_signature, uint8_t hash_algorithm, uint32_t slen,
	uint32_t *pinput_message, uint32_t input_message_length_bytes)
{
	struct se_sha_input_params input;
	struct se_sha_context context;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	static uint8_t *pmessage_hash;
	uint32_t hlen = 0;
	uint32_t hash_algorithm_32 = (uint32_t)hash_algorithm;
	bool flag = false;

	if ((pinput_message == NULL) || (slen == 0U) ||
		(input_message_length_bytes == 0UL) || (pinput_signature == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}
	/* Other function parameters will be checked in
	 * tegrabl_se_rsa_pss_verify API.
	 */

	switch (hash_algorithm_32) {
	case SE_MODE_PKT_SHAMODE_SHA1:
		hlen =  ARSE_SHA1_HASH_SIZE / 8;
		break;
	case SE_MODE_PKT_SHAMODE_SHA224:
		hlen = ARSE_SHA224_HASH_SIZE / 8;
		break;
	case SE_MODE_PKT_SHAMODE_SHA256:
		hlen = ARSE_SHA256_HASH_SIZE / 8;
		break;
	case SE_MODE_PKT_SHAMODE_SHA384:
		hlen = ARSE_SHA384_HASH_SIZE / 8;
		break;
	case SE_MODE_PKT_SHAMODE_SHA512:
		hlen = ARSE_SHA512_HASH_SIZE / 8;
		break;
	default:
		flag = true;
		break;
	}
	if (flag) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (pmessage_hash == NULL) {
		pmessage_hash = tegrabl_alloc(TEGRABL_HEAP_DMA, hlen);
		if (pmessage_hash == NULL) {
			ret = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			goto fail;
		}
	}

	context.input_size = input_message_length_bytes;
	context.hash_algorithm = hash_algorithm;
	input.block_addr = (uintptr_t)pinput_message;
	input.block_size = input_message_length_bytes;
	input.size_left = input_message_length_bytes;
	input.hash_addr = (uintptr_t)pmessage_hash;
	ret = _tegrabl_se_sha_process_block(&input, &context);

	if (ret != TEGRABL_NO_ERROR) {
		goto fail;
	}
	ret = tegrabl_se_rsa_pss_verify(
		rsa_keyslot, rsa_keysize_bits, pmessage_hash,
		pinput_signature, hash_algorithm, slen);

	if (ret != TEGRABL_NO_ERROR) {
		goto fail;
	}
fail:
	return ret;
}

tegrabl_error_t tegrabl_se_clear_aes_keyslot(uint8_t keyslot)
{
	static uint32_t keydata[8];
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	uint32_t reg;

	if (keyslot >= SE_AES_MAX_KEYSLOTS) {
		goto fail;
	}

	/* Clear key */
	ret = tegrabl_se_aes_write_key_iv(keyslot,
					SE_MODE_PKT_AESMODE_KEY256,
					(uint8_t)SE_CRYPTO_KEYIV_PKT_WORD_QUAD_KEYS_0_3,
					keydata);
	if (ret != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Clear OIV */
	ret = tegrabl_se_aes_write_key_iv(keyslot,
					SE_MODE_PKT_AESMODE_KEY128,
					(uint8_t)SE_CRYPTO_KEYIV_PKT_WORD_QUAD_ORIGINAL_IVS,
					keydata);
	if (ret != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Clear UIV */
	ret = tegrabl_se_aes_write_key_iv(keyslot,
					(uint8_t)SE_MODE_PKT_AESMODE_KEY128,
					(uint8_t)SE_CRYPTO_KEYIV_PKT_WORD_QUAD_UPDATED_IVS,
					keydata);
	if (ret != TEGRABL_NO_ERROR) {
		goto fail;
	}
	/* Check err_status register to make sure keyslot write is success */
	reg = tegrabl_get_se0_reg(SE0_AES0_ERR_STATUS_0);
	if (reg != 0UL) {
		ret = TEGRABL_ERROR(TEGRABL_ERR_WRITE_FAILED, 0);
	}

fail:
	if (ret != TEGRABL_NO_ERROR) {
		pr_debug("Error = %d, in tegrabl_se_clear_aes_keyslot\n", ret);
	}
	return ret;
}

tegrabl_error_t tegrabl_se_read_lock_aes_keyslot(uint8_t keyslot)
{
	uint32_t reg = 0;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (keyslot >= AES_KEYSLOT_MAX) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_AES_KEYSLOT);
		goto fail;
	}

	reg =
		tegrabl_get_se0_reg((uint32_t)SE0_AES0_CRYPTO_KEYTABLE_ACCESS_0 +
																(keyslot * 4UL));

	/* lock the given keyslot */
	reg =
	NV_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_KEYTABLE_ACCESS, KEYREAD, DISABLE, reg);

	tegrabl_set_se0_reg((uint32_t)SE0_AES0_CRYPTO_KEYTABLE_ACCESS_0 +
															(keyslot * 4UL), reg);

fail:
	return err;
}

tegrabl_error_t tegrabl_se_read_lock_rsa_keyslot(uint8_t keyslot)
{
	uint32_t reg = 0;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (keyslot >= PKA0_KEYSLOT_MAX) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_RSA_KEYSLOT);
		goto fail;
	}

	reg =
	tegrabl_get_se0_reg((uint32_t)SE0_RSA_KEYTABLE_ACCESS_0 + (keyslot * 4UL));

	/* lock the given keyslot */
	reg = NV_FLD_SET_DRF_DEF(SE0_RSA, KEYTABLE_ACCESS, KEYREAD, DISABLE, reg);

	tegrabl_set_se0_reg((uint32_t)SE0_RSA_KEYTABLE_ACCESS_0 + (keyslot * 4UL),
																		reg);

fail:
	return err;
}

/* Referred from #8 of Bug 1864615 */
static void tegrabl_se_pre_configure_drbg(void)
{
	uint32_t val;

	tegrabl_get_se0_mutex();

	val =
		NV_DRF_DEF(SE0_AES0, RNG_SRC_CONFIG, RO_ENTROPY_SOURCE, ENABLE) |
		NV_DRF_DEF(SE0_AES0, RNG_SRC_CONFIG, RO_ENTROPY_SOURCE_LOCK, ENABLE);
	tegrabl_set_se0_reg(SE0_AES0_RNG_SRC_CONFIG_0, val);

	val = NV_DRF_NUM(SE0_AES0, RNG_RESEED_INTERVAL, VAL, 0);
	tegrabl_set_se0_reg(SE0_AES0_RNG_RESEED_INTERVAL_0, val);

	tegrabl_release_se0_mutex();
}

/* Referred from section 3.14.7.4.1 of SE_Unit_IAS.docx */
static tegrabl_error_t tegrabl_se_init_drbg(uint8_t dst, uint8_t *buffer,
											uint8_t num_of_blocks)
{
	uint32_t val;
	uint64_t dst_addr;
	uint32_t dst_32 = (uint32_t)dst;

	tegrabl_error_t error = TEGRABL_NO_ERROR;

	tegrabl_get_se0_mutex();

	if (!tegrabl_se_is_dst_valid(dst)) {
		pr_error("Invalid destination passed\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	switch (dst_32) {

	case SE0_AES0_CONFIG_0_DST_MEMORY:

		if (buffer == NULL) {
			pr_error("Invalid buffer passed for DMA map\n");
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}

		error = tegrabl_se_setup_op_mode(SE_OPR_MODE_AES_RNG,
										 true,
										 false,
										 (uint8_t)SE0_AES0_CONFIG_0_DST_MEMORY,
										 0,
										 (uint8_t)SE_MODE_PKT_AESMODE_KEY128);

		/* Program RNG source and mode of operation */
		val =
			NV_DRF_DEF(SE0_AES0, RNG_CONFIG, SRC, ENTROPY) |
			NV_DRF_DEF(SE0_AES0, RNG_CONFIG, MODE, FORCE_INSTANTION);
		tegrabl_set_se0_reg(SE0_AES0_RNG_CONFIG_0, val);

		/* Program num of random vectors to be generated */
		val = NV_DRF_NUM(SE0_AES0, CRYPTO_LAST_BLOCK, VAL, num_of_blocks - 1U);
		tegrabl_set_se0_reg(SE0_AES0_CRYPTO_LAST_BLOCK_0, val);

		dst_addr = tegrabl_dma_map_buffer(TEGRABL_MODULE_SE,
									 0,
									 (void *)buffer,
						(uint32_t)num_of_blocks * SE_AES_BLOCK_LENGTH,
								TEGRABL_DMA_FROM_DEVICE);
		/* Program 32-bit LSB of 40-bit address */
		tegrabl_set_se0_reg(SE0_AES0_OUT_ADDR_0, (uint32_t)dst_addr);


		/* Program 8-bit MSB of 40-bit address */
		val =
			NV_DRF_NUM(SE0_AES0, OUT_ADDR_HI, MSB, dst_addr >> 32) |
			NV_DRF_NUM(SE0_AES0, OUT_ADDR_HI, SZ,
					   num_of_blocks * SE_AES_BLOCK_LENGTH);
		tegrabl_set_se0_reg(SE0_AES0_OUT_ADDR_HI_0, val);

		break;

	case SE0_AES0_CONFIG_0_DST_SRK:

		error = tegrabl_se_setup_op_mode(SE_OPR_MODE_AES_RNG,
										 true,
										 false,
										 (uint8_t)SE0_AES0_CONFIG_0_DST_SRK,
										 0,
										 (uint8_t)SE_MODE_PKT_AESMODE_KEY128);

		/* Program num of random vectors to be generated */
		val = NV_DRF_NUM(SE0_AES0, CRYPTO_LAST_BLOCK, VAL, num_of_blocks - 1U);
		tegrabl_set_se0_reg(SE0_AES0_CRYPTO_LAST_BLOCK_0, val);

		/* Program RNG mode of operation */
		val =
			NV_DRF_DEF(SE0_AES0, RNG_CONFIG, SRC, ENTROPY) |
			NV_DRF_DEF(SE0_AES0, RNG_CONFIG, MODE, FORCE_RESEED);
		tegrabl_set_se0_reg(SE0_AES0_RNG_CONFIG_0, val);

		break;

	default:
		pr_error("Unsupported destination\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		break;
	}

fail:
	tegrabl_release_se0_mutex();

	return error;
}

tegrabl_error_t tegrabl_se_generate_random_num(void)
{
	uint8_t *buffer = NULL;
	uint8_t num_of_blocks = 1;  /* Generate only 1 random vector */
	bool is_engine_busy;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	tegrabl_se_pre_configure_drbg();

	/* Set output data buffer */
	buffer = tegrabl_alloc(TEGRABL_HEAP_DMA, SE_AES_BLOCK_LENGTH);
	if (buffer == NULL) {
		pr_error("Failed to allocate memory\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto done;
	}

	/* Generate random number in memory */
	error = tegrabl_se_init_drbg((uint8_t)SE0_AES0_CONFIG_0_DST_MEMORY,
							buffer, num_of_blocks);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Failed to set DRBG's destination as memory\n");
		goto fail;
	}

	error = tegrabl_start_se0_operation(ARSE_ENG_IDX_AES0, true);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Failed to start operation\n");
		goto fail;
	}

	/* Poll for op done */
	is_engine_busy = true;
	while (is_engine_busy) {
		error = tegrabl_is_se0_engine_busy(ARSE_ENG_IDX_AES0, &is_engine_busy);
		if (error != TEGRABL_NO_ERROR) {
			pr_error("Failed to check se0 engine busy\n");
			break;
		}
	}

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SE,
							 0,
							 (void *)&buffer,
					(uint32_t)num_of_blocks * SE_AES_BLOCK_LENGTH,
							 TEGRABL_DMA_FROM_DEVICE);

	/* Generate random number in SRK */
	error = tegrabl_se_init_drbg((uint8_t)SE0_AES0_CONFIG_0_DST_SRK, NULL,
								 num_of_blocks);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Failed to set DRBG's destination as SRK\n");
		goto fail;
	}

	error = tegrabl_start_se0_operation(ARSE_ENG_IDX_AES0, true);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Failed to start operation\n");
		goto fail;
	}

	is_engine_busy = true;
	/* Poll for op done */
	while (is_engine_busy) {
	    error = tegrabl_is_se0_engine_busy(ARSE_ENG_IDX_AES0, &is_engine_busy);
		if (error != TEGRABL_NO_ERROR) {
			pr_error("Failed to check se0 engine busy\n");
			break;
		}
	}

fail:
	tegrabl_dealloc(TEGRABL_HEAP_DMA, buffer);

done:
	return error;
}
