/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDE_TEGRABL_CLK_RST_PRIVATE_H
#define INCLUDE_TEGRABL_CLK_RST_PRIVATE_H

#include <tegrabl_debug.h>
#include <arclk_rst.h>
#include <tegrabl_drf.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_module.h>
#include <tegrabl_clock.h>
#include <tegrabl_utils.h>

#define NV_CLK_RST_READ_REG(reg)							\
	NV_READ32((((uintptr_t)(NV_ADDRESS_MAP_CAR_BASE)) +		\
				CLK_RST_CONTROLLER_##reg##_0))

#define NV_CLK_RST_WRITE_REG(reg, value)					\
	NV_WRITE32(((uintptr_t)(NV_ADDRESS_MAP_CAR_BASE)) +		\
			CLK_RST_CONTROLLER_##reg##_0, value)

#define NV_CLK_RST_READ_OFFSET(offset)						\
	NV_READ32(((uintptr_t)(NV_ADDRESS_MAP_CAR_BASE)) +		\
			(offset))

#define NV_CLK_RST_WRITE_OFFSET(offset, value)				\
	NV_WRITE32(((uintptr_t)(NV_ADDRESS_MAP_CAR_BASE)) +		\
			(offset), (value))

#define NV_CLK_RST_UPDATE_OFFSET(offset, value)				\
		NV_CLK_RST_WRITE_OFFSET(offset,						\
			NV_CLK_RST_READ_OFFSET(offset) | (value))		\


#define UPDATE_CLK_REG(clk_reg_val, shifted_mask, shifted_val)	\
	do  {														\
		clk_src_reg_val &= ~(shifted_mask);						\
		clk_src_reg_val |= (shifted_val);						\
	} while (0)

#define CLK_SRC_SHIFT _MK_SHIFT_CONST(29)
#define UPDATE_CLK_SRC(clk_src_reg_val, src_mask, src_id) \
	UPDATE_CLK_REG(clk_src_reg_val, src_mask, src_id << CLK_SRC_SHIFT)

#define UPDATE_CLK_DIV(clk_src_reg_val, div_mask, div_val)			\
	UPDATE_CLK_REG(clk_src_reg_val, div_mask, div_val)

#define CHECK_PLL_ENABLE(PLLID)	 \
	NV_DRF_VAL(CLK_RST_CONTROLLER, PLLID##_BASE,			\
			PLLID##_ENABLE,									\
			NV_CLK_RST_READ_REG(PLLID##_BASE))

#define MAX_SRC_ID 0x7

#define CLK_RST_ARRAY(module, source, rate, divtype)				\
{																	\
	{																\
		.rst_set_reg = CLK_RST_CONTROLLER_RST_DEV_##module##_SET_0,	\
		.rst_clr_reg = CLK_RST_CONTROLLER_RST_DEV_##module##_CLR_0,	\
	},																\
	{																\
		.clk_enb_set_reg = CLK_RST_CONTROLLER_CLK_OUT_ENB_##module##_SET_0,	\
		.clk_enb_clr_reg = CLK_RST_CONTROLLER_CLK_OUT_ENB_##module##_CLR_0,	\
		.clk_src_reg = CLK_RST_CONTROLLER_CLK_SOURCE_##module##_0,	\
		.clk_src_mask =												\
			CLK_RST_CONTROLLER_CLK_SOURCE_##module##_0_##module##_CLK_SRC_DEFAULT_MASK << \
			CLK_RST_CONTROLLER_CLK_SOURCE_##module##_0_##module##_CLK_SRC_SHIFT, \
		.clk_div_mask =												\
			CLK_RST_CONTROLLER_CLK_SOURCE_##module##_0_##module##_CLK_DIVISOR_DEFAULT_MASK << \
			CLK_RST_CONTROLLER_CLK_SOURCE_##module##_0_##module##_CLK_DIVISOR_SHIFT, \
		.src_list = NULL,											\
		.div_type = TEGRABL_CLK_DIV_TYPE_##divtype,					\
		.clk_src = TEGRABL_CLK_SRC_##source,						\
		.clk_src_idx = CLK_RST_CONTROLLER_CLK_SOURCE_##module##_0_##module##_CLK_SRC_##source, \
		.clk_rate = 0,												\
		.resume_rate = (rate),												\
		.ON  = false,												\
		.allow_src_switch = false,									\
	},														\
	.bit_offset = 0,												\
}

#define CLK_RST_ARRAY_ALLOW_SWITCH(module, list_name, source, rate, divtype)	\
{																	\
	{																\
		.rst_set_reg = CLK_RST_CONTROLLER_RST_DEV_##module##_SET_0, \
		.rst_clr_reg = CLK_RST_CONTROLLER_RST_DEV_##module##_CLR_0, \
	},																\
	{																\
		.clk_enb_set_reg = CLK_RST_CONTROLLER_CLK_OUT_ENB_##module##_SET_0, \
		.clk_enb_clr_reg = CLK_RST_CONTROLLER_CLK_OUT_ENB_##module##_CLR_0, \
		.clk_src_reg = CLK_RST_CONTROLLER_CLK_SOURCE_##module##_0,	\
		.clk_src_mask =												\
			CLK_RST_CONTROLLER_CLK_SOURCE_##module##_0_##module##_CLK_SRC_DEFAULT_MASK << \
			CLK_RST_CONTROLLER_CLK_SOURCE_##module##_0_##module##_CLK_SRC_SHIFT, \
		.clk_div_mask =												\
			CLK_RST_CONTROLLER_CLK_SOURCE_##module##_0_##module##_CLK_DIVISOR_DEFAULT_MASK << \
			CLK_RST_CONTROLLER_CLK_SOURCE_##module##_0_##module##_CLK_DIVISOR_SHIFT, \
		.src_list = g_##list_name##_src_list,						\
		.div_type = TEGRABL_CLK_DIV_TYPE_##divtype,					\
		.clk_src = TEGRABL_CLK_SRC_##source,						\
		.clk_src_idx = CLK_RST_CONTROLLER_CLK_SOURCE_##module##_0_##module##_CLK_SRC_##source, \
		.clk_rate = 0,											\
		.resume_rate = (rate),										\
		.ON  = false,												\
		.allow_src_switch = true,									\
	},														\
	.bit_offset = 0,												\
}

#define CLK_RST_NO_SRC_ARRAY(module, offset)								\
{																	\
	{																\
		.rst_set_reg = CLK_RST_CONTROLLER_RST_DEV_##module##_SET_0, \
		.rst_clr_reg = CLK_RST_CONTROLLER_RST_DEV_##module##_CLR_0, \
	},																\
	{																\
		.clk_enb_set_reg = CLK_RST_CONTROLLER_CLK_OUT_ENB_##module##_SET_0, \
		.clk_enb_clr_reg = CLK_RST_CONTROLLER_CLK_OUT_ENB_##module##_CLR_0, \
		.clk_src_reg = 0,											\
		.clk_src_mask = 0,											\
		.clk_div_mask = 0,											\
		.src_list = NULL,											\
		.div_type = TEGRABL_CLK_DIV_TYPE_INVALID,					\
		.clk_src = TEGRABL_CLK_SRC_INVALID,							\
		.clk_src_idx = 0,											\
		.clk_rate = 0,												\
		.resume_rate = 0,											\
		.ON  = false,												\
		.allow_src_switch = false,									\
	},														\
	.bit_offset = (offset),											\
}

#define CLK_RST_NO_CLK_ARRAY(module, offset)						        \
{												\
	{											\
		.rst_set_reg = CLK_RST_CONTROLLER_RST_DEV_##module##_SET_0,			\
		.rst_clr_reg = CLK_RST_CONTROLLER_RST_DEV_##module##_CLR_0,			\
	},																\
	{0x0},														\
	.bit_offset = (offset),											\
}

#define CLK_RST_NO_RST_ARRAY_ALLOW_SWITCH(enb_module, src_module, offset,	\
		list_name, source, rate, divtype)									\
{																			\
	{0x0},																	\
	{																		\
		.clk_enb_set_reg = CLK_RST_CONTROLLER_CLK_OUT_ENB_##enb_module##_SET_0, \
		.clk_enb_clr_reg = CLK_RST_CONTROLLER_CLK_OUT_ENB_##enb_module##_CLR_0, \
		.clk_src_reg = CLK_RST_CONTROLLER_CLK_SOURCE_##src_module##_0,		\
		.clk_src_mask =														\
			CLK_RST_CONTROLLER_CLK_SOURCE_##src_module##_0_##src_module##_CLK_SRC_DEFAULT_MASK << \
			CLK_RST_CONTROLLER_CLK_SOURCE_##src_module##_0_##src_module##_CLK_SRC_SHIFT, \
		.clk_div_mask =														\
			CLK_RST_CONTROLLER_CLK_SOURCE_##src_module##_0_##src_module##_CLK_DIVISOR_DEFAULT_MASK << \
			CLK_RST_CONTROLLER_CLK_SOURCE_##src_module##_0_##src_module##_CLK_DIVISOR_SHIFT, \
		.src_list = g_##list_name##_src_list,								\
		.div_type = TEGRABL_CLK_DIV_TYPE_##divtype,							\
		.clk_src = TEGRABL_CLK_SRC_##source,								\
		.clk_src_idx = CLK_RST_CONTROLLER_CLK_SOURCE_##src_module##_0_##src_module##_CLK_SRC_##source, \
		.clk_rate = 0,														\
		.resume_rate = rate,												\
		.ON  = false,														\
		.allow_src_switch = true,											\
	},																		\
	.bit_offset = offset,													\
}

#define CLK_RST_NO_SRC_NO_RST_ARRAY(module, offset)								\
{													\
	{0x0},												\
	{																\
		.clk_enb_set_reg = CLK_RST_CONTROLLER_CLK_OUT_ENB_##module##_SET_0, \
		.clk_enb_clr_reg = CLK_RST_CONTROLLER_CLK_OUT_ENB_##module##_CLR_0, \
		.clk_src_reg = 0,											\
		.clk_src_mask = 0,											\
		.clk_div_mask = 0,											\
		.src_list = NULL,											\
		.div_type = TEGRABL_CLK_DIV_TYPE_INVALID,					\
		.clk_src = TEGRABL_CLK_SRC_INVALID,							\
		.clk_src_idx = 0,											\
		.clk_rate = 0,												\
		.resume_rate = 0,											\
		.ON  = false,												\
		.allow_src_switch = false,									\
	},																\
	.bit_offset = (offset),											\
}

/* struct module_car_info has name parameter only when TEGRABL_DEBUG is defined
 * so handle both cases separately */
#if defined(TEGRABL_DEBUG)

/* In this driver we assert in case of erroneous calls, CLOCK_BUG macro
 * is for verbose and unconditional assertions */
#define CLOCK_BUG(module, instance, fmt, ...)			\
	do {												\
		if ((module) < ARRAY_SIZE(s_module_car_info)) {	\
			TEGRABL_BUG("module[%s] instance[%u]" fmt,	\
					s_module_car_info[(module)].name,	\
					(instance) , ##__VA_ARGS__);		\
		} else {										\
			TEGRABL_BUG("module[%u] instance[%u]" fmt,	\
				(module), (instance) , ##__VA_ARGS__);	\
		}												\
	} while (0)

#define CLOCK_DEBUG(module, instance, fmt, ...)			\
	do {												\
		if ((module) < ARRAY_SIZE(s_module_car_info)) {	\
			pr_info("module[%s] instance[%u]"			\
				fmt, s_module_car_info[(module)].name,	\
				(instance) , ##__VA_ARGS__);			\
		} else {										\
			pr_info("module[%s] instance[%d]"			\
			fmt, (module), (instance) , ##__VA_ARGS__);	\
		}												\
	} while (0)

#define CLOCKINFO_DATA(module)							\
{														\
	#module,											\
	ARRAY_SIZE(g_##module##_car_info),					\
	(struct car_info *)&(g_##module##_car_info),				\
}

/* This is for cases which need to be dealt explicitly */
#define CLOCKINFO_DATA_DUMMY(module)					\
{														\
	#module,											\
	0,													\
	NULL,												\
}

#else

#define CLOCK_BUG(module, instance, fmt, ...)			\
		TEGRABL_BUG("module[%u] instance[%u]" fmt,		\
					(module),							\
					(instance) , ##__VA_ARGS__)

#define CLOCK_DEBUG(module, instance, fmt, ...)			\
		pr_debug("module[%u] instance[%d]"				\
				 fmt, (module), (instance),				\
				 ##__VA_ARGS__)

#define CLOCKINFO_DATA(module)							\
{														\
	ARRAY_SIZE(g_##module##_car_info),					\
	(struct car_info *)&g_##module##_car_info,					\
}

#define CLOCKINFO_DATA_DUMMY(module)					\
{														\
	0,													\
	NULL,												\
}

#endif	  /* TEGRABL_DEBUG */

#define TEGRABL_CLK_CHECK_SRC_ID(src_id)				\
		(((src_id) == TEGRABL_CLK_SRC_CLK_M) ||			\
		((src_id) == TEGRABL_CLK_SRC_PLLP_OUT0) ||		\
		((src_id) == TEGRABL_CLK_SRC_PLLM_OUT0) ||		\
		((src_id) == TEGRABL_CLK_SRC_PLLC4_OUT0_LJ) ||	\
		((src_id) == TEGRABL_CLK_SRC_PLLE))

struct clk_info {
	/* Addr of clk enable-set register */
	uint32_t clk_enb_set_reg;
	/* Addr of clk enable-clear register */
	uint32_t clk_enb_clr_reg;
	/* Addr of clock source regsiter */
	uint32_t clk_src_reg;
	/* Mask of clock source field in clk_src register */
	uint32_t clk_src_mask;
	/* Mask of divisor field in clk_src register */
	uint32_t clk_div_mask;
	/* List of the 8 possible clk sources in the order of
	 * their indexes given in CLK_SOURCE register */
	tegrabl_clk_src_id_t *src_list;
	/* Divider type - fractional or regular */
	tegrabl_clk_div_type_t div_type;
	/* Current clock source */
	tegrabl_clk_src_id_t clk_src;
	/* Index of current clock source */
	uint32_t clk_src_idx;
	/* Current rate of the clock */
	uint32_t clk_rate;
	/* Init rate or rate at which clock will be resumed
	 * after enable/re-enable */
	uint32_t resume_rate;
	/* Whether reset is asserted for the module */
	bool ON;
	/* Whether the module can be allowed to switch sources.*/
	bool allow_src_switch;
};

struct rst_info {
	/* Addr of reset-set register */
	uint32_t rst_set_reg;
	/* Addr of reset-clear register */
	uint32_t rst_clr_reg;
};

struct car_info {
	struct rst_info reset_info;
	struct clk_info clock_info;
	/* Bit offset in rst/clk set/clear register */
	uint32_t bit_offset;
};

struct module_car_info {
#if defined(TEGRABL_DEBUG)
	const char *name;
#endif
	uint32_t instance_count;
	struct car_info *pcar_info;
};

struct utmi_pll_clock_params {
	/* pll feedback divider */
	uint32_t n;
	/*pll input dividier */
	uint32_t m;
};

struct usb_pll_delay_params {
	uint16_t enable_delay_count;
	uint16_t stable_count;
	uint16_t active_delay_count;
	uint16_t xtal_freq_count;
};

/* Global module car info table */
extern struct module_car_info g_module_carinfo[TEGRABL_MODULE_NUM];

bool module_support(tegrabl_module_t module, uint8_t instance);

uint32_t get_divider(uint32_t src_rate, uint32_t module_rate,
		tegrabl_clk_div_type_t divtype);

uint32_t get_clk_rate_khz(
		uint32_t src_rate_khz,
		uint32_t div,
		tegrabl_clk_div_type_t div_type);

void update_clk_src_reg(
		tegrabl_module_t module,
		uint32_t instance,
		struct clk_info *pclk_info,
		uint32_t div, uint32_t src_idx);

tegrabl_error_t tegrabl_clk_enb_set(tegrabl_module_t module,
		uint8_t instance,
		bool enable,
		void *priv_data);

bool tegrabl_clk_is_enb_set(
		tegrabl_module_t module,
		uint8_t instance);

tegrabl_error_t tegrabl_rst_set(tegrabl_module_t module,
		uint8_t instance,
		bool enable);

int get_src_idx(
		tegrabl_clk_src_id_t *src_list,
		tegrabl_clk_src_id_t clk_src);

#endif /* INCLUDE_TEGRABL_CLK_RST_PRIVATE_H */
