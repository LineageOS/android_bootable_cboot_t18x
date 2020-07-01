/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_SATA_CONFIG_H
#define INCLUDED_TEGRABL_SATA_CONFIG_H

#define FPCI_BAR5_START_VAL 0x0040020
#define NVOOB_COMMA_CNT_VAL 0x07
#define CFG2NVOOB_2_COMWAKE_IDLE_CNT_LOW_VAL 0xC
#define NVOOB_SQUELCH_FILTER_LENGTH_VAL 0x3
#define NVOOB_SQUELCH_FILTER_MODE_VAL 0x1
#define BKDOOR_CC_CLASS_CODE_VAL 0x0106
#define BKDOOR_CC_PROG_IF_VAL 0x85

#define CFG9_BASE_ADDRESS_VAL 0x40020000

#define SATA_CLK_FREQUENCY_VAL 102000
#define SATA_OOB_CLK_FREQUENCY_VAL 204000

#endif
