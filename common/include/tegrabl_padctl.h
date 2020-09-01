/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDE_TEGRABL_PADCTL_H
#define INCLUDE_TEGRABL_PADCTL_H

#include <stdint.h>
/**
 *  @brief convert a particular pin to GPIO mode to check hdmi cable connection
 *
 *  @param pin_num pin number to be converted to GPIO mode
 */
void tegrabl_padctl_config_to_gpio(uint32_t pin_num);

#endif /* INCLUDE_TEGRABL_PADCTL_H */
