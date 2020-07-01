/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

#ifndef TEGRABL_PKC_OPS_H
#define TEGRABL_PKC_OPS_H

#include <tegrabl_error.h>

/**
 * @brief read PKC modulus from BCT
 *
 * @param modulus buffer to store PKC
 *
 * @return TEGRABL_NO_ERROR if successful, else appropriate error
 */
tegrabl_error_t tegrabl_pkc_modulus_get(uint8_t *modulus);

#endif
