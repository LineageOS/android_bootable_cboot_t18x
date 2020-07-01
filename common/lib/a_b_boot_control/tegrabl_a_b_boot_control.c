/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_AB_BOOTCTRL

#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_utils.h>
#include <stdbool.h>
#include <inttypes.h>
#include <tegrabl_a_b_boot_control.h>
#include <tegrabl_soc_misc.h>
#include <tegrabl_malloc.h>
#include <tegrabl_partition_manager.h>

enum smd_bin_copy {
	SMD_COPY_PRIMARY = 0U,
	SMD_COPY_SECONDARY,
	MAX_SMD_COPY,
};

static void *smd_loadaddress;

#if defined(CONFIG_ENABLE_DEBUG)
static void boot_chain_dump_slot_info(void *load_address)
{
	int i = 0;
	struct slot_meta_data *smd_info = (struct slot_meta_data *)load_address;

	pr_info("magic:0x%x, version: %d, num_slots: %d\n",
			smd_info->magic,
			smd_info->version,
			smd_info->num_slots);

	for (i = 0; i < MAX_SLOTS; i++) {
		pr_info("slot: %d, pri: %d, suffix: %c%c, retry: %d, boot_succ: %d\n",
				i, smd_info->slot_info[i].priority,
				smd_info->slot_info[i].suffix[0],
				smd_info->slot_info[i].suffix[1],
				smd_info->slot_info[i].retry_count,
				smd_info->slot_info[i].boot_successful);
	}
}
#endif

uint16_t tegrabl_a_b_get_version(void *smd)
{
	struct slot_meta_data *smd_info = (struct slot_meta_data *)smd;

	TEGRABL_ASSERT(smd != NULL);

	return smd_info->version;
}

void tegrabl_a_b_init(void *smd)
{
	struct slot_meta_data *smd_info = (struct slot_meta_data *)smd;

	TEGRABL_ASSERT(smd);

	/* Set default: A: bootable, B: unbootable */
	smd_info->slot_info[0].priority = 15;
	smd_info->slot_info[0].retry_count = 7;
	smd_info->slot_info[0].boot_successful = 1;
	(void) memcpy(smd_info->slot_info[0].suffix, BOOT_CHAIN_SUFFIX_A,
			BOOT_CHAIN_SUFFIX_LEN);

	smd_info->slot_info[1].priority = 0;
	smd_info->slot_info[1].retry_count = 0;
	smd_info->slot_info[1].boot_successful = 0;
	(void) memcpy(smd_info->slot_info[1].suffix, BOOT_CHAIN_SUFFIX_B,
			BOOT_CHAIN_SUFFIX_LEN);

	smd_info->magic = BOOT_CHAIN_MAGIC;
	smd_info->version = BOOT_CHAIN_VERSION;

	/*
	 * Simulate SMD but set max_slots to 1 so that device is handled as
	 * non-A/B system.
	 */
	smd_info->num_slots = 1;
}

static inline uint16_t tegrabl_a_b_get_max_num_slots(void *smd)
{
	struct slot_meta_data *smd_info = (struct slot_meta_data *)smd;

	TEGRABL_ASSERT(smd);

	return smd_info->num_slots;
}

tegrabl_error_t tegrabl_a_b_init_boot_slot_reg(void *smd)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t reg;
	uint32_t slot;

	reg = tegrabl_get_boot_slot_reg();

	/* In case not power on reset, scratch register may have been already
	 * initialized in previous boot, hence skip */
	if (BOOT_CHAIN_REG_MAGIC_GET(reg) == BOOT_CHAIN_REG_MAGIC) {
		goto done;
	}

	/* If reg magic is invalid, init reg with SMD and retain update flag */
	reg = 0;
	/* Set reg magic */
	reg = BOOT_CHAIN_REG_MAGIC_SET(reg);
	tegrabl_set_boot_slot_reg(reg);

	err = tegrabl_a_b_get_active_slot(smd, &slot);
	if (err != TEGRABL_NO_ERROR) {
		goto done;
	}

	tegrabl_a_b_save_boot_slot_reg(smd, slot);

done:
	return err;
}

