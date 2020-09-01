/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_DISPLAY

#include <tegrabl_display_soc.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_bpmp_fw_interface.h>
#include <bpmp_abi.h>
#include <powergate-t186.h>
#include <tegrabl_i2c.h>

void tegrabl_display_unpowergate(void)
{
	struct mrq_pg_request disp_pg_request = {
		.cmd = CMD_PG_SET_STATE,
		.id = TEGRA186_POWER_DOMAIN_DISP,
		.set_state = {
			.state = PG_STATE_ON,
		}
	};

	while (disp_pg_request.id <= TEGRA186_POWER_DOMAIN_DISPC) {
		if (tegrabl_ccplex_bpmp_xfer(&disp_pg_request, NULL, sizeof(disp_pg_request),
				0, MRQ_PG) != TEGRABL_NO_ERROR) {
			pr_error("%s: Unable to power on - TEGRA186_POWER_DOMAIN_DISP%c\n",
					 __func__, disp_pg_request.id - TEGRA186_POWER_DOMAIN_DISP == 0 ?
					 ' ' : disp_pg_request.id - TEGRA186_POWER_DOMAIN_DISP + 'A');
		}

		(disp_pg_request.id)++;
	}

	pr_debug("%s: unpowergate done\n", __func__);
}

void tegrabl_display_powergate(void)
{
	struct mrq_pg_request disp_pg_request = {
		.cmd = CMD_PG_SET_STATE,
		.id = TEGRA186_POWER_DOMAIN_DISP,
		.set_state = {
			.state = PG_STATE_OFF,
		}
	};

	while (disp_pg_request.id <= TEGRA186_POWER_DOMAIN_DISPC) {
		if (tegrabl_ccplex_bpmp_xfer(&disp_pg_request, NULL, sizeof(disp_pg_request),
				0, MRQ_PG) != TEGRABL_NO_ERROR) {
			pr_error("%s: Unable to power on - TEGRA194_POWER_DOMAIN_DISP%c\n",
					 __func__, disp_pg_request.id - TEGRA186_POWER_DOMAIN_DISP == 0 ?
					 ' ' : disp_pg_request.id - TEGRA186_POWER_DOMAIN_DISP + 'A');
		}

		(disp_pg_request.id)++;
	}

	pr_debug("%s: display powergate done\n", __func__);
}

tegrabl_error_t tegrabl_display_get_i2c(int32_t sor_instance, uint32_t *i2c_instance)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	switch (sor_instance) {
	case 0:
		*i2c_instance = TEGRABL_INSTANCE_I2C6;
		break;
	case 1:
		*i2c_instance = TEGRABL_INSTANCE_I2C4;
		break;
	default:
		pr_error("%s: invalid SOR instance %d\n", __func__, sor_instance);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, 0);
	}

	return err;
}
