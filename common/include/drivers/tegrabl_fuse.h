/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_FUSE_H
#define TEGRABL_FUSE_H

#include <stdint.h>
#include <tegrabl_error.h>
#include <stdbool.h>

#define MAGICID_FUSE 0x46555345 /* "fuse" */
#define MAGICID_ENDIAN_FUSE 0x45535546 /* "esuf" */
#define MAGICID_FSKP 0x46534B50 /* "FSKP" */
#define FSKP_STRUCT_OFFSET 0x40020004
#define FUSEDATA_MAXSIZE 8 /* 256 bits, max data size for a single fuse */

/*
 * @brief List of security modes
 */
enum {
	FUSE_BOOT_SECURITY_AESCMAC,
	FUSE_BOOT_SECURITY_RSA,
	FUSE_BOOT_SECURITY_ECC,
	FUSE_BOOT_SECURITY_AESCMAC_ENCRYPTION,
	FUSE_BOOT_SECURITY_RSA_ENCRYPTION,
	FUSE_BOOT_SECURITY_ECC_ENCRYPTION,
	FUSE_BOOT_SECURITY_AES_ENCRYPTION,
	FUSE_BOOT_SECURITY_MAX,
};

/*
 * @brief Type of the fuses whose read/write is supported
 */
enum fuse_type {
	FUSE_BOOT_SECURITY_INFO = 0x0,
	FUSE_BOOT_SECURITY_REDUNDANT_INFO = FUSE_BOOT_SECURITY_INFO,
	FUSE_SEC_BOOTDEV,
	FUSE_UID,
	FUSE_SKU_INFO,
	FUSE_TID,
	FUSE_CPU_SPEEDO0,
	FUSE_CPU_SPEEDO1,
	FUSE_CPU_SPEEDO2,
	FUSE_CPU_IDDQ,
	FUSE_SOC_SPEEDO0,
	FUSE_SOC_SPEEDO1,
	FUSE_SOC_SPEEDO2,
	FUSE_ENABLED_CPU_CORES,
	FUSE_TPC_DISABLE,
	FUSE_APB2JTAG_LOCK,
	FUSE_SOC_IDDQ,
	FUSE_SATA_NV_CALIB,
	FUSE_SATA_MPHY_ODM_CALIB,
	FUSE_TSENSOR9_CALIB,
	FUSE_TSENSOR_COMMON_T1,
	FUSE_TSENSOR_COMMON_T2,
	FUSE_TSENSOR_COMMON_T3,
	FUSE_HYPERVOLTAGING,
	FUSE_RESERVED_CALIB0,
	FUSE_OPT_PRIV_SEC_EN,
	FUSE_USB_CALIB,
	FUSE_USB_CALIB_EXT,
	FUSE_PRODUCTION_MODE,
	FUSE_SECURITY_MODE,
	FUSE_SECURITY_MODE_REDUNDANT = FUSE_SECURITY_MODE,
	FUSE_ODM_LOCK,
	FUSE_ODM_LOCK_R = FUSE_ODM_LOCK,
	FUSE_ARM_JTAG_DIS,
	FUSE_ARM_JTAG_DIS_REDUNDANT = FUSE_ARM_JTAG_DIS,
	FUSE_RESERVED_ODM0,
	FUSE_RESERVED_ODM0_REDUNDANT = FUSE_RESERVED_ODM0,
	FUSE_RESERVED_ODM1,
	FUSE_RESERVED_ODM1_REDUNDANT = FUSE_RESERVED_ODM1,
	FUSE_RESERVED_ODM2,
	FUSE_RESERVED_ODM2_REDUNDANT = FUSE_RESERVED_ODM2,
	FUSE_RESERVED_ODM3,
	FUSE_RESERVED_ODM3_REDUNDANT = FUSE_RESERVED_ODM3,
	FUSE_RESERVED_ODM4,
	FUSE_RESERVED_ODM4_REDUNDANT = FUSE_RESERVED_ODM4,
	FUSE_RESERVED_ODM5,
	FUSE_RESERVED_ODM5_REDUNDANT = FUSE_RESERVED_ODM5,
	FUSE_RESERVED_ODM6,
	FUSE_RESERVED_ODM6_REDUNDANT = FUSE_RESERVED_ODM6,
	FUSE_RESERVED_ODM7,
	FUSE_RESERVED_ODM7_REDUNDANT = FUSE_RESERVED_ODM7,
	FUSE_KEK256,
	FUSE_KEK2,
	FUSE_PKC_PUBKEY_HASH,
	FUSE_SECURE_BOOT_KEY,
	FUSE_RESERVED_SW,
	FUSE_BOOT_DEVICE_SELECT,
	FUSE_SKIP_DEV_SEL_STRAPS,
	FUSE_BOOT_DEVICE_INFO,
	FUSE_KEK0,
	FUSE_KEK1,
	FUSE_ENDORSEMENT_KEY,
	FUSE_ODMID,
	FUSE_H2,
	FUSE_ODM_INFO,
	FUSE_DEBUG_AUTHENTICATION,
	FUSE_DEBUG_AUTHENTICATION_REDUNDANT = FUSE_DEBUG_AUTHENTICATION,
	FUSE_CCPLEX_DFD_ACCESS_DISABLE,
	FUSE_CCPLEX_DFD_ACCESS_DISABLE_REDUNDANT = FUSE_CCPLEX_DFD_ACCESS_DISABLE,
	FUSE_DENVER_NV_MTS_RATCHET,
	FUSE_FORCE32 = 0x7FFFFFFF,

};

/*
 * @brief Type of the boot devices
 */
