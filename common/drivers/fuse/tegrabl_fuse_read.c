/*
 * Copyright (c) 2015-2019, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_FUSE

#include <stdint.h>
#include <stdbool.h>
#include <tegrabl_error.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_drf.h>
#include <tegrabl_debug.h>
#include <tegrabl_io.h>
#include <tegrabl_clock.h>
#include <tegrabl_fuse.h>
#include <arfuse.h>
#include <arpmc_impl.h>
#include <tegrabl_timer.h>

/* Stores the base address of the fuse module */
static uintptr_t fuse_base_address = NV_ADDRESS_MAP_FUSE_BASE;

/* Local macro for simplification */
#define NV_FUSE_READ(reg) NV_READ32((fuse_base_address + (uint32_t)(reg)))
#define NV_FUSE_WRITE(reg, val) NV_WRITE32((fuse_base_address + (uint32_t)(reg)), val)
#define PMC_IMPL_READ(reg) NV_READ32((NV_ADDRESS_MAP_PMC_IMPL_BASE + (reg)))
#define PMC_IMPL_WRITE(reg, val) \
	NV_WRITE32((NV_ADDRESS_MAP_PMC_IMPL_BASE + (reg)), val)

#define PUBKEY_SIZE_BYTES 32U
#define SBKKEY_SIZE_BYTES 16U
#define KEKKEY_SIZE_BYTES 32U
#define EKKEY_SIZE_BYTES 32U
#define KGKKEY_SIZE_BYTES 16U
#define KEK0KEY_SIZE_BYTES 16U
#define ODMID_SIZE_BYTES 8U
/*
 * Ideally, nvboot_fuse.h has to be included.
 * But, the MACROS in that file are redefined in
 * this file, ex: ECID_ECID0_0_WAFER_MASK hence,
 * creating a macro for the size of ECID key size.
 */
#define ECID_SIZE_BYTES 16U

#define ECID_ECID0_0_RSVD1_MASK 0x3FU
#define ECID_ECID0_0_Y_MASK 0x1FFU
#define ECID_ECID0_0_Y_RANGE 6U
#define ECID_ECID0_0_X_MASK 0x1FFU
#define ECID_ECID0_0_X_RANGE 15U
#define ECID_ECID0_0_WAFER_MASK 0x3FU
#define ECID_ECID0_0_WAFER_RANGE 24U
#define ECID_ECID0_0_LOT1_MASK 0x3U
#define ECID_ECID0_0_LOT1_RANGE 30U
#define ECID_ECID1_0_LOT1_MASK 0x3FFFFFFU
#define ECID_ECID1_0_LOT0_MASK 0x3FU
#define ECID_ECID1_0_LOT0_RANGE 26U
#define ECID_ECID2_0_LOT0_MASK 0x3FFFFFFU
#define ECID_ECID2_0_FAB_MASK 0x3FU
#define ECID_ECID2_0_FAB_RANGE 26U
#define ECID_ECID3_0_VENDOR_MASK 0xFU
#define FUSE_RESERVED_SW_MASK 0xFFU
#define FUSE_RESERVED_SW_SHIFT 4U
#define FUSE_RESERVED_BOOT_DEVICE_MASK 0x7U
#define FUSE_RESERVED_BOOT_DEVICE_SHIFT 0U
#define FUSE_RESERVED_IGNORE_STRAP_MASK 0x1U
#define FUSE_RESERVED_IGNORE_STRAP_SHIFT 3U
#define FUSE_RESERVED_APB2JTAG_LOCK_MASK 0x8U
#define FUSE_BOOT_SECURITY_INFO_SECURE_MASK 0x7U

