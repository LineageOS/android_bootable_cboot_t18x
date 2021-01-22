/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

/**
 * @file tegrabl_pcie_soc.c
 */

tegrabl_error_t tegrabl_pcie_soc_preinit(uint8_t ctrl_num)
{
	pr_error("%s: is not supported for T18x\n", __func__);
	return TEGRABL_ERR_NOT_SUPPORTED'
}

tegrabl_error_t tegrabl_pcie_soc_init(uint8_t ctrl_num, uint8_t link_speed)
{
	pr_error("%s: is not supported for T18x\n", __func__);
	return TEGRABL_ERR_NOT_SUPPORTED'
}
