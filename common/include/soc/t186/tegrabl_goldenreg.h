/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_GR_H
#define INCLUDED_TEGRABL_GR_H

struct tegrabl_gr_hdr {
	uint32_t mb1_offset;
	uint32_t mb1_size;
	uint32_t mb2_offset;
	uint32_t mb2_size;
	uint32_t cpu_bl_offset;
	uint32_t cpu_bl_size;
};


struct tegrabl_gr_value {
	/* Register Address */
	uint32_t gr_address;
	/* Value corresponding to this address */
	uint32_t gr_value;
};


enum tegrabl_gr_state {
	TEGRABL_GR_MB1,
	TEGRABL_GR_MB2,
	TEGRABL_GR_CPU_BL,
};

#define TEGRABL_GR_CARVEOUT_SIZE (64 * 1024)


/**
 * @brief dumps the golden registers specified by golden_reg.h
 */
void tegrabl_dump_golden_regs(enum tegrabl_gr_state state, uint64_t start);

#endif /* INCLUDED_TEGRABL_GR_H */