uint32_t tegrabl_fuserdata_read(uint32_t addr)
{
	uint32_t val;
	bool original_visibility;
	uint32_t reg;

	/* set visibility to true */
	original_visibility = tegrabl_set_fuse_reg_visibility(true);

	/* prepare the data */
	NV_FUSE_WRITE(FUSE_FUSEADDR_0, addr);

	/* trigger the read */
	reg = NV_FUSE_READ(FUSE_FUSECTRL_0);
	reg = NV_FLD_SET_DRF_DEF(FUSE, FUSECTRL, FUSECTRL_CMD, READ, reg);
	NV_FUSE_WRITE(FUSE_FUSECTRL_0, reg);

	do {
		reg = NV_FUSE_READ(FUSE_FUSECTRL_0);
	} while (NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_STATE, reg) !=
			FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);

	/* read fuse */
	val = NV_FUSE_READ(FUSE_FUSERDATA_0);

	/* set original visibility */
	(void)tegrabl_set_fuse_reg_visibility(original_visibility);

	return val;
}

void tegrabl_fuse_program_mirroring(bool is_enable)
{
	uint32_t data;

	data = PMC_IMPL_READ(PMC_IMPL_FUSE_CONTROL_0);
	data = NV_FLD_SET_DRF_NUM(PMC_IMPL, FUSE_CONTROL, ENABLE_REDIRECTION,
		is_enable, data);
	PMC_IMPL_WRITE(PMC_IMPL_FUSE_CONTROL_0, data);
}

void tegrabl_pmc_fuse_control_ps18_latch_set(void)
{
	uint32_t data;

	data = PMC_IMPL_READ(PMC_IMPL_FUSE_CONTROL_0);
	data = NV_FLD_SET_DRF_NUM(PMC_IMPL, FUSE_CONTROL, PS18_LATCH_CLEAR,
		0, data);
	PMC_IMPL_WRITE(PMC_IMPL_FUSE_CONTROL_0, data);
	tegrabl_mdelay(1);

	data = NV_FLD_SET_DRF_NUM(PMC_IMPL, FUSE_CONTROL, PS18_LATCH_SET,
		1, data);
	PMC_IMPL_WRITE(PMC_IMPL_FUSE_CONTROL_0, data);
	tegrabl_mdelay(1);
}

void tegrabl_pmc_fuse_control_ps18_latch_clear(void)
{
	uint32_t data;

	data = PMC_IMPL_READ(PMC_IMPL_FUSE_CONTROL_0);
	data = NV_FLD_SET_DRF_NUM(PMC_IMPL, FUSE_CONTROL, PS18_LATCH_SET,
		0, data);
	PMC_IMPL_WRITE(PMC_IMPL_FUSE_CONTROL_0, data);
	tegrabl_mdelay(1);

	data = NV_FLD_SET_DRF_NUM(PMC_IMPL, FUSE_CONTROL, PS18_LATCH_CLEAR,
		1, data);
	PMC_IMPL_WRITE(PMC_IMPL_FUSE_CONTROL_0, data);
	tegrabl_mdelay(1);
}

bool tegrabl_fuse_ignore_dev_sel_straps(void)
{
	uint32_t val;

	val = NV_FUSE_READ(FUSE_RESERVED_SW_0);

	/* Get secondary boot device from straps if IGNORE_STRAP bit is not set */
	if (((val >> FUSE_RESERVED_IGNORE_STRAP_SHIFT) & FUSE_RESERVED_IGNORE_STRAP_MASK) == 0U) {
		return false;
	} else {
		return true;
	}
}

/**
 * @brief Read apb2jtag lock status
 *
 * @return TRUE in case if the bit is set
 *	FALSE if the bit is not set.
 */
static bool tegrabl_fuse_apb2jtag_lock_status(void)
{
	uint32_t val;

	val = NV_FUSE_READ(FUSE_SKU_DIRECT_CONFIG_0);

	if ((val & FUSE_RESERVED_APB2JTAG_LOCK_MASK) != 0U) {
		return true;
	} else {
		return false;
	}
}

/**
 * @brief Read boot device from fuses
 *
 * @param reg_data the secondary boot device that  needs to be returned
 *
 * @return TEGRABL_NO_ERROR in case if able to read from fuse
 *	ERR_INVALID in case it needs to be read from straps
 */
