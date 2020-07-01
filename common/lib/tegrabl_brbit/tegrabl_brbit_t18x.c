/*
 * Copyright (c) 2015, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_BRBIT
#define NVBOOT_TARGET_FPGA 0

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <tegrabl_utils.h>
#include <tegrabl_debug.h>
#include <tegrabl_brbit.h>
#include <tegrabl_brbit_core.h>
#include <nvboot_bit.h>
#include <nvboot_version_defs.h>

#define TEGRABL_BRBIT_LOCATION 0xD480000UL
#define TEGRABL_AVP_CACHE_LINE_SIZE 32
#define TEGRABL_AVP_CACHE_LINE_MASK (TEGRABL_AVP_CACHE_LINE_SIZE - 1)
#define TEGRABL_CHIP 0x18

#define sizeoff(st, m) ((int)(sizeof((st *)0)->m));

void *tegrabl_brbit_location(void)
{
	uint64_t address = 0;

	address = ((TEGRABL_BRBIT_LOCATION + TEGRABL_AVP_CACHE_LINE_MASK) &
					(~TEGRABL_AVP_CACHE_LINE_MASK));

	return (void *)(intptr_t) address;
}

bool tegrabl_brbit_verify(void *buffer)
{
	NvBootInfoTable *boot_info = (NvBootInfoTable *)buffer;

	if (((boot_info->BootRomVersion == NVBOOT_VERSION(TEGRABL_CHIP, 0x01)) ||
		 (boot_info->BootRomVersion == NVBOOT_VERSION(TEGRABL_CHIP, 0x02))) &&
		 (boot_info->DataVersion    == NVBOOT_VERSION(TEGRABL_CHIP, 0x01)) &&
		 (boot_info->RcmVersion     == NVBOOT_VERSION(TEGRABL_CHIP, 0x01)) &&
		 (boot_info->PrimaryDevice  == NvBootDevType_Irom)) {
		return true;
	}

	return false;
}

tegrabl_error_t tegrabl_brbit_get_offset_size(enum tegrabl_brbit_data_type type,
		uint32_t instance, uint32_t *offset, uint32_t *size)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	TEGRABL_UNUSED(instance);

	if (!offset || !size) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	switch (type) {
	case TEGRABL_BRBIT_DATA_BRBIT:
		*offset = 0;
		*size = sizeof(NvBootInfoTable);
		break;
	case TEGRABL_BRBIT_DATA_BOOT_TYPE:
		*offset = offsetof(NvBootInfoTable, BootType);
		*size = sizeoff(NvBootInfoTable, BootType);
		break;
	case TEGRABL_BRBIT_DATA_SAFE_START_ADDRESS:
		*offset = offsetof(NvBootInfoTable, SafeStartAddr);
		*size = sizeoff(NvBootInfoTable, SafeStartAddr);
		break;
	case TEGRABL_BRBIT_DATA_ACTIVE_BCT_PTR:
		*offset = offsetof(NvBootInfoTable, BctPtr);
		*size = sizeoff(NvBootInfoTable, BctPtr);
		break;
	case TEGRABL_BRBIT_DATA_ACTIVE_BCT_BLOCK:
		*offset = offsetof(NvBootInfoTable, BctBlock);
		*size = sizeoff(NvBootInfoTable, BctBlock);
		break;
	case TEGRABL_BRBIT_DATA_ACTIVE_BCT_SECTOR:
		*offset = offsetof(NvBootInfoTable, BctPage);
		*size = sizeoff(NvBootInfoTable, BctPage);
		break;
	case TEGRABL_BRBIT_DATA_BCT_SIZE:
		*offset = offsetof(NvBootInfoTable, BctSize);
		*size = sizeoff(NvBootInfoTable, BctSize);
		break;
	case TEGRABL_BRBIT_DATA_BL_STATUS:
		if (instance >= NVBOOT_MAX_BOOTLOADERS) {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}
		*offset = offsetof(NvBootInfoTable, BlState[instance]);
		*size = sizeoff(NvBootInfoTable, BlState[instance]);
		break;

	default:
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}

fail:
	return error;
}

