 /*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_SE_CRYPTO

#include "build_config.h"
#include <tegrabl_se_helper.h>
#include <tegrabl_error.h>
#include <stdint.h>
#include <stdbool.h>

tegrabl_error_t tegrabl_se_swap(uint8_t *str, uint32_t one, uint32_t two)
{
	uint8_t temp;

	if (str == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}
	temp = str[one];
	str[one] = str[two];
	str[two] = temp;
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_se_change_endian(uint8_t *str, uint32_t size)
{
	uint32_t i;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	if (str == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}


	for (i = 0; i < size; i += 4UL) {
		ret = tegrabl_se_swap(str, i, i + 3UL);
		if (ret != TEGRABL_NO_ERROR) {
			goto fail;
		}
		ret = tegrabl_se_swap(str, i + 1UL, i + 2UL);
		if (ret != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

fail:
	return ret;
}

tegrabl_error_t tegrabl_se_reverse_list(
	uint8_t *original, uint32_t list_size)
{
	uint32_t i, j;
	uint32_t *list_32p = (uint32_t *)original;
	uint32_t temp1, temp;

	if (original == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	i = 0;
	for (j = ((list_size / 4U) - 1UL); i < j; j--) {
		temp = list_32p[i];
		temp = ((temp >> 24)  & 0xFFU) | ((temp >> 8) & 0xFF00U) |
			((temp << 24) & 0xFF000000U) | ((temp  << 8) & 0xFF0000U);
		temp1 = list_32p[j];
		temp1 = ((temp1 >> 24)  & 0xFFU) | ((temp1 >> 8) & 0xFF00U) |
			((temp1 << 24) & 0xFF000000U) | ((temp1  << 8) & 0xFF0000U);
		list_32p[i] = temp1;
		list_32p[j] = temp;
		i++;
	}
	return TEGRABL_NO_ERROR;
}

bool tegrabl_se_is_msb_set(uint8_t val)
{
	uint8_t flag;

	flag = (val >> 7) & 0x01U;
	if (flag != 0U)
		return true;
	else
		return false;
}

tegrabl_error_t tegrabl_se_left_shift_one_bit(uint8_t *in_buf, uint32_t size)
{
	uint32_t i;

	if (in_buf == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	/* left shift one bit */
	for (i = 0; i < size; i++) {
		in_buf[i] <<= 1;

		if (i < (size - 1UL)) {
			if (tegrabl_se_is_msb_set(in_buf[i + 1UL]))
				in_buf[i] |= 0x1U;
			else
				in_buf[i] |= 0x0U;
		}
	}

	return TEGRABL_NO_ERROR;
}

uint32_t tegrabl_se_swap_bytes(const uint32_t value)
{
	uint32_t tmp = (value << 16) | (value >> 16); /* Swap halves */
	/* Swap bytes pairwise */
	tmp = ((tmp >> 8) & 0x00FF00FFU) | ((tmp & 0x00FF00FFU) << 8);
	return tmp;
}
#if defined(CONFIG_ENABLE_ECDSA)
bool
tegrabl_se_cmp_bigunsignedint_is_zero(uint8_t *value, uint32_t value_size)
{
	uint32_t i;

	if (value_size == 0) {
		return false;
	}
	for (i = 0; i < value_size; i++) {
		if (value[i] != 0) {
			return false;
		}
	}
	return true;
}


int32_t
tegrabl_se_cmp_bigunsignedint(uint8_t *value1, uint8_t *value2, uint32_t size)
{
	uint32_t i;

	if (size == 0) {
		return TEGRABL_BIGINT_INPUT_ERROR;
	}
	for (i = size; i > 0; i--) {
		if (value1[i-1] > value2[i-1]) {
			return TEGRABL_BIGINT_GT;
		} else if (value1[i-1] < value2[i-1]) {
			return TEGRABL_BIGINT_LT;
		}
	}
	return TEGRABL_BIGINT_EQ;
}
#endif

