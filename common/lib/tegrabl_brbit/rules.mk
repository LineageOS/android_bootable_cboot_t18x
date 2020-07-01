#
# Copyright (c) 2016, NVIDIA Corporation.  All Rights Reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property and
# proprietary rights in and to this software and related documentation.  Any
# use, reproduction, disclosure or distribution of this software and related
# documentation without an express license agreement from NVIDIA Corporation
# is strictly prohibited.
#

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR) \
	$(LOCAL_DIR)/../../../../common/include \
	$(LOCAL_DIR)/../../../../common/include/lib \
	$(LOCAL_DIR)/../../include/lib \

MODULE_SRCS += \
	$(LOCAL_DIR)/tegrabl_brbit.c \
	$(LOCAL_DIR)/tegrabl_brbit_t18x.c

include make/module.mk

