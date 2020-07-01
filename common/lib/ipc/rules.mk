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

MODULE_DEPS += \

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/../../include/lib \
	$(LOCAL_DIR)/../../../../../../core/include/ \
	$(LOCAL_DIR)/../../../../../nvtboot/common/include

MODULE_SRCS += \
	$(LOCAL_DIR)/tegrabl_bpmp_fw_interface.c \
	$(LOCAL_DIR)/../../drivers/timer \
	$(LOCAL_DIR)/tegra-ivc.c

include make/module.mk

