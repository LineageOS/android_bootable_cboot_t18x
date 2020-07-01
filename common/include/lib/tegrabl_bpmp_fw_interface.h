/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file
 * @brief <b>NVIDIA TEGRABL Interface: BPMP IPC </b>
 *
 * @b Description: This file declares APIs for CCPLEX <-> BPMP IPC
 */

#ifndef INCLUDED_TEGRABL_BPMP_FW_INTERFACE_H
#define INCLUDED_TEGRABL_BPMP_FW_INTERFACE_H

#include <tegrabl_error.h>

/**
 * @brief Performs CCPLEX <-> BPMP IPC initialization.
 *
 * @param none
 * @retval TEGRABL_NO_ERROR if successful, or the appropriate error code.
 */
tegrabl_error_t tegrabl_ipc_init(void);

/**
 * @brief Send input data buffer to BPMP and reieve data from BPMP in output
 * data buffer.
 *
 * @param p_data_out Pointer to output data buffer
 * @param p_data_in Pointer to input data buffer
 * @param size_out Output data length
 * @param size_in Input data length
 * @param mrq MRQ_ID as per the bpmp-abi
 *
 * @retval response by bpmp (mrq) in case of successful IPC transaction
 *         and -1 in case of failure
 */
tegrabl_error_t tegrabl_ccplex_bpmp_xfer(
		void *p_data_out,
		void *p_data_in,
		uint32_t size_out,
		uint32_t size_in,
		uint32_t mrq);

#endif /* INCLUDED_TEGRABL_BPMP_FW_INTERFACE_H */
