/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDE_TEGRABL_UART_SOC_LOCAL_H
#define INCLUDE_TEGRABL_UART_SOC_LOCAL_H

#include <tegrabl_uart.h>

void tegrabl_uart_soc_get_info(struct tegrabl_uart **huart,
	uint32_t *num_of_instances);

#endif // INCLUDE_TEGRABL_UART_SOC_LOCAL_H