static bool tegrabl_a_b_is_valid(void *smd)
{
	uint32_t reg;
	struct slot_meta_data *smd_info;

	if (!smd) {
		/* check info stored in scratch register */
		reg = tegrabl_get_boot_slot_reg();

		return (bool)
				((BOOT_CHAIN_REG_MAGIC_GET(reg) == BOOT_CHAIN_REG_MAGIC) &&
				 (BOOT_CHAIN_REG_MAX_SLOTS_GET(reg) > 1));
	}

	/* check info from smd buffer */
	smd_info = (struct slot_meta_data *)smd;
	return (bool)((smd_info->magic == BOOT_CHAIN_MAGIC) &&
				  (smd_info->num_slots > 1));
}

static tegrabl_error_t boot_chain_get_bootslot_from_reg(uint32_t *active_slot)
{
	tegrabl_error_t err;
	uint32_t reg;

	reg = tegrabl_get_boot_slot_reg();

	if (!tegrabl_a_b_is_valid(NULL)) {
		pr_warn("No valid slot number is found in scratch register\n");
		pr_warn("Return default slot: %s\n", BOOT_CHAIN_SUFFIX_A);
		*active_slot = BOOT_SLOT_A;
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto done;
	}

	*active_slot = BOOT_CHAIN_REG_SLOT_NUM_GET(reg);
	err = TEGRABL_NO_ERROR;

done:
	return err;
}

