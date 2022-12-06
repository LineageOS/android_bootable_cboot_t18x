/*
 * Copyright (c) 2017-2022, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_ROLLBACK_PREVENTION

#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_binary_types.h>
#include <tegrabl_rollback_prevention.h>
#include <tegrabl_prevent_rollback.h>
#include <tegrabl_fuse.h>
#include <tegrabl_rpb.h>
#include <tegrabl_auth.h>
#include <arscratch.h>
#include <tegrabl_addressmap.h>
#include <string.h>
#include <tegrabl_io.h>

#define RSA_KEY_SIZE 256
#define FUSE_ECID_SIZE 4

/* RBP magic message is "RPB0" */
const uint8_t rpb_magic[] = {0x52, 0x50, 0x42, 0x30};
const uint32_t rpb_version = 1;

#define SCRATCH_WRITE(reg, val)			\
	NV_WRITE32(NV_ADDRESS_MAP_SCRATCH_BASE + SCRATCH_##reg, val)

static struct tegrabl_rollback *rb_data;

uint8_t tegrabl_rollback_fusevalue_to_level(uint32_t fuse_value)
{
	uint8_t level = 0;
	while (fuse_value) {
		level += 1U;
		fuse_value = fuse_value >> 1U;
	}
	return level;
}

uint32_t tegrabl_rollback_level_to_fusevalue(uint8_t fuse_level)
{
	uint32_t fuse_value = 0;
	fuse_value = (1U << fuse_level) - 1U;
	return fuse_value;
}

tegrabl_error_t tegrabl_init_rollback_data(struct tegrabl_rollback *rb)
{
#ifdef CONFIG_ENFORCE_ROLLBACK_ENBALED
	uint32_t rollback_fuse_value;

	if (rb == NULL) {
		pr_error("rollback data is NULL, probably using older MB1\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	pr_debug("rb->version                %u\n", rb->version);
	pr_debug("rb->enabled                %u\n", rb->enabled);
	pr_debug("rb->fuse_idx               %u\n", rb->fuse_idx);
	pr_debug("rb->level                  %u\n", rb->level);
	pr_debug("rb->limits.boot            %u\n", rb->limits.boot);
	pr_debug("rb->limits.bpmp_fw         %u\n", rb->limits.bpmp_fw);
	pr_debug("rb->limits.tos             %u\n", rb->limits.tos);
	pr_debug("rb->limits.tsec            %u\n", rb->limits.tsec);
	pr_debug("rb->limits.nvdec           %u\n", rb->limits.nvdec);
	pr_debug("rb->limits.srm             %u\n", rb->limits.srm);
	pr_debug("rb->limits.tsec_gsc_ucode  %u\n", rb->limits.tsec_gsc_ucode);
	pr_debug("rb->limits.hdcp2srm        %u\n", rb->limits.hdcp2srm);
	pr_debug("rb->limits.cpubl           %u\n", rb->limits.cpubl);

	if (rb->version != 3) {
		pr_error("RB version is not 3\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (rb->enabled == 0) {
		if (!fuse_is_nv_production_mode()) {
			/* MB1 marks rb->enabled = 0 for internal devices,
			 * but make it 1 again so that TSEC firmware can run */
			pr_info("Making rb->enabled=1 for internal devices\n");
			rb->enabled = 1;
		} else {
			pr_error("Rollback not enabled\n");
			return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		}
	}
	/* Set register RSV56 to rollback fuse level and fuse index for QB */
	rollback_fuse_value = tegrabl_rollback_level_to_fusevalue(rb->level);
	SCRATCH_WRITE(SECURE_RSV56_SCRATCH_0, rollback_fuse_value);
	SCRATCH_WRITE(SECURE_RSV56_SCRATCH_1, rb->fuse_idx);
#endif
	rb_data = rb;
	return TEGRABL_NO_ERROR;
}

inline struct tegrabl_rollback *tegrabl_get_rollback_data(void)
{
	return rb_data;
}

tegrabl_error_t tegrabl_check_binary_rollback(uint32_t bin_type,
											  uint16_t rollback_level)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_rollback *rb = NULL;
	const char *bin_name = NULL;
	uint8_t expected_level = 0;

	rb = tegrabl_get_rollback_data();

	if (rb == NULL || rb->enabled == 0) {
		err = TEGRABL_NO_ERROR;
		goto out;
	}

	switch (bin_type) {
	case TEGRABL_BINARY_EXTENDED_CAN:
		bin_name = "Extended spe-fw";
		if (rollback_level < rb->limits.extended_spe_fw) {
			expected_level = rb->limits.extended_spe_fw;
			goto fail;
		}
		break;
	case TEGRABL_BINARY_CPU_BL:
		bin_name = "Cpu-bl";
		if (rollback_level < rb->limits.cpubl) {
			expected_level = rb->limits.cpubl;
			goto fail;
		}
		break;
	case TEGRABL_BINARY_TOS:
		bin_name = "SecureOs";
		if (rollback_level < rb->limits.tos) {
			expected_level = rb->limits.tos;
			goto fail;
		}
		break;
	case TEGRABL_BINARY_BPMP_FW:
		bin_name = "Bpmp-fw";
		if (rollback_level < rb->limits.bpmp_fw) {
			expected_level = rb->limits.bpmp_fw;
			goto fail;
		}
		break;
	default:
		goto out;
		break;
	}

	pr_info("%s rollback check passed, continue to boot...\n", bin_name);
	goto out;

fail:
	err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);

	pr_error("%s rollback detected - sw rollback lvl %u versus expected %u\n",
			 bin_name, rollback_level, expected_level);
out:
	return err;
}

tegrabl_error_t tegrabl_disable_rollback_prevention(void)
{
	struct tegrabl_rollback *rb;
	rb = tegrabl_get_rollback_data();

	if (rb == NULL)
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 0);

	rb->enabled = 0U;
	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t validate_rpb(struct tegrabl_rpb_handle *rpb)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t ecid[FUSE_ECID_SIZE];

	/* Check magic */
	if (memcmp(rpb->magic, rpb_magic, sizeof(rpb_magic)) != 0) {
		pr_error("RPB validation failed: magic mismatch\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	/* Check version */
	if (rpb->version != rpb_version) {
		pr_error("RPB validation failed: version mismatch\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	/* Check ECID */
	err = tegrabl_fuse_read(FUSE_UID, ecid, sizeof(ecid));
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get ECID\n");
		TEGRABL_SET_HIGHEST_MODULE(err);
		return err;
	}
	if (memcmp(rpb->ecid, ecid, sizeof(ecid)) != 0) {
		pr_error("RPB validation failed: ECID mismatch\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t check_rpb_signature(struct tegrabl_rpb_handle *rpb)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_verify_binary(TEGRABL_BINARY_RPB, (void *)rpb);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("RPB validation failed: signature mismatch\n");
		TEGRABL_SET_HIGHEST_MODULE(err);
	}

	return err;
}

tegrabl_error_t tegrabl_bypass_rollback_prevention(void *p_rpb)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_rpb_handle *rpb = NULL;

	rpb = (struct tegrabl_rpb_handle *)p_rpb;

	err = validate_rpb(rpb);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("RPB token is invalid, skip rollback prevention bypass.\n");
		return err;
	}

	err = check_rpb_signature(rpb);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("RPB token is invalid, skip rollback prevention bypass.\n");
		return err;
	}

	err = tegrabl_disable_rollback_prevention();
	return err;
}

tegrabl_error_t tegrabl_update_rollback_fuse(uint32_t fuse_value)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_rollback *rb = NULL;
	uint8_t fuse_idx;

	rb = tegrabl_get_rollback_data();
	if (rb == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 0);
		goto out;
	}

	fuse_idx = rb->fuse_idx;
	/* ReservedODM[0 to 7] are used for rollback protection.
	 * therefore, only burn the fuses corresponding to these rollback fuse levels.
	 */
	if (fuse_idx <= 7) {
		err = tegrabl_fuse_write(FUSE_RESERVED_ODM0 + fuse_idx, &fuse_value,
							 sizeof(fuse_value));
	} else {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	if (err != TEGRABL_NO_ERROR)
		TEGRABL_SET_HIGHEST_MODULE(err);

out:
	return err;
}

tegrabl_error_t tegrabl_prevent_rollback(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_rollback *rb = NULL;
	uint32_t rollback_fuse_value;
	uint8_t rollback_hw_level;
	uint8_t fuse_idx;

	rb = tegrabl_get_rollback_data();

	if (rb == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 0);
		goto out;
	}

	if (rb->enabled == 0) {
		err = TEGRABL_NO_ERROR;
		pr_info("Rollback prevention is disabled...\n");
		goto out;
	}

	/* Read rollback level from fuse */
	fuse_idx = rb->fuse_idx;

	err = tegrabl_fuse_read(FUSE_RESERVED_ODM0 + fuse_idx, &rollback_fuse_value,
							sizeof(rollback_fuse_value));
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto out;
	}

	rollback_hw_level =
			tegrabl_rollback_fusevalue_to_level(rollback_fuse_value);

	if (rb->level < rollback_hw_level) {
		/* If MB1_BCT's rollback level is stale, simply hang the device since
		 * all sw levels is outdated and not trust-worthy */
		pr_error("MB1_BCT rollback detected - "
				 "fuse level %u versus expected %u\n",
				 rollback_hw_level, rb->level);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto out;
	} else if (rb->level > rollback_hw_level) {
		/* Store new rollback level to register SECURE_RSV56_SCRATCH_0
		 * for CPU-BL to later read and burn this value to fuse */
		rollback_fuse_value = tegrabl_rollback_level_to_fusevalue(rb->level);
		SCRATCH_WRITE(SECURE_RSV56_SCRATCH_0, rollback_fuse_value);
		pr_info("Rollback fuse level is outdated, update to 0x%x\n",
				rollback_fuse_value);
		/* WAR:
		 * Burn rollback fuse right after mb1bct rollback check in this API,
		 * since fuse burning is not enabled in cboot now.
		 * Tracked in Bug 200266097
		 * Should remove this WAR in a very soon future once fuse burning is
		 * supported in cboot
		 */
		err = tegrabl_update_rollback_fuse(rollback_fuse_value);
		if (err != TEGRABL_NO_ERROR) {
			pr_warn("Rollback fuse burnt failed.\n");
			err = TEGRABL_NO_ERROR;
		}
	} else {
		/* Set register RSV56 to 0 if no need to update rollback fuse */
		SCRATCH_WRITE(SECURE_RSV56_SCRATCH_0, 0);
	}

	pr_info("MB1_BCT rollback level check passed, continue to boot..\n");

out:
	return err;
}

