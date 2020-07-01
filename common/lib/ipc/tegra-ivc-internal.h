/*
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef __tegra_ivc_internal_h__
#define  __tegra_ivc_internal_h__

#include <stddef.h>

#define IVC_ALIGN 64

#ifdef IVC_ARM_FULL_BARRIERS
/*
 * Older toolchains do not support the store-only barriers or the inner-
 * shareable designation, so we're forced to use full barriers.
 */
#define dmb(option) asm volatile("dmb " ::: "memory")
#else
#define dmb(option) asm volatile("dmb " #option ::: "memory")
#endif

#ifdef __ghs__
#ifdef __ARM64__
#define IVC_AARCH64
#endif
#elif defined (__GNUC__)
#ifdef __aarch64__
#define IVC_AARCH64
#endif
#else
#error "unsupported compiler"
#endif

static inline void ivc_mb(void)
{
#ifdef IVC_AARCH64
	dmb(ish);
#else
	dmb();
#endif
}
static inline void ivc_rmb(void)
{
#ifdef IVC_AARCH64
	dmb(ld);
#else
	dmb();
#endif
}
static inline void ivc_wmb(void)
{
	dmb(st);
}

void abort(void);

#ifdef IVC_DEBUG
# define IVC_ASSERT(c) do { if (!(c)) abort(); } while (0);
#else
# define IVC_ASSERT(c)
#endif

#define IVC_UNUSED __attribute__((unused))
#define IVC_WEAK __attribute__((weak))

#endif /* __tegra_ivc_internal_h__ */
