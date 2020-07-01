/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#define MODULE TEGRABL_ERR_FUSE

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_drf.h>
#include <tegrabl_debug.h>
#include <tegrabl_io.h>
#include <tegrabl_clock.h>
#include <tegrabl_fuse.h>
#include <arfuse.h>
#include <tegrabl_timer.h>
#include <tegrabl_fuse_bitmap.h>
#include <tegrabl_malloc.h>
#include <tegrabl_soc_misc.h>
#include <tegrabl_aotag.h>

/*
 * The range is limited to 4C - 101C because we are using AOTAG sensor to
 * read the temperature. Refer http://nvbugs/200357438/2 for details.
 */

#define FUSE_BURNING_TEMPERATURE_MIN 4000
#define FUSE_BURNING_TEMPERATURE_MAX 101000

#define H2_START_MACRO_BIT_INDEX 2167
#define H2_END_MACRO_BIT_INDEX 3326

#define DEBUG_FUSEWRITE 0

#define GENERIC_FUSE_DEBUG  0/* Print out fuse info for debug */

/* these names should match with enum values in tegrabl_fuse.h */
static char *tegrabl_map_fusename_to_type[] = {
	"BootSecurityInfo",
	"SecBootDeviceSelect",
	"Uid",
	"Sku",
	"Tid",
	"CpuSpeedo0",
	"CpuSpeedo1",
	"CpuSpeedo2",
	"CpuIddq",
	"SocSpeedo0",
	"SocSpeedo1",
	"SocSpeedo2",
	"EnabledCpuCores",
	"Apb2JtagDisable",
	"TpcDisable",
	"Apb2JtagLock",
	"SocIddq",
	"SataNvCalib",
	"SataMphyOdmCalib",
	"Tsensor9Calib",
	"TsensorCommonT1",
	"TsensorCommonT2",
	"TsensorCommonT3",
	"HyperVoltaging",
	"ReservedCalib0",
	"OptPrivSecEn",
	"UsbCalib",
	"UsbCalibExt",
	"ProductionMode",
	"SecurityMode",
	"OdmLock",
	"JtagDisable",
	"ReservedOdm0",
	"ReservedOdm1",
	"ReservedOdm2",
	"ReservedOdm3",
	"ReservedOdm4",
	"ReservedOdm5",
	"ReservedOdm6",
	"ReservedOdm7",
	"Kek256",
	"Kek2",
	"PublicKeyHash",
	"SecureBootKey",
	"ReservedSw",
	"BootDevSelect",
	"IgnoreBootDevStraps",
	"BootDevInfo",
	"SecureProvisionInfo",
	"Kek0",
	"Kek1",
	"EndorsementKey",
	"OdmId",
	"H2",
	"Odm_info",
	"DebugAuthentication",
	"CcplexDfdAccessDisable",
	"Unknown",
};

static uint32_t tegrabl_fuse_calculate_parity(uint32_t value)
{
	uint32_t count = 0;
	uint32_t parity = 0;
	for (count = 0; count < 32; count++) {
		parity ^= ((value >> count) & 0x1U);
	}
	return parity;
}