static tegrabl_error_t fuse_get_secondary_boot_device(uint32_t *reg_data)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t val;

	if (tegrabl_fuse_ignore_dev_sel_straps()) {
		val = NV_FUSE_READ(FUSE_RESERVED_SW_0);

		*reg_data = (val & FUSE_RESERVED_BOOT_DEVICE_MASK);
	} else {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}
	return err;
}

bool fuse_is_odm_production_mode(void)
{
	if ((NV_FUSE_READ(FUSE_SECURITY_MODE_0)) != 0U) {
		return true;
	} else {
		return false;
	}
}

uint32_t tegrabl_fuse_get_bootrom_patch_version(void)
{
	uint32_t val;
	bool original_visibility;

	original_visibility = tegrabl_set_fuse_reg_visibility(true);
	val =  NV_FUSE_READ(FUSE_BOOTROM_PATCH_VERSION_0) & 0xFFU;
	(void)tegrabl_set_fuse_reg_visibility(original_visibility);

	return val;
}

uint32_t tegrabl_fuse_get_security_info(void)
{
	uint32_t val;
	bool original_visibility;

	original_visibility = tegrabl_set_fuse_reg_visibility(true);
	val =  NV_FUSE_READ(FUSE_BOOT_SECURITY_INFO_0);
	(void)tegrabl_set_fuse_reg_visibility(original_visibility);

	return val;
}

bool fuse_is_nv_production_mode(void)
{
	if ((NV_FUSE_READ(FUSE_PRODUCTION_MODE_0)) != 0U) {
		return true;
	} else {
		return false;
	}
}

static uint32_t fuse_get_boot_security_info(void)
{
	uint32_t val;
	static uint32_t secure_mode[FUSE_BOOT_SECURITY_INFO_SECURE_MASK + 1] = {
		FUSE_BOOT_SECURITY_AESCMAC,
		FUSE_BOOT_SECURITY_AESCMAC,
		FUSE_BOOT_SECURITY_RSA,
		FUSE_BOOT_SECURITY_ECC,
		FUSE_BOOT_SECURITY_AESCMAC_ENCRYPTION,
		FUSE_BOOT_SECURITY_AESCMAC_ENCRYPTION,
		FUSE_BOOT_SECURITY_RSA_ENCRYPTION,
		FUSE_BOOT_SECURITY_ECC_ENCRYPTION
	};

	val = tegrabl_fuse_get_security_info();
	val &= FUSE_BOOT_SECURITY_INFO_SECURE_MASK;

	return secure_mode[val];
}

/**
 * @brief Reads the Uid of the chip
 *
 * @param serial_no Argument to hold the unique id.
 */
