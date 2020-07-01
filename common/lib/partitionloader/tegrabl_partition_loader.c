/*
 * Copyright (c) 2015 - 2018, NVIDIA Corporation.  All Rights Reserved.
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
#include <tegrabl_gpt.h>
#include <stdbool.h>
#include <inttypes.h>
#include <tegrabl_malloc.h>
#include <tegrabl_sdram_usage.h>
#include <tegrabl_mb1_bct.h>
#include <tegrabl_soc_misc.h>
#include <tegrabl_a_b_boot_control.h>
#include <tegrabl_bootimg.h>
#include <tegrabl_auth.h>

/* boot.img signature size for verify_boot */
#define BOOT_IMG_SIG_SIZE (4 * 1024)

struct tegrabl_binary_info
	binary_info_table[TEGRABL_BINARY_MAX] = {
	[TEGRABL_BINARY_KERNEL] = {
			.load_address = (void *) BOOT_IMAGE_LOAD_ADDRESS
	},
	[TEGRABL_BINARY_KERNEL_DTB] = {
			.load_address = (void *) DTB_LOAD_ADDRESS
	},
	[TEGRABL_BINARY_RECOVERY_KERNEL] = {
			.load_address = (void *) BOOT_IMAGE_LOAD_ADDRESS
	},
	[TEGRABL_BINARY_NCT] = {
			.load_address = (void *)NCT_PART_LOAD_ADDRESS
	}
};

static char *tegrabl_get_partition_name(enum tegrabl_binary_type bin_type,
						enum tegrabl_binary_copy binary_copy,
						char *partition_name)
{
	static char partition_names[TEGRABL_BINARY_MAX]
								[TEGRABL_GPT_MAX_PARTITION_NAME + 1] = {
		[TEGRABL_BINARY_KERNEL] = {"kernel"},
		[TEGRABL_BINARY_KERNEL_DTB] = {"kernel-dtb"},
		[TEGRABL_BINARY_RECOVERY_KERNEL] = {"SOS"},
		[TEGRABL_BINARY_NCT] = {"NCT"}
	};

	TEGRABL_ASSERT(strlen(partition_names[bin_type]) <=
				(TEGRABL_GPT_MAX_PARTITION_NAME - 2));

	strcpy(partition_name, partition_names[bin_type]);

#if defined(CONFIG_ENABLE_A_B_SLOT)
	/*
	 * Note: Needs to map from binary_copy to boot slot number
	 *       once they are no longer identical matching
	 */
	tegrabl_a_b_set_bootslot_suffix(binary_copy, partition_name, false);

#else
	if (binary_copy == TEGRABL_BINARY_COPY_RECOVERY)
		strcat(partition_name, "-r");

#endif
	return partition_name;
}

#if defined(CONFIG_ENABLE_A_B_SLOT)
static tegrabl_error_t a_b_get_bin_copy(enum tegrabl_binary_type bin_type,
		enum tegrabl_binary_copy *binary_copy)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t slot;
	struct slot_meta_data *smd = NULL;

	/* Do A/B selection for bin_type that have a/b slots */
	switch (bin_type) {
	case TEGRABL_BINARY_KERNEL:
	case TEGRABL_BINARY_KERNEL_DTB:
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

	*binary_copy = (enum tegrabl_binary_copy)slot;
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
		enum tegrabl_binary_type bin_type, struct tegrabl_binary_info *binary,
		enum tegrabl_binary_copy binary_copy)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	TEGRABL_ASSERT(binary);
	TEGRABL_ASSERT(binary_copy < TEGRABL_BINARY_COPY_MAX);
	TEGRABL_ASSERT(bin_type < TEGRABL_BINARY_MAX);

	binary->load_address =
		binary_info_table[bin_type].load_address;

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
	}

	/* read the remaining pages */
	err = tegrabl_partition_read(partition,
								 (char *)load_address + ANDROID_HEADER_SIZE,
								 remain_size);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error reading kernel partition remaining pages\n");
		TEGRABL_SET_HIGHEST_MODULE(err);
		return err;
	}

	*partition_size = remain_size + ANDROID_HEADER_SIZE;
	return err;
}