static tegrabl_error_t boot_chain_get_bootslot_from_smd(void *smd,
							uint32_t *active_slot)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct slot_meta_data *bootctrl = (struct slot_meta_data *)smd;
	uint8_t max_priority;
	int i, slot;

	/* Check if the data is correct */
	if (bootctrl->magic != BOOT_CHAIN_MAGIC) {
		pr_error("SMD is corrupted!\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto done;
	}

	/* Init priority as unbootable */
	max_priority = 0;

	/* Find slot with retry_count > 0 and highest priority */
	for (i = 0; i < bootctrl->num_slots; i++) {
		if (bootctrl->slot_info[i].retry_count &&
			bootctrl->slot_info[i].priority) {
			if (max_priority < bootctrl->slot_info[i].priority) {
				max_priority = bootctrl->slot_info[i].priority;
				slot = i;
			}
		}
	}

	/* Found a bootable slot? */
	if (!max_priority) {
		pr_error("ERROR: No bootable slots found\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto done;
	}

	*active_slot = (uint32_t)slot;
	pr_info("Boot chain chosen: %d\n", *active_slot);

	err = TEGRABL_NO_ERROR;

done:
	return err;
}

tegrabl_error_t tegrabl_a_b_get_active_slot(void *smd, uint32_t *active_slot)
{
	tegrabl_error_t err;

	/* Use slot number previous saved in scratch register */
	if (!smd) {
		err = boot_chain_get_bootslot_from_reg(active_slot);
		goto done;
	}

	/* Run a/b slot selection logic to find out active slot */
	err = boot_chain_get_bootslot_from_smd(smd, active_slot);

done:
	return err;
}

void tegrabl_a_b_set_retry_count_reg(uint32_t slot, uint8_t retry_count)
{
	uint32_t reg;

	reg = tegrabl_get_boot_slot_reg();
	switch (slot) {
	case BOOT_SLOT_A:
		reg = BOOT_CHAIN_REG_A_RETRY_COUNT_SET(retry_count, reg);
		break;
	case BOOT_SLOT_B:
		reg = BOOT_CHAIN_REG_B_RETRY_COUNT_SET(retry_count, reg);
		break;
	default:
		break;
	}
	tegrabl_set_boot_slot_reg(reg);
}

static uint8_t tegrabl_a_b_get_retry_count_reg(uint32_t slot, uint32_t reg)
{
	uint8_t retry_count = 0;

	switch (slot) {
	case BOOT_SLOT_A:
		retry_count = BOOT_CHAIN_REG_A_RETRY_COUNT_GET(reg);
		break;
	case BOOT_SLOT_B:
		retry_count = BOOT_CHAIN_REG_B_RETRY_COUNT_GET(reg);
		break;
	default:
		TEGRABL_ASSERT(0);
		break;
	}
	return retry_count;
}

void tegrabl_a_b_copy_retry_count(void *smd, uint32_t *reg, uint32_t direct)
{
	struct slot_meta_data *bootctrl = (struct slot_meta_data *)smd;
	uint8_t retry_count;

	TEGRABL_ASSERT(smd != NULL);

	switch (direct) {
	case FROM_REG_TO_SMD:
		retry_count = BOOT_CHAIN_REG_A_RETRY_COUNT_GET(*reg);
		bootctrl->slot_info[BOOT_SLOT_A].retry_count = retry_count;
		retry_count = BOOT_CHAIN_REG_B_RETRY_COUNT_GET(*reg);
		bootctrl->slot_info[BOOT_SLOT_B].retry_count = retry_count;
		break;
	case FROM_SMD_TO_REG:
		retry_count = bootctrl->slot_info[BOOT_SLOT_A].retry_count;
		*reg = BOOT_CHAIN_REG_A_RETRY_COUNT_SET(retry_count, *reg);
		retry_count = bootctrl->slot_info[BOOT_SLOT_B].retry_count;
		*reg = BOOT_CHAIN_REG_B_RETRY_COUNT_SET(retry_count, *reg);
		break;
	default:
		break;
	}
}

void tegrabl_a_b_save_boot_slot_reg(void *smd, uint32_t slot)
{
	uint32_t reg;
	struct slot_meta_data *bootctrl = (struct slot_meta_data *)smd;

	TEGRABL_ASSERT(smd != NULL);

	reg = tegrabl_get_boot_slot_reg();

	/* Set slot number */
	reg = BOOT_CHAIN_REG_SLOT_NUM_SET(slot, reg);

	/* Set max slots */
	reg = BOOT_CHAIN_REG_MAX_SLOTS_SET(tegrabl_a_b_get_max_num_slots(smd), reg);

	/* Set retry counts */
	tegrabl_a_b_copy_retry_count(smd, &reg, FROM_SMD_TO_REG);

	/* Set update flag if current boot slot's boot_succ flag is 0 */
	if (bootctrl->slot_info[slot].boot_successful == 0U) {
		reg = BOOT_CHAIN_REG_UPDATE_FLAG_SET(BC_FLAG_OTA_ON, reg);
	} else {
		/* check SMD version and set REDUNDANCY flag if it is supported */
		if (BOOTCTRL_SUPPORT_REDUNDANCY(bootctrl->version) != 0U) {
			reg = BOOT_CHAIN_REG_UPDATE_FLAG_SET(BC_FLAG_REDUNDANCY_BOOT, reg);
		}
	}

	tegrabl_set_boot_slot_reg(reg);
}

tegrabl_error_t tegrabl_a_b_check_and_update_retry_count(void *smd,
														 uint32_t slot)
{
	struct slot_meta_data *bootctrl = (struct slot_meta_data *)smd;

	TEGRABL_ASSERT(smd != NULL);

	/*
	 * Decrement retry count if
	 * a. REDUNDNACY is supported, or
	 * b. Current slot state is unsuccessful
	 */

	if ((BOOTCTRL_SUPPORT_REDUNDANCY(bootctrl->version) != 0U) ||
		(bootctrl->slot_info[slot].boot_successful == 0U)) {
		TEGRABL_ASSERT(bootctrl->slot_info[slot].retry_count);
		bootctrl->slot_info[slot].retry_count--;
	}

#if defined(CONFIG_ENABLE_DEBUG)
	boot_chain_dump_slot_info(bootctrl);
#endif

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_a_b_get_bootslot_suffix(char *suffix, bool full_suffix)
{
	tegrabl_error_t err;
	uint32_t slot;

	err = tegrabl_a_b_get_active_slot(NULL, &slot);
	if (err != TEGRABL_NO_ERROR) {
		if (TEGRABL_ERROR_REASON(err) == TEGRABL_ERR_NOT_FOUND) {
			err = TEGRABL_NO_ERROR;
		} else {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}
	}

	if ((full_suffix == false) &&  (slot == BOOT_SLOT_A)) {
		*suffix = 0x0;
		goto done;
	}

	if (slot == BOOT_SLOT_A) {
		strncpy(suffix, BOOT_CHAIN_SUFFIX_A, BOOT_CHAIN_SUFFIX_LEN);
	} else {
		strncpy(suffix, BOOT_CHAIN_SUFFIX_B, BOOT_CHAIN_SUFFIX_LEN);
	}
	*(suffix + BOOT_CHAIN_SUFFIX_LEN) = 0x0;

done:
	pr_info("Active slot suffix: %s\n", suffix);

fail:
	return err;
}

tegrabl_error_t tegrabl_a_b_set_bootslot_suffix(uint32_t slot, char *partition,
												bool full_suffix)
{
	char *suffix;

	/*
	 * To be compatible with legacy partition names, set slot A's suffix to
	 * none, ie, no suffix for slot A, if full_suffix is not needed
	 */
	if ((full_suffix == false) && (slot == BOOT_SLOT_A)) {
		goto done;
	}

	if (slot == BOOT_SLOT_A) {
		suffix = BOOT_CHAIN_SUFFIX_A;
	} else {
		suffix = BOOT_CHAIN_SUFFIX_B;
	}
	strcat(partition, suffix);

done:
	pr_debug("Select partition: %s\n", partition);
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_a_b_get_slot_via_suffix(const char *suffix,
												uint32_t *boot_slot_id)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (*suffix == 0x00 || !strcmp(suffix, BOOT_CHAIN_SUFFIX_A)) {
		*boot_slot_id = (uint32_t)BOOT_SLOT_A;
	} else if (!strcmp(suffix, BOOT_CHAIN_SUFFIX_B)) {
		*boot_slot_id = (uint32_t)BOOT_SLOT_B;
	} else {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	return error;
}

tegrabl_error_t tegrabl_a_b_get_slot_num(void *smd_addr, uint8_t *num_slots)
{
	struct slot_meta_data *smd;

	if ((smd_addr == NULL) || (num_slots == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd = (struct slot_meta_data *)smd_addr;
	*num_slots = (uint8_t)smd->num_slots;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_a_b_get_successful(void *smd_addr, uint32_t slot,
										   uint8_t *successful)
{
	struct slot_meta_data *smd;

	if ((smd_addr == NULL) || (slot >= MAX_SLOTS) || (successful == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd = (struct slot_meta_data *)smd_addr;
	*successful = smd->slot_info[slot].boot_successful;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_a_b_get_retry_count(void *smd_addr, uint32_t slot,
											uint8_t *retry_count)
{
	struct slot_meta_data *smd;

	if ((smd_addr == NULL) || (slot >= MAX_SLOTS) || (retry_count == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd = (struct slot_meta_data *)smd_addr;
	*retry_count = smd->slot_info[slot].retry_count;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_a_b_get_priority(void *smd_addr, uint32_t slot,
										 uint8_t *priority)
{
	struct slot_meta_data *smd;

	if ((smd_addr == NULL) || (slot >= MAX_SLOTS) || (priority == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd = (struct slot_meta_data *)smd_addr;
	*priority = smd->slot_info[slot].priority;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_a_b_is_unbootable(void *smd_addr, uint32_t slot,
										  bool *is_unbootable)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct slot_meta_data *smd;
	uint8_t priority, retry_count;

	if ((smd_addr == NULL) || (slot >= MAX_SLOTS) || (is_unbootable == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto done;
	}

	smd = (struct slot_meta_data *)smd_addr;

	error = tegrabl_a_b_get_priority((void *)smd, slot, &priority);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}

	error = tegrabl_a_b_get_retry_count((void *)smd, slot, &retry_count);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}

	*is_unbootable = (priority == 0 || retry_count == 0) ? true : false;

done:
	return error;
}

tegrabl_error_t tegrabl_a_b_set_successful(void *smd_addr, uint32_t slot,
										   uint8_t successful)
{
	struct slot_meta_data *smd;

	if ((smd_addr == NULL) || (slot >= MAX_SLOTS)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd = (struct slot_meta_data *)smd_addr;
	smd->slot_info[slot].boot_successful = successful;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_a_b_set_retry_count(void *smd_addr, uint32_t slot,
											uint8_t retry_count)
{
	struct slot_meta_data *smd;

	if ((smd_addr == NULL) || (slot >= MAX_SLOTS)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd = (struct slot_meta_data *)smd_addr;
	smd->slot_info[slot].retry_count = retry_count;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_a_b_set_priority(void *smd_addr, uint32_t slot,
										 uint8_t priority)
{
	struct slot_meta_data *smd;

	if ((smd_addr == NULL) || (slot >= MAX_SLOTS)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd = (struct slot_meta_data *)smd_addr;
	smd->slot_info[slot].priority = priority;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_a_b_set_active_slot(void *smd_addr, uint32_t slot_id)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct slot_meta_data *smd;
	uint32_t i;

	if ((smd_addr == NULL) || (slot_id >= MAX_SLOTS)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto done;
	}

	smd = (struct slot_meta_data *)smd_addr;

	/* Decrease all slot's priority by 1 */
	for (i = 0; i < MAX_SLOTS; i++) {
		if (smd->slot_info[i].priority > 1) {
			smd->slot_info[i].priority -= 1;
		}
	}

	/* Reset slot info of given slot to default */
	error = tegrabl_a_b_set_priority((void *)smd, slot_id,
									 SLOT_PRIORITY_DEFAULT);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}
	error = tegrabl_a_b_set_retry_count((void *)smd, slot_id,
										SLOT_RETRY_COUNT_DEFAULT);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}

	error = tegrabl_a_b_set_successful((void *)smd, slot_id, 0);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}
done:
	return error;
}

static tegrabl_error_t load_smd_bin_copy(enum smd_bin_copy bin_copy)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct slot_meta_data *smd = NULL;
	struct tegrabl_partition part;
	char *smd_part;
	uint32_t crc32;
	uint32_t smd_len = sizeof(struct slot_meta_data);

	smd_part = (bin_copy == SMD_COPY_PRIMARY) ? "SMD" : "SMD_b";
	error = tegrabl_partition_open(smd_part, &part);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}

	/* Always move to first byte */
	error = tegrabl_partition_seek(&part, 0, TEGRABL_PARTITION_SEEK_SET);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}

	error = tegrabl_partition_read(&part, smd_loadaddress, smd_len);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}
	tegrabl_partition_close(&part);

	smd = (struct slot_meta_data *)smd_loadaddress;

	if ((tegrabl_a_b_get_version(smd) & BOOT_CHAIN_VERSION_MASK) >=
				BOOT_CHAIN_VERSION_CRC32) {
		/* Check crc for SMD sanity */
		crc32 = tegrabl_utils_crc32(0, smd_loadaddress,
									smd_len - sizeof(crc32));
		if (crc32 != smd->crc32) {
			error = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 0);
			pr_error("%s corrupt with incorrect crc\n", smd_part);
			goto done;
		}
	}

done:
	return error;
}

tegrabl_error_t tegrabl_a_b_get_smd(void **smd)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t smd_len;

	TEGRABL_ASSERT(smd);

	/* Return smd address directly if it has already been loaded */
	if (smd_loadaddress != NULL) {
		*smd = smd_loadaddress;
		goto done;
	}

	smd_len = sizeof(struct slot_meta_data);
	smd_loadaddress = tegrabl_malloc(smd_len);
	if (smd_loadaddress == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto done;
	}

	/* Load and verify SMD primary copy */
	error = load_smd_bin_copy(SMD_COPY_PRIMARY);
	if (error == TEGRABL_NO_ERROR) {
		*smd = smd_loadaddress;
		goto done;
	}

	/* If SMD primary copy has corrupted, fallback to SMD secondary copy */
	error = load_smd_bin_copy(SMD_COPY_SECONDARY);
	if (error == TEGRABL_NO_ERROR) {
		*smd = smd_loadaddress;
		goto done;
	}

	tegrabl_free(smd_loadaddress);
	smd_loadaddress = NULL;

done:
	return error;
}

static tegrabl_error_t flush_smd_bin_copy(void *smd, enum smd_bin_copy bin_copy)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_partition part;
	char *smd_part;

	smd_part = (bin_copy == SMD_COPY_PRIMARY) ? "SMD" : "SMD_b";

	/* Write SMD back to storage */
	error = tegrabl_partition_open(smd_part, &part);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}

#if defined(CONFIG_ENABLE_QSPI)
	uint32_t storage_type;
	/* Erase SMD since QSPI storage needs to be erased before writing */
	storage_type = tegrabl_blockdev_get_storage_type(part.block_device);
	if (storage_type == TEGRABL_STORAGE_QSPI_FLASH) {
		error = tegrabl_partition_erase(&part, false);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto done;
		}
	}
#endif

	/* Always move to first byte */
	error = tegrabl_partition_seek(&part, 0, TEGRABL_PARTITION_SEEK_SET);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}

	error = tegrabl_partition_write(&part, smd, sizeof(struct slot_meta_data));
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}

	tegrabl_partition_close(&part);
done:
	return error;
}

tegrabl_error_t tegrabl_a_b_flush_smd(void *smd)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct slot_meta_data *bootctrl = (struct slot_meta_data *)smd;
	uint32_t smd_payload_len;

	if (smd == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto done;
	}

	/* Update crc field before writing */
	smd_payload_len = sizeof(struct slot_meta_data) - sizeof(uint32_t);
	bootctrl->crc32 = tegrabl_utils_crc32(0, smd, smd_payload_len);

	/* Always flush both primary SMD and secondary SMD for backup, otherwise
	 * secondary copy will be stale and may lead to unknown behavior */
	error = flush_smd_bin_copy(smd, SMD_COPY_PRIMARY);

	error = flush_smd_bin_copy(smd, SMD_COPY_SECONDARY);

done:
	return error;
}

tegrabl_error_t tegrabl_a_b_update_smd(void)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct slot_meta_data *smd = NULL;
	uint32_t reg;
	uint8_t bc_flag;
	uint8_t retry_count;
	uint32_t smd_slot;
	uint32_t sr_slot;

	reg = tegrabl_get_boot_slot_reg();
	bc_flag = BOOT_CHAIN_REG_UPDATE_FLAG_GET(reg);
	if ((BOOT_CHAIN_REG_MAGIC_GET(reg) == BOOT_CHAIN_REG_MAGIC) &&
		((bc_flag == BC_FLAG_OTA_ON) || (bc_flag == BC_FLAG_REDUNDANCY_BOOT))) {
		error = tegrabl_a_b_get_smd((void **)&smd);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto done;
		}

		/*
		 * When control reaches here, BL can claim safe.
		 * For REDUNDANCY BL only, there is no need to update SMD except when
		 * first time booting from redundant slot after retrying reaches max
		 * count.
		 */
		if ((bc_flag == BC_FLAG_REDUNDANCY_BOOT) &&
		    (BOOTCTRL_SUPPORT_REDUNDANCY(tegrabl_a_b_get_version(smd)) != 0U) &&
			(BOOTCTRL_SUPPORT_REDUNDANCY_USER(tegrabl_a_b_get_version(smd))
				== 0U)) {

			/* Get initial retrying boot slot from SMD */
			error = tegrabl_a_b_get_active_slot(smd, &smd_slot);
			if (error != TEGRABL_NO_ERROR) {
				goto done;
			}

			/* Get actual boot slot from SR */
			sr_slot = BOOT_CHAIN_REG_SLOT_NUM_GET(reg);

			/* Skip updating SMD if there is no boot slot changing */
			if (sr_slot == smd_slot) {
				goto done;
			}

			/*
			 * Restore retry_count for REDUNDANCY BL only. For REDUNDANCY_USER
			 * it is UE responsibility to restore retry count if control can
			 * reach UE.
			 */
			retry_count = tegrabl_a_b_get_retry_count_reg(sr_slot, reg);
			++retry_count;
			tegrabl_a_b_set_retry_count_reg(sr_slot, retry_count);
			reg = tegrabl_get_boot_slot_reg();
		}

		/* Update SMD and flush to storage */
		tegrabl_a_b_copy_retry_count(smd, &reg, FROM_REG_TO_SMD);
		error = tegrabl_a_b_flush_smd(smd);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto done;
		}
	}

done:
	/* Clear SR before handing over to kernel */
	if (BOOT_CHAIN_REG_MAGIC_GET(reg) == BOOT_CHAIN_REG_MAGIC) {
		reg = 0;
		tegrabl_set_boot_slot_reg(reg);
	}
	return error;
}
