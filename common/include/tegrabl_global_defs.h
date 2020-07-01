/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

/**
 * @file tegrabl_global_defs.h
 * @brief This file contains definition that are shared between all the
 *        bootloader components.
 */

#ifndef INCLUDED_TEGRABL_GLOBAL_DEFS_H
#define INCLUDED_TEGRABL_GLOBAL_DEFS_H

#define KB (1024U)
#define MB (1024U * 1024U)
#define GB (1024U * 1024U * 1024U)

#define MAPPED_CO_VA  0xc0000000U
#define MAPPED_CO_SIZE  (512U * MB)
#define CARVEOUT_AST_REGION 7U
#define END_ADDR_32_REGION 0xFFFFFFFFLLU

#define CLEAR_BIT(val, bit_pos)  val = val & (~(1 << bit_pos))

#endif /* INCLUDED_TEGRABL_GLOBAL_DEFS_H */
