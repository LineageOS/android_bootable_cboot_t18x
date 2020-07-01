/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDE_TEGRABL_DISPLAY_SOC_H
#define INCLUDE_TEGRABL_DISPLAY_SOC_H

#include <tegrabl_display.h>
#include <tegrabl_error.h>

#define NVDISPLAY_NODE "nvidia,tegra186-dc"
#define HOST1X_NODE "nvidia,tegra186-host1x\0simple-bus"

/**
 *  @brief unpowergate display partitions
 */
void tegrabl_display_unpowergate(void);

/**
 *  @brief powergate display partitions
 */
void tegrabl_display_powergate(void);

/**
 *  @brief return the i2c instance corresponding to particular sor instance
 *
 *  @param sor_instance sor instance
 *  @param i2c_instance pointer to return i2c instance.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_display_get_i2c(int32_t sor_instance, uint32_t *i2c_instance);

#endif /* INCLUDE_TEGRABL_DISPLAY_SOC_H */