static void fuse_query_uid(uint32_t *serial_no)
{
	uint32_t vendor;
	uint32_t fab;
	uint32_t wafer;
	uint32_t lot0;
	uint32_t lot1;
	uint32_t x;
	uint32_t y;
	uint32_t rsvd1;
	uint32_t reg;
	bool original_visibility;

	original_visibility = tegrabl_set_fuse_reg_visibility(true);

	reg = NV_FUSE_READ(FUSE_OPT_VENDOR_CODE_0);
	vendor = reg & FUSE_OPT_VENDOR_CODE_0_READ_MASK;

	reg = NV_FUSE_READ(FUSE_OPT_FAB_CODE_0);
	fab = reg & FUSE_OPT_FAB_CODE_0_READ_MASK;

	lot0 = NV_FUSE_READ(FUSE_OPT_LOT_CODE_0_0);

	lot1 = 0;
	reg = NV_FUSE_READ(FUSE_OPT_LOT_CODE_1_0);
	lot1 = reg & FUSE_OPT_LOT_CODE_1_0_READ_MASK;

	reg = NV_FUSE_READ(FUSE_OPT_WAFER_ID_0);
	wafer = reg & FUSE_OPT_WAFER_ID_0_READ_MASK;

	reg = NV_FUSE_READ(FUSE_OPT_X_COORDINATE_0);
	x = reg & FUSE_OPT_X_COORDINATE_0_READ_MASK;

	reg = NV_FUSE_READ(FUSE_OPT_Y_COORDINATE_0);
	y = reg & FUSE_OPT_Y_COORDINATE_0_READ_MASK;

	reg = NV_FUSE_READ(FUSE_OPT_OPS_RESERVED_0);
	rsvd1 = reg & FUSE_OPT_OPS_RESERVED_0_READ_MASK;

	reg = 0;
	reg |= (rsvd1 & ECID_ECID0_0_RSVD1_MASK);
	reg |= (y & ECID_ECID0_0_Y_MASK) << ECID_ECID0_0_Y_RANGE;
	reg |= (x & ECID_ECID0_0_X_MASK) << ECID_ECID0_0_X_RANGE;
	reg |= (wafer & ECID_ECID0_0_WAFER_MASK) << ECID_ECID0_0_WAFER_RANGE;
	reg |= (lot1 & ECID_ECID0_0_LOT1_MASK) << ECID_ECID0_0_LOT1_RANGE;
	serial_no[0] = reg;

	lot1 >>= 2;

	reg = 0;
	reg |= lot1 & ECID_ECID1_0_LOT1_MASK;
	reg |= (lot0 & ECID_ECID1_0_LOT0_MASK) << ECID_ECID1_0_LOT0_RANGE;
	serial_no[1] = reg;

	lot0 >>= 6;

	reg = 0;
	reg |= lot0 & ECID_ECID2_0_LOT0_MASK;
	reg |= (fab & ECID_ECID2_0_FAB_MASK) << ECID_ECID2_0_FAB_RANGE;
	serial_no[2] = reg;

	reg = 0;
	reg |= vendor & ECID_ECID3_0_VENDOR_MASK;
	serial_no[3] = reg;

	(void)tegrabl_set_fuse_reg_visibility(original_visibility);
}

/**
 * @brief Read the floorsweeping registers and determine which core(s) are
 * enabled
 *
 * @param enabled_cores (output parameter) Bitmask representing all cores
 * enabled as per floorsweeping registers
 */
static void fuse_get_enabled_cpu_cores(uint32_t *enabled_cores)
{
	uint32_t reg;
	/* A set(1) bit represents an invalid/absent/disabled core */
	uint32_t disabled_core_mask = 0xffffffff;

	/* The following macros define the bits allocated for ARM and Denver
	 * cores in disabled_core_mask */
#define CPU_CORE_ABSENT_MASK_0_ARM_DISABLE_RANGE 7:4
#define CPU_CORE_ABSENT_MASK_0_DENVER_DISABLE_RANGE 1:0

	if (disabled_core_mask == 0xffffffff) {
		reg = NV_FUSE_READ(FUSE_OPT_ARM_CORE_DISABLE_0);
		reg = NV_DRF_VAL(FUSE, OPT_ARM_CORE_DISABLE,
						 OPT_ARM_CORE_DISABLE, reg);
		disabled_core_mask = NV_FLD_SET_DRF_NUM(CPU, CORE_ABSENT_MASK,
												ARM_DISABLE, reg,
												disabled_core_mask);

		reg = NV_FUSE_READ(FUSE_OPT_DENVER_CORE_DISABLE_0);
		reg = NV_DRF_VAL(FUSE, OPT_DENVER_CORE_DISABLE,
						 OPT_DENVER_CORE_DISABLE, reg);
		disabled_core_mask = NV_FLD_SET_DRF_NUM(CPU, CORE_ABSENT_MASK,
												DENVER_DISABLE, reg,
												disabled_core_mask);

		pr_info("disabled_core_mask: 0x%08x\n", disabled_core_mask);
	}

	*enabled_cores = ~disabled_core_mask;
}

/**
 * @brief Queries the max size for the given fuse
 *
 * @param type Type of the fuse whose size is to be queried.
 * @param size Argument to hold the size of the fuse.
 *
 * @return TEGRABL_NO_ERROR if operation is successful.
 */
