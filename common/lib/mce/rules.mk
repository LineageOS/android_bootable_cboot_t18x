#
# Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software and related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/../../../../common/include \
	$(LOCAL_DIR)/../../../../common/include/arch \
	$(LOCAL_DIR)/../../common/include \

MODULE_SRCS += \
	$(LOCAL_DIR)/cache.c

include make/module.mk