static uint32_t tegrabl_fuse_generate_fuse_h2_ecc(void)
{
	uint32_t start_row_index = 0;
	uint32_t start_bit_index = 0;
	uint32_t end_row_index = 0;
	uint32_t end_bit_index = 0;
	uint32_t bit_index = 0;
	uint32_t row_index = 0;
	uint32_t row_data = 0;
	uint32_t hamming_value = 0;
	uint32_t pattern = 0x7ff;
	uint32_t parity = 0;
	uint32_t hamming_code = 0;

	start_row_index = H2_START_MACRO_BIT_INDEX / 32;
	start_bit_index = H2_START_MACRO_BIT_INDEX % 32;
	end_row_index = H2_END_MACRO_BIT_INDEX / 32;
	end_bit_index = H2_END_MACRO_BIT_INDEX % 32;

	for (row_index = start_row_index;
		row_index <= end_row_index; row_index++) {
		row_data = tegrabl_fuserdata_read(row_index);
		pr_debug("fuse :%0x has value :%0x\n", row_index, row_data);
		for (bit_index = 0; bit_index < 32; bit_index++) {
			pattern++;
			if ((row_index == start_row_index) &&
				(bit_index < start_bit_index)) {
				continue;
			}
			if ((row_index == end_row_index) &&
				(bit_index < end_bit_index)) {
				continue;
			}
			if (((row_data >> bit_index) & 0x1) != 0) {
				hamming_value ^= pattern;
			}
		}
	}
	parity = tegrabl_fuse_calculate_parity(hamming_value);
	hamming_code = hamming_value | (1 << 12) | (parity ^ 1 << 13);

	pr_debug("hamming code value is %0x\n", hamming_code);
	return hamming_code;
}

/* validate fuse according magic ID */
static bool fuse_info_validate(uint8_t *buffer, uint32_t bufsize)
{
	uint32_t *magic_id = NULL;

	if ((buffer == NULL) || (bufsize < sizeof(struct fuse_info_header)))
		return false;

	magic_id = (uint32_t *)buffer;
	if (*magic_id != MAGICID_FUSE)
		return false;

	return true;

    /* TODO: Add a data integrity check (CRC/MD5) */
}

/* parse fuse buffer received */
static
bool parse_fuse_info(uint8_t *buffer, struct fuse_info *pinfo)
{
	uint32_t psize = 0; /* parsed size */
	uint32_t i = 0;
	uint32_t fuse_size = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	/* parse header field */
	pinfo->head = (struct fuse_info_header *)buffer;
	psize += sizeof(struct fuse_info_header);

	/* parse fuse nodes field */
	pinfo->nodes = (struct fuse_node *)(buffer + psize);
	psize += pinfo->head->fusenum * sizeof(struct fuse_node);

	/* parse data field */
	pinfo->data = (uint32_t *)(buffer + psize);

	/* checkout parse size */
	struct fuse_node *pnode = pinfo->nodes;
	while (i < pinfo->head->fusenum) {
		psize += pnode->size;
		pr_info("index : %d  fuse: %s size:%d\n",
			i, tegrabl_map_fusename_to_type[pnode->type], pnode->size);
		error = tegrabl_fuse_query_size(pnode->type, &fuse_size);
		if (error != TEGRABL_NO_ERROR) {
			pr_error("size query for %s fuse failed\n",
				tegrabl_map_fusename_to_type[pnode->type]);
			return false;
		} else {
			if (fuse_size != pnode->size) {
				pr_error("fuse : %s  size mismatch \n",
					tegrabl_map_fusename_to_type[pnode->type]);
				pr_error("Xml value is %d and query size value is %d\n",
					pnode->size, fuse_size);
				return false;
			}
		}
		pnode++;
		i++;
	}

	if (psize != pinfo->head->infosize) {
		pr_info("Parse failed at %dB, fuseblob has %dB\n",
			psize, pinfo->head->infosize);
		return false;
	}

	return true;
}

/* verify burnt fuses by reading fuses back and compare */
static tegrabl_error_t verify_burnt_fuses(struct fuse_info *pinfo)
{
	uint32_t data[FUSEDATA_MAXSIZE];
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	struct fuse_node *pnode;
	uint32_t *pdata;
	uint32_t i = 0;

	pnode = pinfo->nodes;
	pdata = pinfo->data;
	while (i < pinfo->head->fusenum) {
		if (pnode->type != FUSE_SECURITY_MODE) {
			if (pnode->type == FUSE_BOOT_SECURITY_INFO) {
				data[0] = tegrabl_fuse_get_security_info();
				pr_info("security info fuse val :0x%0x\n", data[0]);
			} else {
				e = tegrabl_fuse_read(pnode->type, data, pnode->size);
				if (e) {
					pr_error("Read fuse type failed\n");
					goto fail;
				}
			}
			pr_debug("fuse value is %0x\n",data[0]);
			pr_debug("fuse blob value is %0x\n",*pdata);
			e = memcmp(data, pdata, pnode->size);
			if (e) {
				uint32_t count = 0;
				pr_error("fuse : %s value mismatch\n",
					tegrabl_map_fusename_to_type[pnode->type]);
				for (count = 0; count < pnode->size; count += 4)
					pr_info("expected :0x%0x read: 0x%0x\n ",
						pdata[count], data[count]);
				goto fail;
			}
		}

		pdata += pnode->size >> 2;
		pnode++;
		i++;
	}

	fail:
		return e;
}

