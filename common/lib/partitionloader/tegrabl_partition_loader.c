/*
 * Copyright (c) 2015-2020, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_LOADER

#include <stdint.h>
#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_utils.h>
#include <tegrabl_partition_manager.h>
#include <tegrabl_partition_loader.h>
#include <tegrabl_binary_types.h>
#include <tegrabl_gpt.h>
#include <stdbool.h>
#include <inttypes.h>
#include <tegrabl_malloc.h>
#include <tegrabl_sdram_usage.h>
#include <tegrabl_soc_misc.h>
#include <tegrabl_bootimg.h>
#include <tegrabl_linuxboot_helper.h>
#include <tegrabl_exit.h>

#ifdef CONFIG_ENABLE_A_B_SLOT
#include <tegrabl_a_b_boot_control.h>
#endif

/* boot.img signature size for verify_boot */
#define BOOT_IMG_SIG_SIZE (4 * 1024)

tegrabl_error_t tegrabl_get_partition_name(tegrabl_binary_type_t bin_type,
						tegrabl_binary_copy_t binary_copy,
						char *partition_name)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	static char partition_names[TEGRABL_BINARY_MAX]
								[TEGRABL_GPT_MAX_PARTITION_NAME + 1] = {
		[TEGRABL_BINARY_KERNEL] = {"kernel"},
		[TEGRABL_BINARY_KERNEL_DTB] = {"kernel-dtb"},
		[TEGRABL_BINARY_KERNEL_DTBO] = {"kernel-dtbo"},
		[TEGRABL_BINARY_RECOVERY_KERNEL] = {"SOS"},
		[TEGRABL_BINARY_NCT] = {"NCT"},
#if defined(CONFIG_ENABLE_L4T_RECOVERY)
		[TEGRABL_BINARY_RECOVERY_IMG] = {"recovery"},
		[TEGRABL_BINARY_RECOVERY_DTB] = {"recovery-dtb"},
		[TEGRABL_BINARY_KERNEL_BOOTCTRL] = {"kernel-bootctrl"}
#endif
	};

	TEGRABL_ASSERT(strlen(partition_names[bin_type]) <=
				(TEGRABL_GPT_MAX_PARTITION_NAME - 2));
	if (strlen(partition_names[bin_type]) == 0) {
		err = TEGRABL_ERR_NOT_FOUND;
		goto exit;
	}

	strcpy(partition_name, partition_names[bin_type]);

#if defined(CONFIG_ENABLE_A_B_SLOT)
	/*
	 * Note: Needs to map from binary_copy to boot slot number
	 *       once they are no longer identical matching
	 */
	tegrabl_a_b_set_bootslot_suffix(binary_copy, partition_name, false);

#else
	if (binary_copy == TEGRABL_BINARY_COPY_RECOVERY) {
		strcat(partition_name, "-r");
	}

#endif
exit:
	return err;
}

#if defined(CONFIG_ENABLE_A_B_SLOT)
static tegrabl_error_t a_b_get_bin_copy(tegrabl_binary_type_t bin_type,
		tegrabl_binary_copy_t *binary_copy)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t slot;
	struct slot_meta_data *smd = NULL;

	/* Do A/B selection for bin_type that have a/b slots */
	switch (bin_type) {
	case TEGRABL_BINARY_KERNEL:
	case TEGRABL_BINARY_KERNEL_DTB:
	case TEGRABL_BINARY_KERNEL_DTBO:
		/* TODO: add a bin_type that supports a/b */
		break;

	default:
		/* Choose _A for bin_type that have only one slot */
		*binary_copy = TEGRABL_BINARY_COPY_PRIMARY;
		slot = BOOT_SLOT_A;
		goto done;
	}

	err = tegrabl_a_b_get_active_slot(NULL, &slot);
	if (err != TEGRABL_NO_ERROR) {
		if (TEGRABL_ERROR_REASON(err) == TEGRABL_ERR_NOT_FOUND) {
			/*
			 * No slot number has been set by MB1
			 * Device is handled as non A/B system
			 */
			err = TEGRABL_NO_ERROR;
		} else {
			pr_error("Select a/b slot failed\n");
			err = tegrabl_err_set_highest_module(TEGRABL_ERR_LOADER, 1);
			goto done;
		}
	}

	*binary_copy = (tegrabl_binary_copy_t)slot;
	if (slot == BOOT_SLOT_A) {
		goto done;
	}

	/*
	 * In case redundancy is supported, there are two possible modes:
	 *   1. BL only, 2. BL + Kernel, file system and user partitions.
	 *
	 * In case of BL only, all partitions beyond BL wll be loaded from SLOT_A.
	 *
	 * The conditons that support BL redundancy only is:
	 *
	 * a. REDUNDANCY mode enabled
	 * b. REDUNDANCY_USER partitions disabled
	 *
	 */
	err = tegrabl_a_b_get_smd((void *)&smd);
	if ((err != TEGRABL_NO_ERROR) || (smd == NULL)) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	if ((BOOTCTRL_SUPPORT_REDUNDANCY(tegrabl_a_b_get_version(smd)) != 0U) &&
		(BOOTCTRL_SUPPORT_REDUNDANCY_USER(tegrabl_a_b_get_version(smd)) ==
			0U)) {
		*binary_copy = TEGRABL_BINARY_COPY_PRIMARY;
		slot = BOOT_SLOT_A;
		goto done;
	}