tegrabl_error_t tegrabl_fuse_query_size(uint32_t type, uint32_t *size)
{
	bool flag = false;
	if (size == NULL) {
		pr_debug("Empty buffer given to query size\n");
		return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
	}

	switch (type) {
	case FUSE_TYPE_BOOT_SECURITY_INFO:
	case FUSE_SEC_BOOTDEV:
	case FUSE_TID:
	case FUSE_SKU_INFO:
	case FUSE_CPU_SPEEDO0:
	case FUSE_CPU_SPEEDO1:
	case FUSE_CPU_SPEEDO2:
	case FUSE_CPU_IDDQ:
	case FUSE_SOC_SPEEDO0:
	case FUSE_SOC_SPEEDO1:
	case FUSE_SOC_SPEEDO2:
	case FUSE_SOC_IDDQ:
	case FUSE_ENABLED_CPU_CORES:
	case FUSE_APB2JTAG_LOCK:
	case FUSE_SATA_NV_CALIB:
	case FUSE_SATA_MPHY_ODM_CALIB:
	case FUSE_TPC_DISABLE:
	case FUSE_TSENSOR9_CALIB:
	case FUSE_TSENSOR_COMMON_T1:
	case FUSE_TSENSOR_COMMON_T2:
	case FUSE_HYPERVOLTAGING:
	case FUSE_RESERVED_CALIB0:
	case FUSE_OPT_PRIV_SEC_EN:
	case FUSE_USB_CALIB:
	case FUSE_USB_CALIB_EXT:
	case FUSE_RESERVED_ODM0:
	case FUSE_RESERVED_ODM1:
	case FUSE_RESERVED_ODM2:
	case FUSE_RESERVED_ODM3:
	case FUSE_RESERVED_ODM4:
	case FUSE_RESERVED_ODM5:
	case FUSE_RESERVED_ODM6:
	case FUSE_RESERVED_ODM7:
	case FUSE_RESERVED_SW:
	case FUSE_BOOT_DEVICE_SELECT:
	case FUSE_SKIP_DEV_SEL_STRAPS:
	case FUSE_PRODUCTION_MODE:
	case FUSE_SECURITY_MODE:
	case FUSE_ODM_LOCK:
	case FUSE_ARM_JTAG_DIS:
	case FUSE_H2:
	case FUSE_ODM_INFO:
	case FUSE_DBG_AUTHENTICATN:
	case FUSE_CCPLEX_DFD_ACCESS_DISABLE:
	case FUSE_PRIVATE1:
	case FUSE_PRIVATE2:
		*size = sizeof(uint32_t);
		break;
	case FUSE_UID:
		*size = ECID_SIZE_BYTES;
		break;
	case FUSE_SECURE_BOOT_KEY:
	case FUSE_KEK2:
		*size = SBKKEY_SIZE_BYTES;
		break;
	case FUSE_KEK0:
		*size = KEK0KEY_SIZE_BYTES;
		break;
	case FUSE_KEK1:
		*size = KGKKEY_SIZE_BYTES;
		break;
	case FUSE_PKC_PUBKEY_HASH:
		*size = PUBKEY_SIZE_BYTES;
		break;
	case FUSE_KEK256:
		*size = KEKKEY_SIZE_BYTES;
		break;
	case FUSE_ENDORSEMENT_KEY:
		*size = EKKEY_SIZE_BYTES;
		break;
	case FUSE_ODMID:
		*size = ODMID_SIZE_BYTES;
		break;

	default:
		pr_error("Unknown fuse type size requested\n");
		*size = 0;
		flag = true;
		break;
	}
	if (flag) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
	}
	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t tegrabl_copy_fuse_bytes
	(uint32_t regaddress,
	uint8_t *pbyte,
	const uint32_t nbytes)
{
	uint32_t regdata;
	uint32_t i;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if ((nbytes == 0U) || (regaddress == 0U) || (pbyte == NULL)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		return err;
	}
	regdata = 0;
	for (i = 0; i < nbytes; i++) {
		if ((i & 3U) == 0U) {
			regdata = NV_FUSE_READ(regaddress);
			regaddress += 4U;
		}
		pbyte[i] = (uint8_t)(regdata & 0xFFUL);
		regdata >>= 8;
	}
	return err;
}

