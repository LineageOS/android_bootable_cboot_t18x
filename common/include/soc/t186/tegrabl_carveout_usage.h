/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_CARVEOUT_USAGE_H
#define INCLUDED_TEGRABL_CARVEOUT_USAGE_H

/*                     ________________________________________
 *
 *                   Memory map of CARVEOUT_MB2_HEAP usage by Mb1/Mb2
 *                     ________________________________________
 *
 * [Offset]                                                       [Size]
 *
 *          0x0 __________________________________________________
 *             |                                                  |
 *             |                  TOS Params                      |  <- 64 KB
 *      0x10000|__________________________________________________|
 *             |                                                  |
 *             |                  Reserved1                       |  <- 960 KB
 *     0x100000|__________________________________________________|
 *             |                                                  |
 *             |             Temporary TSEC carveout              |  <- 1 MB
 *     0x200000|__________________________________________________|
 *             |                                                  |
 *             |                                                  |
 *             |                                                  |
 *             |                    Mb2 HEAP                      |
 *             |                                                  |  <- 9 MB
 *             |                                                  |
 *             |                                                  |
 *             |                                                  |
 *     0xB00000|__________________________________________________|
 */

#define TEGRABL_CARVEOUT_PAGE_SIZE                 (64U * 1024U)
#define TEGRABL_CARVEOUT_RAMOOPS_SIZE              (2U * 1024U * 1024U)
#define TEGRABL_SYSCFG_SIZE                        (2U * 1024U * 1024U)
#define TEGRABL_CARVEOUT_GAMEDATA_SIZE             (1U * 1024U * 1024U)

#define TEGRABL_TOS_PARAMS_OFFSET					0U
#define TEGRABL_CARVEOUT_MB2_HEAP_RSVD1_OFFSET		(TEGRABL_TOS_PARAMS_OFFSET + TEGRABL_CARVEOUT_PAGE_SIZE)
#define TEGRABL_CARVEOUT_MB2_HEAP_RSVD1_SIZE		(15 * TEGRABL_CARVEOUT_PAGE_SIZE)
#define TEGRABL_TSEC_TEMP_CARVEOUT_OFFSET			(TEGRABL_CARVEOUT_MB2_HEAP_RSVD1_OFFSET + TEGRABL_CARVEOUT_MB2_HEAP_RSVD1_SIZE)
#define TEGRABL_TSEC_TEMP_CARVEOUT_SIZE				0x00100000
#define TEGRABL_MB2_HEAP_OFFSET						(TEGRABL_TSEC_TEMP_CARVEOUT_OFFSET + TEGRABL_TSEC_TEMP_CARVEOUT_SIZE)
#define TEGRABL_MB2_HEAP_SIZE						0x01000000
#define TEGRABL_CARVEOUT_MB2_HEAP_SIZE				(TEGRABL_MB2_HEAP_OFFSET + TEGRABL_MB2_HEAP_SIZE)

/*                     ________________________________________
 *
 *                   Memory map of CARVEOUT_CPUBL_PARAMS usage by Mb1/Mb2/CPUBL
 *                     ________________________________________
 *
 * [Offset]                                                       [Size]
 *
 *        0x0  ____________________________________________________
 *             |                                                  |
 *             |               Mb2 / CPU BL params                |  <- 64 KB
 *     0x10000 |__________________________________________________|
 *             |                                                  |
 *             |                   Reserved1                      |  <- 64 KB
 *     0x20000 |__________________________________________________|
 *             |                                                  |
 *             |                    BR BCT                        |  <- 64 KB
 *     0x30000 |__________________________________________________|
 *             |                                                  |
 *             |                   Reserved2                      |  <- 64 KB
 *     0x40000 |__________________________________________________|
 *             |                                                  |
 *             |                   Profiling                      |  <- 64 KB
 *     0x50000 |__________________________________________________|
 *             |                                                  |
 *             |                   Reserved3                      |  <- 64 KB
 *     0x60000 |__________________________________________________|
 *             |                                                  |
 *             |                  GR carveout                     |  <- 64 KB
 *     0x70000 |__________________________________________________|
 *             |                                                  |
 *             |                   Reserved4                      |  <- 64 KB
 *     0x80000 |__________________________________________________|
 *             |                                                  |
 *             |                                                  |
 *             |                                                  |
 *             |                  Ramoops                         |  <- 2MB
 *             |                                                  |
 *             |                                                  |
 *     0x280000|__________________________________________________|
 *             |                                                  |
 *             |                   Reserved5                      |  <- 64 KB
 *     0x290000|__________________________________________________|
 *             |                                                  |
 *             |                                                  |
 *             |                  Sys.Cfg/Sys.Info                |  <- 2MB
 *             |                                                  |
 *             |                                                  |
 *     0x490000|__________________________________________________|
 *             |                                                  |
 *             |                   Reserved6                      |  <- 448 KB
 *     0x500000|__________________________________________________|
 *             |                                                  |
 *             |                                                  |
 *             |                    GameData                      |  <- 1 MB
 *             |                                                  |
 *     0x600000|__________________________________________________|
 */