done:
	pr_info("A/B: bin_type (%d) slot %d\n", (int)bin_type, (int)slot);
	return err;
}
#endif

static tegrabl_error_t tegrabl_get_binary_info(
		tegrabl_binary_type_t bin_type, struct tegrabl_binary_info *binary,
		tegrabl_binary_copy_t binary_copy)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	TEGRABL_ASSERT(binary);
	TEGRABL_ASSERT(binary_copy < TEGRABL_BINARY_COPY_MAX);
	TEGRABL_ASSERT(bin_type < TEGRABL_BINARY_MAX);

	switch (bin_type) {
	case TEGRABL_BINARY_KERNEL:
	case TEGRABL_BINARY_RECOVERY_KERNEL:
		err = tegrabl_get_boot_img_load_addr(&binary->load_address);
		break;

	case TEGRABL_BINARY_KERNEL_DTB:
#if defined(CONFIG_ENABLE_L4T_RECOVERY)
	case TEGRABL_BINARY_RECOVERY_DTB:
#endif
		binary->load_address = (void *)tegrabl_get_dtb_load_addr();
		break;

	case TEGRABL_BINARY_NCT:
		err = tegrabl_get_nct_load_addr(&binary->load_address);
		break;

#if defined(CONFIG_ENABLE_L4T_RECOVERY)
	case TEGRABL_BINARY_RECOVERY_IMG:
	case TEGRABL_BINARY_KERNEL_BOOTCTRL:
		err = tegrabl_get_recovery_img_load_addr(&binary->load_address);
		break;
#endif

	default:
		pr_error("Invalid bin type\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		break;
	}

	/* Get partition name */
	tegrabl_get_partition_name(bin_type, binary_copy, binary->partition_name);

	return err;
}

static tegrabl_error_t read_kernel_partition(
	struct tegrabl_partition *partition, void *load_address,
	uint64_t *partition_size)
{
	tegrabl_error_t err;
	uint32_t remain_size;
	union tegrabl_bootimg_header *hdr;
	uint32_t device_type;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	/* read head pages equal to android kernel header size */
	err = tegrabl_partition_read(partition, load_address, ANDROID_HEADER_SIZE);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error reading kernel partition header pages\n");
		TEGRABL_SET_HIGHEST_MODULE(err);
		return err;
	}

	hdr = (union tegrabl_bootimg_header *)load_address;
	if (!strncmp((char *)hdr->magic, ANDROID_MAGIC, ANDROID_MAGIC_SIZE)) {
		/* for android kernel, read remaining kernel size */
		/* align kernel/ramdisk/secondimage/signature size with page size */
		remain_size = ALIGN(hdr->kernelsize, hdr->pagesize);
		remain_size += ALIGN(hdr->ramdisksize, hdr->pagesize);
		remain_size += ALIGN(hdr->secondsize, hdr->pagesize);
		remain_size += ALIGN(BOOT_IMG_SIG_SIZE, hdr->pagesize);
		pr_trace("%u: kernel partition: read size (excluding header): 0x%08x\n", __LINE__, remain_size);

		if (remain_size + ANDROID_HEADER_SIZE > *partition_size) {
			/*
			 * as kernel/ramdisk/secondimage/signature is aligned with kernel
			 * page size, so kernel may occupy at most 4*page_size more bytes
			 * than boot.img size
			 */
			pr_error("kernel partition size should be at least %dB larger than \
					 kernel size\n", 4 * hdr->pagesize);
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			return err;
		}
	} else {
		/* for other kernels, read the rest partition */
		remain_size = *partition_size - ANDROID_HEADER_SIZE;
		pr_trace("%u: kernel partition: read size (excluding header): 0x%08x\n", __LINE__, remain_size);
	}

	/* read the remaining pages */
	device_type = BITFIELD_GET(partition->block_device->device_id, 16, 16);
	if (device_type == TEGRABL_STORAGE_USB_MS) {
		/* TODO: WAR for reading kernel image from usb stick */
		partition->offset = 0;
		err = tegrabl_partition_read(partition,
									(char *)load_address,
									remain_size + ANDROID_HEADER_SIZE);
	} else {
		err = tegrabl_partition_read(partition,
									(char *)load_address + ANDROID_HEADER_SIZE,
									remain_size);
	}

	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error reading kernel partition remaining pages\n");
		TEGRABL_SET_HIGHEST_MODULE(err);
		return err;
	}

	*partition_size = remain_size + ANDROID_HEADER_SIZE;
	return err;
}

