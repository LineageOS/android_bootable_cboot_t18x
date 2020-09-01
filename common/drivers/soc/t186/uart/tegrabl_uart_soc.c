/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <tegrabl_uart.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_uart_soc_local.h>

static struct tegrabl_uart uart_list[] =  {
	{
		.instance = 0,
		.base_addr = NV_ADDRESS_MAP_UARTA_BASE,
	},
	{
		.instance = 1,
		.base_addr = NV_ADDRESS_MAP_UARTB_BASE,
	},
	{
		.instance = 2,
		.base_addr = NV_ADDRESS_MAP_UARTC_BASE,
	},
	{
		.instance = 3,
		.base_addr = NV_ADDRESS_MAP_UARTD_BASE,
	},
	{
		.instance = 4,
		.base_addr = NV_ADDRESS_MAP_UARTE_BASE,
	},
	{
		.instance = 5,
		.base_addr = NV_ADDRESS_MAP_UARTF_BASE,
	},
	{
		.instance = 6,
		.base_addr = NV_ADDRESS_MAP_UARTG_BASE,
	},
};

void tegrabl_uart_soc_get_info(struct tegrabl_uart **huart,
	uint32_t *num_of_instances)
{
	*huart = &uart_list[0];
	*num_of_instances = ARRAY_SIZE(uart_list);

	return;
}
