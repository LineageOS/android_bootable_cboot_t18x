/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef TEGRABL_SE_HELPER_H
#define TEGRABL_SE_HELPER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <tegrabl_error.h>

#define TEGRABL_BIGINT_LT -1
#define TEGRABL_BIGINT_EQ 0
#define TEGRABL_BIGINT_GT 1
#define TEGRABL_BIGINT_ZERO 0
#define TEGRABL_BIGINT_NONZERO 1
#define TEGRABL_BIGINT_INPUT_ERROR -2

/*
 * @brief swap bytes in given two indexes of a buffer
 *
 * @param str pointer to buffer
 * @param one index of a byte in the buffer
 * @param two index of a byte in the buffer
 *
 * @return error out if any
 */
tegrabl_error_t tegrabl_se_swap(uint8_t *str, uint32_t one, uint32_t two);

/*
 * @brief change indianness of given buffer into little-endian format
 *
 * @param str pointer to buffer
 * @param size size of input buffer
 *
 * @return error out if any
 */
tegrabl_error_t tegrabl_se_change_endian(uint8_t *str, uint32_t size);

/*
 * @brief reverse the given input buffer
 *
 * @param original pointer to buffer
 * @param list_size size of input buffer
 *
 * @return error out if any
 */
tegrabl_error_t tegrabl_se_reverse_list(
	uint8_t *original, uint32_t list_size);

/*
 * @brief check whether MSB is set in given input value
 *
 * @param val input value in which MSB to be checked
 *
 * @return true if MSB is set
 *	false if MSB is not set
 */
bool tegrabl_se_is_msb_set(uint8_t val);

/*
 * @brief left shift entire input buffer by 1 bit
 *
 * @param in_buf pointer to buffer
 * @param size size of input buffer
 *
 * @return error out if any
 */
tegrabl_error_t tegrabl_se_left_shift_one_bit(
	uint8_t *in_buf, uint32_t size);

/*
 * @brief swap bytes in given input 32-bit value
 *	ex: 0xABCD becomes 0xDCBA
 *
 * @param value input 32-bit value
 *
 * @return swapped 32-bit value
 */
uint32_t tegrabl_se_swap_bytes(const uint32_t value);

bool
tegrabl_se_cmp_bigunsignedint_is_zero(uint8_t *value, uint32_t value_size);

int32_t
tegrabl_se_cmp_bigunsignedint(uint8_t *value1, uint8_t *value2, uint32_t size);

#endif

