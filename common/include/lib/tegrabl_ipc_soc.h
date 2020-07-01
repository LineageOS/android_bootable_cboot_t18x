/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef TEGRABL_IPC_SOC_H
#define TEGRABL_IPC_SOC_H

/**
 * Defines communication channels for CCPlex <--> BPMP
 */
#define CH0_CPU_0_TO_BPMP          0
#define NO_OF_CHANNELS             1

/**
 * IVC specific defines
 */
#define MSG_SZ                          128
#define MSG_DATA_SZ                     120

#define IPC_CPU_MASTER_PHYS_BASE        (0x3004E000)
#define IPC_CPU_MASTER_VIRT_BASE        (IPC_CPU_MASTER_PHYS_BASE)
#define IPC_CPU_MASTER_SIZE             (4 * KB)

#define IPC_CPU_SLAVE_PHYS_BASE         (0x3004F000)
#define IPC_CPU_SLAVE_VIRT_BASE         (IPC_CPU_SLAVE_PHYS_BASE)
#define IPC_CPU_SLAVE_SIZE              (4 * KB)

#endif /* TEGRABL_IPC_SOC_H */