#define TEGRABL_MB2_PARAMS_OFFSET                   0U
#define TEGRABL_CARVEOUT_CPUBL_PARAMS_RSVD1_OFFSET  (TEGRABL_MB2_PARAMS_OFFSET + TEGRABL_CARVEOUT_PAGE_SIZE)
#define TEGRABL_BRBCT_OFFSET                        (TEGRABL_CARVEOUT_CPUBL_PARAMS_RSVD1_OFFSET + \
								TEGRABL_CARVEOUT_PAGE_SIZE)
#define TEGRABL_CARVEOUT_CPUBL_PARAMS_RSVD2_OFFSET  (TEGRABL_BRBCT_OFFSET + TEGRABL_CARVEOUT_PAGE_SIZE)
#define TEGRABL_PROFILER_OFFSET                     (TEGRABL_CARVEOUT_CPUBL_PARAMS_RSVD2_OFFSET + \
								TEGRABL_CARVEOUT_PAGE_SIZE)
#define TEGRABL_CARVEOUT_CPUBL_PARAMS_RSVD3_OFFSET  (TEGRABL_PROFILER_OFFSET + \
								TEGRABL_CARVEOUT_PAGE_SIZE)
#define TEGRABL_GR_OFFSET                           (TEGRABL_CARVEOUT_CPUBL_PARAMS_RSVD3_OFFSET + \
								TEGRABL_CARVEOUT_PAGE_SIZE)
#define TEGRABL_CARVEOUT_CPUBL_PARAMS_RSVD4_OFFSET  (TEGRABL_GR_OFFSET + TEGRABL_CARVEOUT_PAGE_SIZE)
#define TEGRABL_RAMOOPS_OFFSET                      (TEGRABL_CARVEOUT_CPUBL_PARAMS_RSVD4_OFFSET + \
								TEGRABL_CARVEOUT_PAGE_SIZE)
#define TEGRABL_CARVEOUT_CPUBL_PARAMS_RSVD5_OFFSET  (TEGRABL_RAMOOPS_OFFSET + TEGRABL_CARVEOUT_RAMOOPS_SIZE)
#define TEGRABL_SYSCFG_OFFSET                       (TEGRABL_CARVEOUT_CPUBL_PARAMS_RSVD5_OFFSET + \
								TEGRABL_CARVEOUT_PAGE_SIZE)
#define TEGRABL_CARVEOUT_CPUBL_PARAMS_RSVD6_OFFSET  (TEGRABL_SYSCFG_OFFSET + TEGRABL_SYSCFG_SIZE)
#define TEGRABL_CARVEOUT_CPUBL_PARAMS_RSVD6_SIZE    (448 * 1024)
#define TEGRABL_GAMEDATA_OFFSET                     (TEGRABL_CARVEOUT_CPUBL_PARAMS_RSVD6_OFFSET + \
								TEGRABL_CARVEOUT_CPUBL_PARAMS_RSVD6_SIZE)
#define TEGRABL_CARVEOUT_CPUBL_PARAMS_SIZE          (TEGRABL_GAMEDATA_OFFSET + \
								TEGRABL_CARVEOUT_GAMEDATA_SIZE)
#endif
