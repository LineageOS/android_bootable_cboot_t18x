/*
 * Copyright (c) 2015-2019, NVIDIA CORPORATION.  All rights reserved.
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

/* List of security modes */

#define FUSE_BOOT_SECURITY_AESCMAC               0U
#define FUSE_BOOT_SECURITY_RSA                   1U
#define FUSE_BOOT_SECURITY_ECC                   2U
#define FUSE_BOOT_SECURITY_AESCMAC_ENCRYPTION    3U
#define FUSE_BOOT_SECURITY_RSA_ENCRYPTION        4U
#define FUSE_BOOT_SECURITY_ECC_ENCRYPTION        5U
#define FUSE_BOOT_SECURITY_AES_ENCRYPTION        6U
#define FUSE_BOOT_SECURITY_MAX                   7U

/*
 * @brief Type of the fuses whose read/write is supported
 */
typedef uint32_t fuse_type_t;
#define FUSE_TYPE_BOOT_SECURITY_INFO 0x0U
#define FUSE_BOOT_SECURITY_REDUNDANT_INFO FUSE_TYPE_BOOT_SECURITY_INFO
#define FUSE_SEC_BOOTDEV 0x1U
#define FUSE_UID 0x2U
#define FUSE_SKU_INFO 0x3U
#define FUSE_TID 0x4U
#define FUSE_CPU_SPEEDO0 0x5U
#define FUSE_CPU_SPEEDO1 0x6U
#define FUSE_CPU_SPEEDO2 0x7U
#define FUSE_CPU_IDDQ 0x8U
#define FUSE_SOC_SPEEDO0 0x9U
#define FUSE_SOC_SPEEDO1 0xaU
#define FUSE_SOC_SPEEDO2 0xbU
#define FUSE_ENABLED_CPU_CORES 0xcU
#define FUSE_APB2JTAG_DISABLE 0xdU
#define FUSE_PRIVATE1 FUSE_APB2JTAG_DISABLE
#define FUSE_TPC_DISABLE 0xeU
#define FUSE_APB2JTAG_LOCK 0xfU
#define FUSE_SOC_IDDQ 0x10U
#define FUSE_SATA_NV_CALIB 0x11U
#define FUSE_SATA_MPHY_ODM_CALIB 0x12U
#define FUSE_TSENSOR9_CALIB 0x13U
#define FUSE_TSENSOR_COMMON_T1 0x14U
#define FUSE_TSENSOR_COMMON_T2 0x15U
#define FUSE_TSENSOR_COMMON_T3 0x16U
#define FUSE_HYPERVOLTAGING 0x17U
#define FUSE_RESERVED_CALIB0 0x18U
#define FUSE_OPT_PRIV_SEC_EN 0x19U
#define FUSE_USB_CALIB 0x1aU
#define FUSE_USB_CALIB_EXT 0x1bU
#define FUSE_PRODUCTION_MODE 0x1cU
#define FUSE_SECURITY_MODE 0x1dU
#define FUSE_SECURITY_MODE_REDUNDANT FUSE_SECURITY_MODE
#define FUSE_ODM_LOCK 0x1eU
#define FUSE_ODM_LOCK_R FUSE_ODM_LOCK
#define FUSE_ARM_JTAG_DIS 0x1fU
#define FUSE_ARM_JTAG_DIS_REDUNDANT FUSE_ARM_JTAG_DIS
#define FUSE_RESERVED_ODM0 0x20U
#define FUSE_RESERVED_ODM0_REDUNDANT FUSE_RESERVED_ODM0
#define FUSE_RESERVED_ODM1 0x21U
#define FUSE_RESERVED_ODM1_REDUNDANT FUSE_RESERVED_ODM1
#define FUSE_RESERVED_ODM2 0x22U
#define FUSE_RESERVED_ODM2_REDUNDANT FUSE_RESERVED_ODM2
#define FUSE_RESERVED_ODM3 0x23U
#define FUSE_RESERVED_ODM3_REDUNDANT FUSE_RESERVED_ODM3
#define FUSE_RESERVED_ODM4 0x24U
#define FUSE_RESERVED_ODM4_REDUNDANT FUSE_RESERVED_ODM4
#define FUSE_RESERVED_ODM5 0x25U
#define FUSE_RESERVED_ODM5_REDUNDANT FUSE_RESERVED_ODM5
#define FUSE_RESERVED_ODM6 0x26U
#define FUSE_RESERVED_ODM6_REDUNDANT FUSE_RESERVED_ODM6
#define FUSE_RESERVED_ODM7 0x27U
#define FUSE_RESERVED_ODM7_REDUNDANT FUSE_RESERVED_ODM7
#define FUSE_KEK256 0x28U
#define FUSE_KEK2 0x29U
#define FUSE_PKC_PUBKEY_HASH 0x2aU
#define FUSE_SECURE_BOOT_KEY 0x2bU
#define FUSE_RESERVED_SW 0x2cU
#define FUSE_BOOT_DEVICE_SELECT 0x2dU
#define FUSE_SKIP_DEV_SEL_STRAPS 0x2eU
#define FUSE_BOOT_DEVICE_INFO 0x2fU
#define FUSE_SECURE_PROVISION_INFO 0x30U
#define FUSE_PRIVATE2 FUSE_SECURE_PROVISION_INFO
#define FUSE_KEK0 0x31U
#define FUSE_KEK1 0x32U
#define FUSE_ENDORSEMENT_KEY 0x33U
#define FUSE_ODMID 0x34U
#define FUSE_H2 0x35U
#define FUSE_ODM_INFO 0x36U
#define FUSE_DBG_AUTHENTICATN 0x37U
#define FUSE_DBG_AUTHENTICATN_REDUNDANT FUSE_DBG_AUTHENTICATN
#define FUSE_CCPLEX_DFD_ACCESS_DISABLE 0x38U
#define FUSE_CCPLEX_DFD_ACCESS_DISABLE_REDUNDANT FUSE_CCPLEX_DFD_ACCESS_DISABLE
#define FUSE_FORCE32 0x7FFFFFFFU

/*
 * @brief Type of the boot devices
 */
/* macro tegrabl fuse boot dev */
typedef uint32_t tegrabl_fuse_boot_dev_t;
#define TEGRABL_FUSE_BOOT_DEV_SDMMC 0
#define TEGRABL_FUSE_BOOT_DEV_SPIFLASH 1
#define TEGRABL_FUSE_BOOT_DEV_SATA 2
#define TEGRABL_FUSE_BOOT_DEV_RESVD_4 3
#define TEGRABL_FUSE_BOOT_DEV_FOOS TEGRABL_FUSE_BOOT_DEV_RESVD_4
#define TEGRABL_FUSE_BOOT_DEV_USB3 4
#define TEGRABL_FUSE_BOOT_DEV_UFS 5
#define TEGRABL_FUSE_BOOT_DEV_PRODUART 6
#define TEGRABL_FUSE_BOOT_DEV_MAX 7 /* Must appear after the last legal item */
#define TEGRABL_FUSE_BOOT_DEV_FORCE32 0x7FFFFFFF

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
	fuse_type_t type;
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
tegrabl_error_t tegrabl_fuse_query_size(uint32_t type, uint32_t *size);

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
	fuse_type_t type, uint32_t *buffer, uint32_t size);

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
	fuse_type_t type, uint32_t *fuse_value);
#endif