#define AUX_INFO_LOAD_BINARY_BIN_TYPE_ERR	100
#define AUX_INFO_LOAD_BINARY_BDEV_ERR		101
#define AUX_INFO_INVALID_PARTITION_SIZE		102
#define AUX_INFO_OVERFLOW_ERR				103

tegrabl_error_t tegrabl_load_binary_bdev(tegrabl_binary_type_t bin_type, void **load_address,
										 uint32_t *binary_length,  tegrabl_bdev_t *bdev)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_partition partition;
	uint64_t partition_size = 0;
	struct tegrabl_binary_info binary = {0};
	char partition_name[TEGRABL_GPT_MAX_PARTITION_NAME + 1];
	bool load_addr_predefined = true;

	if (bin_type >= TEGRABL_BINARY_MAX) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_LOAD_BINARY_BIN_TYPE_ERR);
		goto done;
	}

	if (bdev == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_LOAD_BINARY_BDEV_ERR);
		goto done;
	}

	binary.partition_name = partition_name;
	err = tegrabl_get_binary_info(bin_type, &binary, TEGRABL_BINARY_COPY_PRIMARY);
	if ((err != TEGRABL_NO_ERROR) && (TEGRABL_ERROR_REASON(err) != TEGRABL_ERR_NOT_FOUND)) {
		pr_error("Cannot get binary info %s\n", binary.partition_name);
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	/* if partition has no fixed predefined load address, callers should
	 * provide valid load address and available load size in advance
	 */
	if (TEGRABL_ERROR_REASON(err) == TEGRABL_ERR_NOT_FOUND) {
		if (*load_address) {
			load_addr_predefined = false;
			binary.load_address = *load_address;
		} else {
			goto done;
		}
	}

	/* Get partition info */
	err = tegrabl_partition_lookup_bdev(binary.partition_name, &partition, bdev);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Cannot open partition %s\n", binary.partition_name);
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	pr_info("Loading partition %s at %p from device(0x%x)\n", binary.partition_name, binary.load_address,
			tegrabl_blockdev_get_storage_type(partition.block_device));

	/* Get partition size */
	partition_size = tegrabl_partition_size(&partition);
	pr_debug("Size of partition: %"PRIu64"\n", partition_size);
	if (!partition_size) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_PARTITION_SIZE);
		goto done;
	}
	/* Check if dynamic load address may overflow */
	if (!load_addr_predefined && binary_length && (partition_size < *binary_length)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, AUX_INFO_OVERFLOW_ERR);
		goto done;
	}

	/* Read the partition from storage */
	if (bin_type == TEGRABL_BINARY_KERNEL) {
		err = read_kernel_partition(&partition, binary.load_address,
									&partition_size);
	} else {
		err = tegrabl_partition_read(&partition, binary.load_address,
									 partition_size);
	}

	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error reading partition %s\n", binary.partition_name);
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	/* Return load address and size */
	if (load_address) {
		*load_address = (void *)binary.load_address;
	}

	if (binary_length) {
		*binary_length = partition_size;
	}

done:
	return err;
}

