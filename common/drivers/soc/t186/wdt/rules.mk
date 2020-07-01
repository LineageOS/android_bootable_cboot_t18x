#
# Copyright (c) 2016, NVIDIA CORPORATION.  All Rights Reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR) \
	$(LOCAL_DIR)/../../../../common/include \
	$(LOCAL_DIR)/../../../../common/include/drivers \
	$(LOCAL_DIR)/../../../../common/include/lib \
        $(LOCAL_DIR)/../../../../include/drivers

MODULE_SRCS += \
	$(LOCAL_DIR)/tegrabl_wdt.c

include make/module.mk
