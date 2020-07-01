/*
 * Copyright (c) 2015-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_SDRAM_USAGE_H
#define INCLUDED_TEGRABL_SDRAM_USAGE_H

/*                     ________________________________________
 *
 *                       Memory map of SDRAM usage by nvtboot
 *                     ________________________________________
 *
 * [Address]                                                       [Size]
 * Carveouts start from MIN(0xFFFF_FFFF, DRAM_END) and grow downwards
 *             ____________________________________________________
 *      ^      |                                                  |
 *      |      |                   MTS Carveout                   |  <- 128 MB
 *      |      |__________________________________________________|
 *     This    |                                                  |
 *   portion   |                  GSC Carveouts                   |  <- 128 MB
 *    is not   |__________________________________________________|
 *  visible to |                                                  |
 *      OS     |                 SecureOS carveout                |  <- 6 MB
 *      |      |__________________________________________________|
 *      |      |                                                  |
 *      |      |                 BPMP-FW carveout                 |  <- 1 MB
 *      V      |__________________________________________________|
 *             |                                                  |
 *             |               .............                      |
 *             |                                                  |
 *             |__________________________________________________|
 *             |                                                  |
 *             |                                                  | <- 320 MB
 *             |                   Boot.img                       |
 * 0xA8000000  |__________________________________________________|
 *             |                                                  |
 *             |                                                  |
 *             |                   Ramdisk                        | <- 224 MB
 * 0x9C000000->|__________________________________________________|
 *             |                                                  |
 *             |                   NvTboot-CPU                    |  <- 64 MB
 * 0x96000000->|__________________________________________________|
 *             |                                                  |
 *             |                   NCT partition                  |  <- 2  MB
 * 0x95E00000->|__________________________________________________|
 *             |                                                  |
 *             |           Backdoor-loading Scratch Space         |  <- 62 MB
 * 0x92000000->|__________________________________________________|
 *             |                                                  |
 *             |                   NvTboot-BPMP                   |  <- 32 MB
 * 0x90000000->|__________________________________________________|
 *             |                                                  |
 *             |                     Temp buffers                 |
 * 0x8F000000->|__________________________________________________|
 *             |                                                  |
 *             |                       Heap                       |  <- 96 MB
 * 0x89000000->|__________________________________________________|
 *             |                                                  |
 *             |                     Quickboot                    |  <- ~65 MB
 * 0x84008000->|__________________________________________________|
 *             |                                                  |
 *             |                      .......                     |
 * 0x82000000->|__________________________________________________|
 *             |                                                  |
 *             |                       Kernel                     |  <- 31.5 MB
 * 0x80080000->|__________________________________________________|
 *             |                                                  |
 *             |                      .......                     |  <- 512 KB
 * Bottom    ->|__________________________________________________|
 * of Mem
 * (0x80000000)
 */

#define MTS_CARVEOUT_SIZE		0x08000000
#define MTS_CARVEOUT_ADDR		0xB8000000
#define EKS_CARVEOUT_SIZE		0x00100000
#define EKS_CARVEOUT_ADDR		0xB7E00000
#define TOS_CARVEOUT_SIZE		0x00400000
#define TOS_CARVEOUT_ADDR		0xB7A00000
#define GSC_DRAM_SIZE			0x01700000
#define GSC_DRAM_BASE			0xB6300000
#define VPR_CARVEOUT_SIZE		0x08000000
#define VPR_CARVEOUT_ADDR		0xAE300000
#define LAST_CARVEOUT			0xAE300000

#define TLK_PARAMS_BUFFER		0x8F000000

#define MAX_KERNEL_IMAGE_SIZE	0x14000000
#define RAMDISK_ADDRESS			0x9C000000
#define RAMDISK_MAX_SIZE		0x0C000000
#define BOOT_IMAGE_MAX_SIZE		0x04000000
#define BOOT_IMAGE_LOAD_ADDRESS	0xA8000000
#define NCT_PART_SIZE			0x00200000
#define NCT_PART_LOAD_ADDRESS	0x95E00000
#define WB0_IMAGE_LOAD_ADDRESS	0x94500000
#define MTS_IMAGE_LOAD_ADDRESS	0x92500000
#define EKS_IMAGE_LOAD_ADDRESS	0x92400000
#define TOS_IMAGE_LOAD_ADDRESS	0x92100000
#define DTB_LOAD_ADDRESS		0x92000000
#define DTB_MAX_SIZE			0x00100000
#define LINUX_LOAD_ADDRESS		0x80080000
#define TBOOT_ENTRY_ADDRESS		0x90000000

#define MAX_EKS_SIZE			(1U * 1024U * 1024U)

/* bootloader-dtb */
#define BL_DTB_SIZE			(1024U * 1024U)
#define BL_DTB_ALIGNMENT		(512U)

#endif