#if GENERIC_FUSE_DEBUG
/* Print out all the fuses info in generic fuse info */
static void print_fuse_info(struct fuse_info *pinfo)
{
	uint32_t i = 0;
	uint32_t j = 0;
	struct fuse_node *pnode;
	uint32_t *pdata;

	pr_info("Print fuse info to burn:\n");

	pnode = pinfo->nodes;
	pdata = pinfo->data;
	while (i < pinfo->head->fusenum) {
		pr_info("fuse: type=%d size=%d\n", pnode->type, pnode->size);
		j = 0;
		while (j < (pnode->size)) {
			pr_info("data[%d]=%x\n", j, pdata[j]);
			j ++;
		}

		pdata += pnode->size;
		pnode ++;
		i++;
	}
	return ;
}

/* Read back all the burnt fuses and print out */
static tegrabl_error_t print_burnt_fuses(struct fuse_info *pinfo)
{
	uint32_t Data[FUSEDATA_MAXSIZE];
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	struct fuse_node *pnode;
	uint32_t i = 0;
	uint32_t j;

	pr_info("Print burnt fuse info:\n");

	pnode = pinfo->nodes;
	while (i < pinfo->head->fusenum) {
		pr_info("fuse: type=%d size=%x\n", pnode->type, pnode->size);
		e = tegrabl_fuse_read(pnode->type, Data, pnode->size);
		if (e) {
			pr_error("Read fuse failed for %d\n", pnode->type);
			return e;
		}

		j = 0;
		while (j < (pnode->size >> 2)) {
			pr_debug("data[%d]=%x\n", j, Data[j]);
			j++;
		}

		pnode++;
		i++;
	}

	return e;
}
#endif /* GENERIC_FUSE_DEBUG */

/* burn all fuses decribed in generic fuse info
 * the info could include any fuse user need
 */