enum tegrabl_fuse_boot_dev {
	TEGRABL_FUSE_BOOT_DEV_SDMMC,
	TEGRABL_FUSE_BOOT_DEV_SPIFLASH,
	TEGRABL_FUSE_BOOT_DEV_SATA,
	TEGRABL_FUSE_BOOT_DEV_RESVD_4,
	TEGRABL_FUSE_BOOT_DEV_FOOS = TEGRABL_FUSE_BOOT_DEV_RESVD_4,
	TEGRABL_FUSE_BOOT_DEV_USB3,
	TEGRABL_FUSE_BOOT_DEV_UFS,
	TEGRABL_FUSE_BOOT_DEV_PRODUART,
	TEGRABL_FUSE_BOOT_DEV_MAX, /* Must appear after the last legal item */
	TEGRABL_FUSE_BOOT_DEV_FORCE32 = 0x7FFFFFFF,
};

/**
 * @brief fskp structure in fskp binary
 *
 * @param magic_id	Identifier for the fskp
 * @param offset	Offset of fuse info in fskp binary
 * @param size	size of fuseinfo struct
 */
struct tegrabl_fskp_info {
	uint32_t magic_id;
	uint32_t offset;
	uint32_t size;
};

/**
 * @brief Entry in the fuse info received from host
 *
 * @param type	Identifier for the fuse
 * @param size	Register size of the fuse
 * @param offset	Offset of fuse data in the fuse info
 */
struct fuse_node {
	enum fuse_type type;
	uint32_t size;
	uint32_t offset;
};

/**
 * @brief Header for fuse info
 *
 * @param magicid	magicid for validation
 * @param version	fuse tool version
 * @param infoSize	total size for fuse info
 * @param fuseNum	numbers of fuses in fuse info
 * @param fuseEntry	offset of the first fuse node in the info
*/
struct fuse_info_header {
	uint32_t magicid;
	uint32_t version;
	uint32_t infosize;
	uint32_t fusenum;
	uint32_t fuseentry;
};

/**
 * @brief The Fuse Info (contains fuse info header and one or more fuse nodes)
 *
 * @param head	pointer to header field in fuse info
 * @param nodes	pointer to fuse node field in fuse info
 * @param data	pointer to data field in fuse info
 */
struct fuse_info {
	struct fuse_info_header *head;
	struct fuse_node *nodes;
	uint32_t *data;
};

/**
 * @brief queries whether strap settings for secondary boot device
 *	can be ignored or not
 *
 * @return true if strap settings can be ignored
 *	false if sec.boot device has to be read from straps
 */
bool tegrabl_fuse_ignore_dev_sel_straps(void);

/**
 * @brief Queries the max size for the given fuse
 *
 * @param type Type of the fuse whose size is to be queried.
 * @param size Argument to hold the size of the fuse.
 *
 * @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_fuse_query_size(uint8_t type, uint32_t *size);

/**
 * @brief Reads the requested fuse into the input buffer.
 *
 * @param type Type of the fuse to be read.
 * @param buffer Buffer to hold the data read.
 * @param size Size of the fuse to be read.
 *
 * @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_fuse_read(
	uint8_t type, uint32_t *buffer, uint32_t size);

/**
 * @brief Sets fuse value to new value
 *
 * @param reg_addr Offset of fuse
 * @param reg_value New value
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_fuse_bypass_set(uint32_t reg_addr, uint32_t reg_value);

/**
 * @brief Retrieves the value of fuse at given offset.
 *
 * @param reg_addr Offset of fuse
 * @param reg_value Will be updated with fuse value
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_fuse_bypass_get(uint32_t reg_addr, uint32_t *reg_value);

/**
 * @brief Determines if the security_mode fuse is burned or not.
 *
 * @return true if security_mode fuse is burned.
 */
bool fuse_is_odm_production_mode(void);

/*
 * @brief read SECURITY_INFO fuse
 *
 * @return value of SECURITY_INFO fuse
 */
uint32_t tegrabl_fuse_get_security_info(void);

/*
 * @brief enable/disable fuse mirroring
 *
 * @param is_enable true/false to enable/disable fuse mirroring
 */
void tegrabl_fuse_program_mirroring(bool is_enable);

/**
 * @brief Burns the desired fuse
 *
 * @param fuse_type type of hte fuse to be burnt
 * @param buffer data with which the fuse is to be burnt
 * @param size size (in bytes) of the fuse to be burnt
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_fuse_write(
	uint32_t fuse_type, uint32_t *buffer, uint32_t size);

/**
 * @brief set ps18_latch_set bit in pmc_fuse_control register
 *
 */
void tegrabl_pmc_fuse_control_ps18_latch_set(void);

/**
 * @brief set ps18_latch_clear bit in pmc_fuse_control register
 *
 */
void tegrabl_pmc_fuse_control_ps18_latch_clear(void);
/*
 * @brief reads bootrom patch version
 *
 * @return bootrom patch version
 */
uint32_t tegrabl_fuse_get_bootrom_patch_version(void);

/*
 * @brief reads ft revision for given address
 *
 * @return ft revision value
 */
uint32_t tegrabl_fuserdata_read(uint32_t addr);

/**
 * @brief Determines if the production_mode fuse is burned or not.
 *
 * @return true if production_mode fuse is burned.
 */
bool fuse_is_nv_production_mode(void);

/**
 * @brief burn fuses as per fuse information
 *
 * @return TEGRABL_NO_ERROR on successful burning
 */
tegrabl_error_t burn_fuses(uint8_t *buffer, uint32_t bufsize);

/**
 * @brief get a particular fuse value from fuse blob
 *
 * @return TEGRABL_NO_ERROR on successful burning
 */
tegrabl_error_t get_fuse_value(uint8_t *buffer, uint32_t bufsize,
	enum fuse_type type, uint32_t *fuse_value);
#endif