static tegrabl_error_t tegrabl_get_pubkey(uint8_t *pkey)
{
	return tegrabl_copy_fuse_bytes(FUSE_PUBLIC_KEY0_0,
			pkey, PUBKEY_SIZE_BYTES);
}

static tegrabl_error_t tegrabl_get_sbk(uint8_t *pkey)
{
	return tegrabl_copy_fuse_bytes(FUSE_PRIVATE_KEY0_0,
			pkey, SBKKEY_SIZE_BYTES);
}

static tegrabl_error_t tegrabl_get_kek(uint8_t *pkey)
{
	return tegrabl_copy_fuse_bytes(FUSE_KEK00_0,
			pkey, KEKKEY_SIZE_BYTES);
}

static tegrabl_error_t tegrabl_get_kek0(uint8_t *pkey)
{
	return tegrabl_copy_fuse_bytes(FUSE_KEK00_0,
			pkey, KEK0KEY_SIZE_BYTES);
}

static tegrabl_error_t tegrabl_get_kgk(uint8_t *pkey)
{
	return tegrabl_copy_fuse_bytes(FUSE_KEK10_0,
			pkey, KGKKEY_SIZE_BYTES);
}

static tegrabl_error_t tegrabl_get_kek2(uint8_t *pkey)
{
	return tegrabl_copy_fuse_bytes(FUSE_KEK20_0,
			pkey, KGKKEY_SIZE_BYTES);
}

static tegrabl_error_t tegrabl_get_ek(uint8_t *pkey)
{
	return tegrabl_copy_fuse_bytes(FUSE_EK0_0,
                              pkey, EKKEY_SIZE_BYTES);
}

static tegrabl_error_t tegrabl_get_odmid(uint8_t *odmid)
{
	return tegrabl_copy_fuse_bytes(FUSE_ODMID0_0,
			odmid, ODMID_SIZE_BYTES);
}

/**
 * @brief Reads the requested fuse into the input buffer.
 *
 * @param type Type of the fuse to be read.
 * @param buffer Buffer to hold the data read.
 * @param size Size(in bytes) of the fuse to be read.
 *
 * @return TEGRABL_NO_ERROR if operation is successful.
 */
