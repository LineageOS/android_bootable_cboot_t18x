/*
 * Copyright (c) 2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/**
 * @file  tegrabl_prevent_rollback.h
 * @brief Define rollback prevention APIs
 *
 * Mb1 does 2 jobs on rollback prevention. 1. Read rollback data from mb1bct and
 * init the rollback framework. 2. Check rollback attempt when loading rollback
 * sensitive binaries.
 * MB2 and CPUBL can get the rollback struct data from previous BL and run the
 * rollback check logic as mb1
 */

#ifndef TEGRABL_PREVENT_ROLLBACK_H
#define TEGRABL_PREVENT_ROLLBACK_H

#include <tegrabl_error.h>
#include <tegrabl_rollback_prevention.h>

/*
 * @brief Transfer rollback level in fuse to readable value
 * @param fuseValue rollback level value in fuse
 * @return Num of bits set in the fuse value
 */
uint8_t tegrabl_rollback_fusevalue_to_level(uint32_t fuse_value);

/*
 * @brief Transfer readable rollback level to the value burnt to fuse
 * @para fuseLevel rollback level
 * @return Rollback level value burnt to fuse
 */
uint32_t tegrabl_rollback_level_to_fusevalue(uint8_t fuse_level);

/* @brief Initialize the rollback data struct
 *
 * @param rb rollback data pointer to be copied locally
 *
 * @return TEGRABL_NO_ERROR if no error
 */
tegrabl_error_t tegrabl_init_rollback_data(struct tegrabl_rollback *rb);

/*
 * @brief Get rollback data from mb1_bct
 *
 * @return Rollback data pointer */
struct tegrabl_rollback *tegrabl_get_rollback_data(void);

/*
 * @brief Disable rollback prevention
 *
 * @return TEGRABL_NO_ERROR if no error
 */
tegrabl_error_t tegrabl_disable_rollback_prevention(void);

/*
 * @brief Bypass rollback prevention per RPB
 *
 * @param p_rpb RPB binary address
 *
 * @return TEGRABL_NO_ERROR if succeed
 */
tegrabl_error_t tegrabl_bypass_rollback_prevention(void *p_rpb);

/*
 * @brief Burn rollback fuse to required value
 *
 * @param fuse_value rollback level to be fused
 *
 * @return TEGRABL_NO_ERROR if fuse burnt successfully
 */
tegrabl_error_t tegrabl_update_rollback_fuse(uint32_t fuse_value);

/*
 * @brief Check if the binary has a rollback attempt
 *
 * @param bin_type binary type
 * @param rollback_level rollback sw level
 *
 * @return TEGRABL_NO_ERROR if no rollback attempt, else invalid error code */
tegrabl_error_t tegrabl_check_binary_rollback(uint32_t bin_type,
											  uint16_t rollback_level);

/*
 * @brief Do rollback prevention logic with given rollback data
 *
 * @return TEGRABL_NO_ERROR if succeed */
tegrabl_error_t tegrabl_prevent_rollback(void);

#endif /* TEGRABL_PREVENT_ROLLBACK_H */
