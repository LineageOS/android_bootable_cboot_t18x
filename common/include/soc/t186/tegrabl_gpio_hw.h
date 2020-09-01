/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef _TEGRABL_GPIO_HW_H_
#define _TEGRABL_GPIO_HW_H_

#define GPIO_MAIN_NODE	"nvidia,tegra186-gpio"
#define GPIO_AON_NODE	"nvidia,tegra186-gpio-aon"

/* GPIOs implemented by main GPIO controller */
#define TEGRA_GPIO_BANK_ID_A 0
#define TEGRA_GPIO_BANK_ID_B 1
#define TEGRA_GPIO_BANK_ID_C 2
#define TEGRA_GPIO_BANK_ID_D 3
#define TEGRA_GPIO_BANK_ID_E 4
#define TEGRA_GPIO_BANK_ID_F 5
#define TEGRA_GPIO_BANK_ID_G 6
#define TEGRA_GPIO_BANK_ID_H 7
#define TEGRA_GPIO_BANK_ID_I 8
#define TEGRA_GPIO_BANK_ID_J 9
#define TEGRA_GPIO_BANK_ID_K 10
#define TEGRA_GPIO_BANK_ID_L 11
#define TEGRA_GPIO_BANK_ID_M 12
#define TEGRA_GPIO_BANK_ID_N 13
#define TEGRA_GPIO_BANK_ID_O 14
#define TEGRA_GPIO_BANK_ID_P 15
#define TEGRA_GPIO_BANK_ID_Q 16
#define TEGRA_GPIO_BANK_ID_R 17
#define TEGRA_GPIO_BANK_ID_T 18
#define TEGRA_GPIO_BANK_ID_X 19
#define TEGRA_GPIO_BANK_ID_Y 20
#define TEGRA_GPIO_BANK_ID_BB 21
#define TEGRA_GPIO_BANK_ID_CC 22

/* GPIOs implemented by AON GPIO controller */
#define TEGRA_GPIO_BANK_ID_S 0
#define TEGRA_GPIO_BANK_ID_U 1
#define TEGRA_GPIO_BANK_ID_V 2
#define TEGRA_GPIO_BANK_ID_W 3
#define TEGRA_GPIO_BANK_ID_Z 4
#define TEGRA_GPIO_BANK_ID_AA 5
#define TEGRA_GPIO_BANK_ID_EE 6
#define TEGRA_GPIO_BANK_ID_FF 7

#endif
