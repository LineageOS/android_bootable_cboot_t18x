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

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
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

/* only used for PKA1 slot operations */

#define SE_PKA1_CTRL_KSLT_ADDR_FIELD_RSA_EXPONENT 0x00000000
#define SE_PKA1_CTRL_KSLT_ADDR_FIELD_RSA_R_SQUARE 0x00000003
#define SE_PKA1_CTRL_KSLT_DATA(i) (0x00008810+((i)*4))
#define SE_PKA1_CTRL_KSLT_ADDR(i) (0x00008800+((i)*4))

#define SE_PKA1_KEYSLOT_ADDR_0_FLD_SHIFT _MK_ENUM_CONST(8)
#define SE_PKA1_KEYSLOT_ADDR_0_FLD_FIELD \
	_MK_FIELD_CONST(0xf, SE_PKA1_KEYSLOT_ADDR_0_FLD_SHIFT)
#define SE_PKA1_KEYSLOT_ADDR_0_FLD_RANGE 11:8

#define SE_PKA1_KEYSLOT_ADDR_0_WORD_ADDR_SHIFT _MK_ENUM_CONST(0)
#define SE_PKA1_KEYSLOT_ADDR_0_WORD_ADDR_FIELD \
	_MK_FIELD_CONST(0x3, SE_PKA1_KEYSLOT_ADDR_0_WORD_ADDR_SHIFT)
#define SE_PKA1_KEYSLOT_ADDR_0_WORD_ADDR_RANGE 6:0

#define TEGRABL_PKA1_KEYSLOT_MAX_BIT_SIZE 4096
#define TEGRABL_PKA1_KEYSLOT_MAX_BYTE_SIZE (TEGRABL_PKA1_KEYSLOT_MAX_BIT_SIZE/8)
#define TEGRABL_PKA1_KEYSLOT_MAX_WORD_SIZE (TEGRABL_PKA1_KEYSLOT_MAX_BYTE_SIZE/4)

#define MAX_PKA1_KEYSLOTS 4

#ifdef PKA_DEBUG
/* Reads pka register */
static uint32_t tegrabl_get_pka1_reg(uint32_t reg)
{
	return NV_READ32(NV_ADDRESS_MAP_PKA1_BASE + reg);
}
#endif

/* Writes data into pka register */
static void tegrabl_set_pka1_reg(uint32_t reg, uint32_t data)
{
	NV_WRITE32(NV_ADDRESS_MAP_PKA1_BASE + reg, data);
}

void
tegrabl_pka1_write_keyslot(uint32_t *data, uint32_t keyslot)
{
	uint32_t i = 0;
	uint32_t j = 0;
	uint32_t reg_addr = 0;

	if (keyslot >= MAX_PKA1_KEYSLOTS) {
		pr_error("Invalid key slot value passed\n");
		return;
	}

	for(i = SE_PKA1_CTRL_KSLT_ADDR_FIELD_RSA_EXPONENT;
		i <= SE_PKA1_CTRL_KSLT_ADDR_FIELD_RSA_R_SQUARE;
		i++) {
		reg_addr = NV_FLD_SET_DRF_NUM(SE_PKA1, KEYSLOT_ADDR, FLD, i,
									  reg_addr);
		for(j = 0; j < TEGRABL_PKA1_KEYSLOT_MAX_WORD_SIZE; j++) {
			reg_addr = NV_FLD_SET_DRF_NUM(SE_PKA1, KEYSLOT_ADDR, WORD_ADDR, j,
										  reg_addr);
			tegrabl_set_pka1_reg(SE_PKA1_CTRL_KSLT_ADDR(keyslot), reg_addr);
			tegrabl_set_pka1_reg(SE_PKA1_CTRL_KSLT_DATA(keyslot),
								 data == 0 ? 0 : *data++);
		}
	}
}
