/*
 * Copyright (c) 2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file tegrabl_page_allocator_pool_map.h
 * @brief This file contains mappimg info used for page allocator
 * intialization
 */

#ifndef INCLUDED_TEGRABL_PAGE_ALLOCATOR_POOL_MAP_H
#define INCLUDED_TEGRABL_PAGE_ALLOCATOR_POOL_MAP_H

/**
 * Using POOL 1 as DRAM region with ECC Disabled
 */
#define TEGRABL_MEMORY_DRAM					TEGRABL_MEMORY_POOL1

/**
 * Maximum number of memory block required
 */
#define MEMORY_BLOCKS_NUM					2

#endif /* INCLUDED_TEGRABL_PAGE_ALLOCATOR_POOL_MAP_H */