tegrabl_error_t tegrabl_fuse_read(
	fuse_type_t type, uint32_t *buffer, uint32_t size)
{
	uint32_t temp_size = 0;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t reg_data = 0;
	(void)tegrabl_set_fuse_reg_visibility(true);
	if (buffer == NULL) {
		pr_debug("Null pointer passed to read the fuse\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if (size > 0U) {
		err = tegrabl_fuse_query_size(type, &temp_size);
		if (err != 0) {
			goto fail;
		}
		if (temp_size < size) {
			pr_debug("wrong size supplied in argument\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}
	} else {
		pr_debug("Size to be read cannot be zero\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	switch (type) {
	case FUSE_SEC_BOOTDEV:
		err = fuse_get_secondary_boot_device(&reg_data);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
		*buffer = reg_data;
		break;
	case FUSE_TYPE_BOOT_SECURITY_INFO:
		*buffer = fuse_get_boot_security_info();
		break;
	case FUSE_UID:
		/* Check if the UID size 16B */
		if (ECID_SIZE_BYTES != size) {
			err = TEGRABL_ERR_OUT_OF_RANGE;
			goto fail;
		}
		fuse_query_uid(buffer);
		break;
	case FUSE_PKC_PUBKEY_HASH:
		err = tegrabl_get_pubkey((uint8_t *)buffer);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
		break;
	case FUSE_SECURE_BOOT_KEY:
		err = tegrabl_get_sbk((uint8_t *)buffer);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
		break;
	case FUSE_KEK256:
		err = tegrabl_get_kek((uint8_t *)buffer);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
		break;
	case FUSE_KEK0:
		err = tegrabl_get_kek0((uint8_t *)buffer);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
		break;
	case FUSE_KEK1:
		err = tegrabl_get_kgk((uint8_t *)buffer);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
		break;
	case FUSE_KEK2:
		err = tegrabl_get_kek2((uint8_t *)buffer);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
		break;
	case FUSE_SKU_INFO:
		reg_data = NV_FUSE_READ(FUSE_SKU_INFO_0);
		*buffer = reg_data & (uint32_t)FUSE_SKU_INFO_0_READ_MASK;
		break;
	case FUSE_CPU_SPEEDO0:
		*buffer = NV_FUSE_READ(FUSE_CPU_SPEEDO_0_CALIB_0);
		break;
	case FUSE_CPU_SPEEDO1:
		*buffer = NV_FUSE_READ(FUSE_CPU_SPEEDO_1_CALIB_0);
		break;
	case FUSE_CPU_SPEEDO2:
		*buffer = NV_FUSE_READ(FUSE_CPU_SPEEDO_2_CALIB_0);
		break;
	case FUSE_CPU_IDDQ:
		*buffer = NV_FUSE_READ(FUSE_MCLUSTER_WITH_RAM_HT_IDDQ_CALIB_0);
		break;
	case FUSE_SOC_SPEEDO0:
		*buffer = NV_FUSE_READ(FUSE_SOC_SPEEDO_0_CALIB_0);
		break;
	case FUSE_SOC_SPEEDO1:
		*buffer = NV_FUSE_READ(FUSE_SOC_SPEEDO_1_CALIB_0);
		break;
	case FUSE_SOC_SPEEDO2:
		*buffer = NV_FUSE_READ(FUSE_SOC_SPEEDO_2_CALIB_0);
		break;
	case FUSE_SOC_IDDQ:
		*buffer = NV_FUSE_READ(FUSE_SOC_HT_IDDQ_CALIB_0);
		break;
	case FUSE_TID:
		*buffer = NV_FUSE_READ(FUSE_RESERVED_ODM1_0);
		break;
	case FUSE_ENABLED_CPU_CORES:
		fuse_get_enabled_cpu_cores(buffer);
		break;
	case FUSE_TPC_DISABLE:
		*buffer = NV_FUSE_READ(FUSE_OPT_TPC_DISABLE_0);
		break;
	case FUSE_APB2JTAG_LOCK:
		*buffer = (uint32_t)tegrabl_fuse_apb2jtag_lock_status();
		break;
	case FUSE_SATA_NV_CALIB:
		*buffer = NV_FUSE_READ(FUSE_SATA_NV_CALIB_0);
		*buffer = NV_DRF_VAL(FUSE, SATA_NV_CALIB, SATA_NV_CALIB, *buffer);
		break;
	case FUSE_SATA_MPHY_ODM_CALIB:
		*buffer = NV_FUSE_READ(FUSE_SATA_MPHY_ODM_CALIB_0);
		*buffer = NV_DRF_VAL(FUSE, SATA_MPHY_ODM_CALIB,
				SATA_MPHY_ODM_CALIB, *buffer);
		break;
	case FUSE_TSENSOR9_CALIB:
		*buffer = NV_FUSE_READ(FUSE_TSENSOR9_CALIB_0);
		break;
	case FUSE_TSENSOR_COMMON_T1:
		*buffer = NV_FUSE_READ(FUSE_TSENSOR_COMMON_T1_0);
		break;
	case FUSE_TSENSOR_COMMON_T2:
		*buffer = NV_FUSE_READ(FUSE_TSENSOR_COMMON_T2_0);
		break;
	case FUSE_HYPERVOLTAGING:
		*buffer = NV_FUSE_READ(FUSE_HYPERVOLTAGING_0);
		break;
	case FUSE_RESERVED_CALIB0:
		*buffer = NV_FUSE_READ(FUSE_RESERVED_CALIB0_0);
		break;
	case FUSE_OPT_PRIV_SEC_EN:
		*buffer = NV_FUSE_READ(FUSE_OPT_PRIV_SEC_EN_0);
		break;
	case FUSE_USB_CALIB:
		*buffer = NV_FUSE_READ(FUSE_USB_CALIB_0);
		break;
	case FUSE_USB_CALIB_EXT:
		*buffer = NV_FUSE_READ(FUSE_USB_CALIB_EXT_0);
		break;
	case FUSE_RESERVED_ODM0:
		*buffer = NV_FUSE_READ(FUSE_RESERVED_ODM0_0);
		break;
	case FUSE_RESERVED_ODM1:
		*buffer = NV_FUSE_READ(FUSE_RESERVED_ODM1_0);
		break;
	case FUSE_RESERVED_ODM2:
		*buffer = NV_FUSE_READ(FUSE_RESERVED_ODM2_0);
		break;
	case FUSE_RESERVED_ODM3:
		*buffer = NV_FUSE_READ(FUSE_RESERVED_ODM3_0);
		break;
	case FUSE_RESERVED_ODM4:
		*buffer = NV_FUSE_READ(FUSE_RESERVED_ODM4_0);
		break;
	case FUSE_RESERVED_ODM5:
		*buffer = NV_FUSE_READ(FUSE_RESERVED_ODM5_0);
		break;
	case FUSE_RESERVED_ODM6:
		*buffer = NV_FUSE_READ(FUSE_RESERVED_ODM6_0);
		break;
	case FUSE_RESERVED_ODM7:
		*buffer = NV_FUSE_READ(FUSE_RESERVED_ODM7_0);
		break;
	case FUSE_RESERVED_SW:
		*buffer = (NV_FUSE_READ(FUSE_RESERVED_SW_0) >>
				FUSE_RESERVED_SW_SHIFT) & FUSE_RESERVED_SW_MASK;
		break;
	case FUSE_BOOT_DEVICE_SELECT:
		*buffer = (NV_FUSE_READ(FUSE_RESERVED_SW_0) >>
				FUSE_RESERVED_BOOT_DEVICE_SHIFT) &
				FUSE_RESERVED_BOOT_DEVICE_MASK;
		break;
	case FUSE_SKIP_DEV_SEL_STRAPS:
		*buffer = (NV_FUSE_READ(FUSE_RESERVED_SW_0) >>
				FUSE_RESERVED_IGNORE_STRAP_SHIFT) &
				FUSE_RESERVED_IGNORE_STRAP_MASK;
		break;
	case FUSE_ENDORSEMENT_KEY:
		err = tegrabl_get_ek((uint8_t *)buffer);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
		break;
	case FUSE_ODMID:
		err = tegrabl_get_odmid((uint8_t *)buffer);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
		break;
	case FUSE_PRODUCTION_MODE:
		*buffer = NV_FUSE_READ(FUSE_PRODUCTION_MODE_0);
		break;
	case FUSE_SECURITY_MODE:
		*buffer = NV_FUSE_READ(FUSE_SECURITY_MODE_0);
		break;
	case FUSE_ODM_LOCK:
		*buffer = NV_FUSE_READ(FUSE_ODM_LOCK_0);
		break;
	case FUSE_ARM_JTAG_DIS:
		*buffer = NV_FUSE_READ(FUSE_ARM_JTAG_DIS_0);
		break;
	case FUSE_H2:
		*buffer = NV_FUSE_READ(FUSE_H2_0);
		break;
	case FUSE_ODM_INFO:
		*buffer = NV_FUSE_READ(FUSE_ODM_INFO_0);
		break;
	case FUSE_DBG_AUTHENTICATN:
		*buffer = NV_FUSE_READ(FUSE_DEBUG_AUTHENTICATION_0);
		break;
	case FUSE_CCPLEX_DFD_ACCESS_DISABLE:
		*buffer = NV_FUSE_READ(FUSE_DENVER_DFD_ACCESS_DISABLE_0);
		break;
	default:
		pr_debug("Unkown fuse type read requested\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		break;
	}
fail:
	if (err != 0U) {
		pr_error("Error = %d in tegrabl_fuse_read\n", err);
	}
	return err;
}