tegrabl_error_t tegrabl_load_binary_copy(
	tegrabl_binary_type_t bin_type, void **load_address,
	uint32_t *binary_length, tegrabl_binary_copy_t binary_copy)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_partition partition;
	uint64_t partition_size = 0;
	struct tegrabl_binary_info binary = {0};
	char partition_name[TEGRABL_GPT_MAX_PARTITION_NAME + 1];
	bool load_addr_predefined = true;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	if (bin_type >= TEGRABL_BINARY_MAX) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto done;
	}

	binary.partition_name = partition_name;
	err = tegrabl_get_binary_info(bin_type, &binary, binary_copy);
	if (err != TEGRABL_NO_ERROR &&
		TEGRABL_ERROR_REASON(err) != TEGRABL_ERR_NOT_FOUND) {
		pr_error("Cannot get binary info %s\n", binary.partition_name);
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	pr_info("Loading %s from partition\n", binary.partition_name);

	/* if partition has no fixed predefined load address, callers should
	 * provide valid load address and available load size in advance
	 */
	if (TEGRABL_ERROR_REASON(err) == TEGRABL_ERR_NOT_FOUND) {
		if (*load_address) {
			load_addr_predefined = false;
			binary.load_address = *load_address;
		} else {
			pr_error("Address is not provided to load binary\n");
			goto done;
		}
	}

	/* Get partition info */
	err = tegrabl_partition_open(binary.partition_name, &partition);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Cannot open partition %s\n", binary.partition_name);
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	pr_info("Loading partition %s at %p from device(0x%x)\n", binary.partition_name, binary.load_address,
			tegrabl_blockdev_get_storage_type(partition.block_device));

	/* Get partition size */
	partition_size = tegrabl_partition_size(&partition);
	pr_debug("Size of partition: %"PRIu64"\n", partition_size);
	if (!partition_size) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto done;
	}

	/* Check if dynamic load address may overflow */
	if (!load_addr_predefined) {
		if ((binary_length != NULL) && (partition_size < *binary_length)) {
			err = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);
			goto done;
		}
	}

	/* Read the partition from storage */
#if defined(CONFIG_ENABLE_L4T_RECOVERY)
	if (bin_type == TEGRABL_BINARY_KERNEL
		|| bin_type == TEGRABL_BINARY_RECOVERY_IMG)
#else
	if (bin_type == TEGRABL_BINARY_KERNEL)
#endif
		err = read_kernel_partition(&partition, binary.load_address,
									&partition_size);
	else
		err = tegrabl_partition_read(&partition, binary.load_address,
									 partition_size);

	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error reading partition %s\n", binary.partition_name);
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	/* Return load address and size */
	if (load_address) {
		*load_address = (void *)binary.load_address;
	}

	if (binary_length) {
		*binary_length = partition_size;
	}

done:
	return err;
}

tegrabl_error_t tegrabl_load_binary(
		tegrabl_binary_type_t bin_type, void **load_address,
		uint32_t *binary_length)
{
#if defined(CONFIG_ENABLE_A_B_SLOT)
	tegrabl_error_t err;
	tegrabl_binary_copy_t bin_copy = TEGRABL_BINARY_COPY_PRIMARY;

	/* Do A/B selection and set bin_copy accordingly */
	err = a_b_get_bin_copy(bin_type, &bin_copy);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("A/B select failed\n");
		goto done;
	}

	err = tegrabl_load_binary_copy(bin_type, load_address, binary_length,
			bin_copy);

	if (err == TEGRABL_NO_ERROR) {
		goto done;
	}

	/*
	 * TODO: Add error handling such as fallover to other good slot or
	 * enter fastboot if no good slot is found
	 */
	pr_error("A/B loader failure\n");
	TEGRABL_ERROR_PRINT(err);
	pr_debug("Trigger soc reset\n");
	tegrabl_reset();
#else	/* !defined(CONFIG_ENABLE_A_B_SLOT) */

	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_load_binary_copy(bin_type, load_address, binary_length,
		TEGRABL_BINARY_COPY_PRIMARY);
	if (err == TEGRABL_NO_ERROR) {
		goto done;
	}

	err = tegrabl_load_binary_copy(bin_type, load_address, binary_length,
		TEGRABL_BINARY_COPY_RECOVERY);
#endif	/* CONFIG_ENABLE_A_B_SLOT */

done:
	return err;
}
