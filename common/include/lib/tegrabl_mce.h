/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_MCE_H
#define INCLUDED_TEGRABL_MCE_H

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_arm64_smccc.h>

/* MCE command enums for SMC calls */
#define MCE_SMC_ROC_FLUSH_CACHE 0x82FFFF0B
#define MCE_SMC_ROC_FLUSH_CACHE_ONLY 0x82FFFF0E
#define MCE_SMC_ROC_CLEAN_CACHE_ONLY 0x82FFFF0F

/**
 * @brief Perform ROC-based flush of all caches
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise appropriate error code
 */
tegrabl_error_t tegrabl_mce_roc_cache_flush(void);

/**
 * @brief Perform ROC-based clean of all caches
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise appropriate error code
 */
tegrabl_error_t tegrabl_mce_roc_cache_clean(void);

#endif /* INCLUDED_TEGRABL_MCE_H */