#if defined(CONFIG_OS_IS_L4T)
#if defined(IS_T186)
static tegrabl_error_t tegrabl_auth_payload(enum tegrabl_binary_type bin_type,
			struct tegrabl_binary_info *binary, uint64_t partition_size)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_auth_handle auth = {0};

	pr_info("%s: partition %s (bin_type %u)\n", __func__,
				binary->partition_name, (uint32_t)bin_type);

	/* validate bin_type type */
	switch (bin_type) {
	case TEGRABL_BINARY_KERNEL:
	case TEGRABL_BINARY_KERNEL_DTB:
		break;
	default:
		pr_info("%s: Unsupported partition %s (bin_type %d)\n", __func__,
					binary->partition_name, (int)bin_type);
		goto fail;
	}

	/*
	 * TODO
	 * Add new bin_types into tegrabl_binary_types.h and convert this bin_type
	 * (defined in tegrabl_partition_loader.h) to corresponding new bin_type
	 */

	/* Initiate authentication of binary */
	err = tegrabl_auth_initiate((bin_type + 24), binary->load_address,
				partition_size, &auth);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	err = tegrabl_auth_process_block(&auth, binary->load_address,
			(uint32_t)partition_size, true);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	/* Verify signature/hash */
	err = tegrabl_auth_finalize(&auth);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

fail:
	/* End of authentication process */
	tegrabl_auth_end(&auth);

	return err;
}
#endif
#if defined(IS_T194)
	/* TODO: Add T194 support */
#endif
#endif

tegrabl_error_t tegrabl_load_binary_copy(
	enum tegrabl_binary_type bin_type, void **load_address,
	uint32_t *binary_length, enum tegrabl_binary_copy binary_copy)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_partition partition;
	uint64_t partition_size = 0;
	struct tegrabl_binary_info binary = {0};
	char partition_name[TEGRABL_GPT_MAX_PARTITION_NAME + 1];

	if (bin_type >= TEGRABL_BINARY_MAX) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto done;
	}

	binary.partition_name = partition_name;
	err = tegrabl_get_binary_info(bin_type, &binary, binary_copy);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Cannot get binary info %s\n", binary.partition_name);
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	pr_info("Loading partition %s at %p\n",
			binary.partition_name, binary.load_address);

	/* Set load address from retrieved info.*/
	if (load_address)
		*load_address = (void *)binary.load_address;

	/* Get partition info */
	err = tegrabl_partition_open(binary.partition_name,
			&partition);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Cannot open partition %s\n", binary.partition_name);
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	/* Get partition size */
	partition_size = tegrabl_partition_size(&partition);
	if (partition_size == 0) {
		err = tegrabl_error_value(TEGRABL_ERR_LOADER, 0, TEGRABL_ERR_INVALID);
		goto done;
	}
	pr_debug("Size of partition: %"PRIu64"\n", partition_size);

	/* Read the partition from storage */
	if (bin_type == TEGRABL_BINARY_KERNEL)
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
	if (binary_length)
		*binary_length = partition_size;

	/* Handle validation of the binaries */
#if defined(CONFIG_OS_IS_L4T)
#if defined(IS_T186)
	err = tegrabl_auth_payload(bin_type, &binary, partition_size);
#endif
#if defined(IS_T194)
	/* TODO: Add T194 support */
#endif
#endif
done:
	return err;
}

tegrabl_error_t tegrabl_load_binary(
		enum tegrabl_binary_type bin_type, void **load_address,
		uint32_t *binary_length)
{
#if defined(CONFIG_ENABLE_A_B_SLOT)
	tegrabl_error_t err;
	enum tegrabl_binary_copy bin_copy = TEGRABL_BINARY_COPY_PRIMARY;

	/* Do A/B selection and set bin_copy accordingly */
	err = a_b_get_bin_copy(bin_type, &bin_copy);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("A/B select failed\n");
		goto done;
	}

	err = tegrabl_load_binary_copy(bin_type, load_address, binary_length,
			bin_copy);

	if (err == TEGRABL_NO_ERROR)
		goto done;

	/*
	 * TODO: Add error handling such as fallover to other good slot or
	 * enter fastboot if no good slot is found
	 */
	TEGRABL_ERROR_PRINT(err);
	tegrabl_pmc_reset();
#else

	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_load_binary_copy(bin_type, load_address, binary_length,
		TEGRABL_BINARY_COPY_PRIMARY);
	if (err == TEGRABL_NO_ERROR)
		goto done;

	err = tegrabl_load_binary_copy(bin_type, load_address, binary_length,
		TEGRABL_BINARY_COPY_RECOVERY);
#endif

done:
	return err;
}
