/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

#ifndef TEGRABL_RPB_H
#define TEGRABL_RPB_H

#define RPB_SIG_OFF 256

struct tegrabl_rpb_handle {
	uint8_t magic[4];
	uint32_t version;
	uint32_t ecid[4];
	uint8_t reserved[232];
	uint8_t signature[256];
};

#endif
