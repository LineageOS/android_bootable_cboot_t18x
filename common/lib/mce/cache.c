/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_NO_MODULE

#include "build_config.h"
#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_arm64_smccc.h>
#include <tegrabl_mce.h>

tegrabl_error_t tegrabl_mce_roc_cache_flush(void)
{
	struct tegrabl_arm64_smc64_params smc_regs = {
		{MCE_SMC_ROC_FLUSH_CACHE}
	};

	tegrabl_arm64_send_smc64(&smc_regs);

	if (smc_regs.reg[0] != 0) {
		pr_error("SMC_ROC_FLUSH_CACHE failed\n");
		pr_error("SMC returned 0x%lx 0x%lx 0x%lx 0x%lx\n",
				 smc_regs.reg[0], smc_regs.reg[1],
				 smc_regs.reg[2], smc_regs.reg[3]);

		return TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 0);
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_mce_roc_cache_clean(void)
{
	struct tegrabl_arm64_smc64_params smc_regs = {
		{MCE_SMC_ROC_CLEAN_CACHE_ONLY}
	};

	tegrabl_arm64_send_smc64(&smc_regs);

	if (smc_regs.reg[0] != 0) {
		pr_error("SMC_ROC_FLUSH_CLEAN_CACHE_ONLY failed\n");
		pr_error("SMC returned 0x%lx 0x%lx 0x%lx 0x%lx\n",
				 smc_regs.reg[0], smc_regs.reg[1],
				 smc_regs.reg[2], smc_regs.reg[3]);

		return TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 0);
	}

	return TEGRABL_NO_ERROR;
}