tegrabl_error_t burn_fuses(uint8_t *buffer, uint32_t bufsize)
{
	bool ret;
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	struct fuse_info fuseinfo;
	struct fuse_node *pnode;
	uint32_t *pdata;
	uint8_t *buffer_local = NULL;
	uint32_t i = 0;
	uint32_t secmdata = 0;
	uint32_t burnsecm = 0;
#if defined(CONFIG_ENABLE_AOTAG)
	/* check if the temperature is in permissable range */
	e = tegrabl_aotag_verify_temperature_range(FUSE_BURNING_TEMPERATURE_MIN,
			FUSE_BURNING_TEMPERATURE_MAX);

	if (e != TEGRABL_NO_ERROR) {
		pr_error("temperature is out of range for fuse burning\n");
		goto fail;
	}
#endif

	if (!fuse_info_validate(buffer, bufsize))
		return false;

	pr_info("Parsing fuse information \n");
	ret = parse_fuse_info(buffer, &fuseinfo);
	if (ret) {
		pr_info("Fuse Blob found\n");
	} else {
		pr_info("Fuse Blob not found\n");
		e = TEGRABL_ERR_BAD_PARAMETER;
		goto fail;
	}
	buffer_local = tegrabl_malloc(bufsize);

	memcpy(buffer_local,buffer, bufsize);

	if (!fuse_is_nv_production_mode()) {
		pr_info("NvProduction Fuse not burnt\n");
		e = TEGRABL_ERR_INVALID;
		goto fail;
	}

	pnode = fuseinfo.nodes;
	pdata = fuseinfo.data;
	pr_info("Burning fuses\n");
	for (i = 0; i < fuseinfo.head->fusenum; i++) {
		/* burn SecurityMode at last */
		if (pnode->type != FUSE_SECURITY_MODE) {
			pr_debug("data is 0x%0x\n", *pdata);
			e = tegrabl_fuse_write(pnode->type, (uint32_t *)pdata, pnode->size);
			if (e != TEGRABL_NO_ERROR) {
				pr_error("Fuse value set failed for %s\n",
					tegrabl_map_fusename_to_type[pnode->type]);
				goto fail;
			}
			pr_info("fuse : %s burnt successfully\n",
				tegrabl_map_fusename_to_type[pnode->type]);
		} else {
			secmdata = *(uint32_t *)pdata;
			burnsecm = 4;
		}
		pdata += pnode->size >> 2;
		pnode++;
	}

	ret = parse_fuse_info(buffer_local, &fuseinfo);
	if (ret) {
		pr_info("Fuse Blob found\n");
	} else {
		pr_info("Fuse Blob not found\n");
		e = TEGRABL_ERR_BAD_PARAMETER;
		goto fail;
	}
	pr_info("verifying burnt fuses\n");
	e = verify_burnt_fuses(&fuseinfo);
	if (e != TEGRABL_NO_ERROR) {
		pr_info("Failed to verify burnt fuse\n");
		goto fail;
	}
	if (burnsecm) {
		uint32_t fuse_h2_value = 0;

		fuse_h2_value = tegrabl_fuse_generate_fuse_h2_ecc();
		pr_info("Hamming ECC value is %0x\n", fuse_h2_value);

		e = tegrabl_fuse_write(FUSE_H2, &fuse_h2_value, burnsecm);
		if (e != TEGRABL_NO_ERROR) {
			pr_error("Hamming ECC fuse set failed\n");
			goto fail;
		}

		e = tegrabl_fuse_write(FUSE_SECURITY_MODE,
			&secmdata, burnsecm);
		if (e != TEGRABL_NO_ERROR) {
			pr_error("Security mode fuse set failed\n");
			goto fail;
		}
	}
	pr_info("Successfully Burnt Fuses as per fuse info\n");
	*(uint32_t *)buffer = (uint32_t)e;

fail:
	if (buffer_local)
		tegrabl_free(buffer_local);
	if (e != TEGRABL_NO_ERROR)
		pr_error("Failed to Burn Fuses as per fuse info\n");

	return e;
}

tegrabl_error_t get_fuse_value(uint8_t *buffer, uint32_t bufsize,
	enum fuse_type type, uint32_t *fuse_value)
{
	bool ret;
	tegrabl_error_t e = TEGRABL_NO_ERROR;
	struct fuse_info fuseinfo;
	struct fuse_node *pnode;
	uint32_t *pdata;
	uint32_t i = 0;

	pr_info("bufsize is %d\n", bufsize);
	if (!fuse_info_validate(buffer, bufsize)) {
		e = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 1);
		goto fail;
	}

	ret = parse_fuse_info(buffer, &fuseinfo);
	if (ret) {
		pr_info("Fuse Blob found\n");
	} else {
		pr_info("Fuse Blob not found\n");
		e = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 2);
		goto fail;
	}

	pnode = fuseinfo.nodes;
	pdata = fuseinfo.data;
	for (i = 0; i < fuseinfo.head->fusenum; i++) {
		if (pnode->type == type) {
			pr_info("requested fuse value found\n");
			memcpy(fuse_value, pdata, pnode->size);
			break;
		}
		pdata += pnode->size >> 2;
		pnode++;
	}
	if (i >= fuseinfo.head->fusenum)
		e = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 3);
fail:
	return e;
}
