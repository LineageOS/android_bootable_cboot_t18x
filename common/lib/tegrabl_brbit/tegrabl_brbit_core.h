/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef TEGRABL_BRBIT_CORE_H
#define TEGRABL_BRBIT_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <tegrabl_error.h>
#include <stdbool.h>

/**
 * @brief Returns the address of Boot Information Table in memory.
 *
 * @return Address of BIT.
 */
void *tegrabl_brbit_location(void);

/**
 * @brief Verifies the version information filled in BIT.
 *
 * @param buffer Reference to BIT.
 *
 * @return true if verification successful else false.
 */
bool tegrabl_brbit_verify(void *buffer);

#endif /* TEGRABL_BRBIT_CORE_H */

