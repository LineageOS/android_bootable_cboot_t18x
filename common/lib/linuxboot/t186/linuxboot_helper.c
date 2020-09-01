/*
 * Copyright (c) 2015-2020, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE  TEGRABL_ERR_LINUXBOOT

#include "build_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <libfdt.h>
#include <tegrabl_io.h>
#include <tegrabl_drf.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_compiler.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_linuxboot.h>
#include <tegrabl_linuxboot_helper.h>
#include <tegrabl_sdram_usage.h>
#include <tegrabl_cpubl_params.h>
#include <linux_load.h>
#include <armc.h>
#include <armiscreg.h>
#include <tegrabl_fuse.h>
#include <tegrabl_profiler.h>
#include <tegrabl_mb1bct_lib.h>
#include <tegrabl_uart.h>
#include <tegrabl_soc_misc.h>
#include <tegrabl_board_info.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_soc_misc.h>
#include <tegrabl_a_b_boot_control.h>
#include <tegrabl_prevent_rollback.h>
#include <arscratch.h>
#include <tegrabl_odmdata_soc.h>
#include <tegrabl_carveout_usage.h>
#include <tegrabl_page_allocator.h>
#include <tegrabl_malloc.h>
#include <tegrabl_partition_loader.h>
#include <tegrabl_gpt.h>
#include <tegrabl_sigheader.h>

#define SDRAM_START_ADDRESS			0x80000000

#define mc_read32(reg)				NV_READ32(NV_ADDRESS_MAP_MCB_BASE + reg)
#define SCRATCH_READ(reg)			NV_READ32(NV_ADDRESS_MAP_SCRATCH_BASE + SCRATCH_##reg)

#if defined(CONFIG_DYNAMIC_LOAD_ADDRESS)
#define MEM_SZ_64KB				(64 * 1024)
#define MEM_SZ_2MB				(0x200000)
#define MEM_ALIGN_MASK(x)			(~((x) - 1))
#define MEM_ALIGN(x, y)				(((uint64_t)(x) + (y) - 1) & MEM_ALIGN_MASK(y))
#define HEADERS_TOTAL_SIZE			(sizeof(struct tegrabl_sigheader) + \
							sizeof(union tegrabl_bootimg_header) + \
							sizeof(struct tegrabl_uboot_header))
/* U-Boot restricts itself to 4GB address space so that it can support platforms that cannot work
 * with addresses larger than 32bits.
 * So make sure that the load address is within 4GB boundary
 */
#define U_BOOT_TOP				(0x100000000)
#define U_BOOT_LOAD_ADDRESS_IS_VALID(x, sz)	(((x) > SDRAM_START_ADDRESS) && \
						(((x) + (sz) <= U_BOOT_TOP)))

#endif

extern struct tboot_cpubl_params *boot_params;
static struct tegrabl_linuxboot_memblock *update_bad_page(void);
struct tegrabl_carveout_info *p_carveout = NULL;

static int add_tegraid(char *cmdline, int len, char *param, void *priv)
{
	uint8_t chip_id = 0;
	uint8_t major = 0;
	uint8_t minor = 0;
	uint16_t netlist = 0;
	uint16_t patch = 0;
	uint32_t reg;
	TEGRABL_UNUSED(priv);

	if (!cmdline || !param) {
		return -1;
	}

	reg = NV_READ32(NV_ADDRESS_MAP_MISC_BASE + MISCREG_HIDREV_0);

	chip_id = NV_DRF_VAL(MISCREG, HIDREV, CHIPID, reg);
	major = NV_DRF_VAL(MISCREG, HIDREV, MAJORREV, reg);
	minor = NV_DRF_VAL(MISCREG, HIDREV, MINORREV, reg);

	reg = NV_READ32(NV_ADDRESS_MAP_MISC_BASE + MISCREG_EMU_REVID_0);

	netlist = NV_DRF_VAL(MISCREG, EMU_REVID, NETLIST, reg);
	patch = NV_DRF_VAL(MISCREG, EMU_REVID, PATCH, reg);

	return tegrabl_snprintf(cmdline, len, "%s=%x.%x.%x.%x.%x ", param,
					chip_id, major, minor, netlist, patch);
}

static int add_kerneltype(char *, int, char *, void *) __attribute__ ((unused));
static int add_kerneltype(char *cmdline, int len, char *param, void *priv)
{
	tegrabl_binary_type_t bin_type;
	int ret = -1;

	TEGRABL_UNUSED(priv);

	if (!cmdline || !param) {
		goto fail;
	}

	bin_type = tegrabl_get_kernel_type();
	ret = tegrabl_snprintf(cmdline, len, "%s=%s ", param,
			bin_type == TEGRABL_BINARY_KERNEL ? "normal" : "recovery");
fail:
	return ret;
}

static int add_maxcpus(char *cmdline, int len, char *param, void *priv)
{
	uint32_t enabled_cores_mask;
	uint32_t num_cores = 0;
	TEGRABL_UNUSED(priv);

	if (!cmdline || !param) {
		return -1;
	}

#if defined(CONFIG_MULTICORE_SUPPORT)
	if (tegrabl_fuse_read(FUSE_ENABLED_CPU_CORES,
						  &enabled_cores_mask, sizeof(uint32_t))) {
		return -1;
	}

	/* The max. number of cpus is equal to number of set bits in
	 * enabled_cores_mask */
	while (enabled_cores_mask != 0) {
		if (enabled_cores_mask & 1) {
			num_cores++;
		}
		enabled_cores_mask >>= 1;
	}

	return tegrabl_snprintf(cmdline, len, "%s=%u ", param, num_cores);
#else
	TEGRABL_UNUSED(enabled_cores_mask);
	TEGRABL_UNUSED(num_cores);

	return tegrabl_snprintf(cmdline, len, "%s=1 ", param);
#endif
}

static int add_secure_state(char *, int, char *, void *) __attribute__ ((unused));
static int add_secure_state(char *cmdline, int len, char *param, void *priv)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t secure_info;
	bool is_nv_production_mode = false;
	char *secure_state = NULL;

	TEGRABL_UNUSED(priv);

	if (!cmdline || !param) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	err = tegrabl_fuse_read(FUSE_TYPE_BOOT_SECURITY_INFO, &secure_info,
							sizeof(uint32_t));
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to read security info from fuse\n");
		return err;
	}

	is_nv_production_mode = fuse_is_nv_production_mode();

	/**
	 *  When FUSE_BOOT_SECURITY_INFO =
	 *  FUSE_BOOT_SECURITY_AESCMAC,
	 *  it means device is in non-secure mode.
	 *  When FUSE_BOOT_SECURITY_INFO =
	 *  FUSE_BOOT_SECURITY_RSA,
	 *  FUSE_BOOT_SECURITY_ECC,
	 *  FUSE_BOOT_SECURITY_AESCMAC_ENCRYPTION,
	 *  FUSE_BOOT_SECURITY_RSA_ENCRYPTION,
	 *  FUSE_BOOT_SECURITY_ECC_ENCRYPTION
	 *  it means device is in secure mode.
	 */
	if (is_nv_production_mode && secure_info > FUSE_BOOT_SECURITY_AESCMAC) {
		/* true secure mode */
		secure_state = "enabled";
	} else if (!is_nv_production_mode &&
			   secure_info > FUSE_BOOT_SECURITY_AESCMAC) {
		/* mixed configuration secure mode, customer will never get this
		 * mode of device */
		secure_state = "test.enabled";
	} else
		secure_state = "non-secure";

	return tegrabl_snprintf(cmdline, len, "%s=%s ", param, secure_state);
}

static int add_bootmode(char *, int, char *, void *) __attribute__ ((unused));
static int add_bootmode(char *cmdline, int len, char *param, void *priv)
{
	uint32_t odmdata_bootmode;
	char *bootmode_str = NULL;

	TEGRABL_UNUSED(priv);

	if (cmdline == NULL || param == NULL)
		return -1;

	odmdata_bootmode = tegrabl_odmdata_get() & TEGRA_BOOTMODE_MASK;

	switch (odmdata_bootmode) {
	case TEGRA_BOOTMODE_ANDROID_SHELL_VAL:
		/* boot to shell */
		bootmode_str = "shell";
		break;
	case TEGRA_BOOTMODE_ANDROID_PRESI_VAL:
		/* pre-silicon mode  */
		bootmode_str = "pre_si";
		break;
	case TEGRA_BOOTMODE_ANDROID_UI_VAL:
		/* fall through */
	default:
		/* boot to ui by default */
		bootmode_str = "ui";
		break;
	}

	return tegrabl_snprintf(cmdline, len, "%s=%s ", param, bootmode_str);
}

#if defined(CONFIG_ENABLE_A_B_SLOT)
static int add_boot_slot_suffix(char *, int, char *, void *) __attribute__ ((unused));
static int add_boot_slot_suffix(char *cmdline, int len, char *param, void *priv)
{
	char slot_suffix[BOOT_CHAIN_SUFFIX_LEN + 1];
	tegrabl_error_t status;

	TEGRABL_UNUSED(priv);

	if (!cmdline || !param) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	status = tegrabl_a_b_get_bootslot_suffix(slot_suffix, false);
	if (status != TEGRABL_NO_ERROR) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
	}

	pr_info("%s: slot_suffix = %s\n", __func__, slot_suffix);

	/* Add slot_suffix to cmdline*/
	return tegrabl_snprintf(cmdline, len, "%s=%s ", param, slot_suffix);
}

static int add_ratchet_values(char *cmdline, int len, char *param, void *priv)
{
	/* Format
	 * android.ratchetvalues=x.y.z
	 * x: rollback ratchet level of the device
	 * y: mb1 ratchet level of the device
	 * z: mts ratchet level of the device */

	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t rb_ratchet = 0;
	uint32_t mb1_ratchet = 0;
	uint32_t mts_ratchet = 0;
	uint32_t rollback_fuse_idx = 0;
	struct tegrabl_rollback *rb;

	TEGRABL_UNUSED(priv);

	/* rollback ratchet is stored in ODM_RESERVED_FUSE, index is stored in
	 * rollback struct */
	rb = tegrabl_get_rollback_data();
	if (rb != NULL) {
		rollback_fuse_idx = rb->fuse_idx;
		err = tegrabl_fuse_read(FUSE_RESERVED_ODM0 + rollback_fuse_idx,
								&rb_ratchet, sizeof(uint32_t));
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Failed to read OEM rollback ratchet\n");
			return -1;
		}
		rb_ratchet = tegrabl_rollback_fusevalue_to_level(rb_ratchet);
	}

	/* mb1 ratchet level is stored in SECURE_RSV54_SCRATCH_0 */
	mb1_ratchet = SCRATCH_READ(SECURE_RSV54_SCRATCH_0);

	/* Get mts ratchet from register SECURE_RSV52_SCRATCH_1 */
	mts_ratchet = SCRATCH_READ(SECURE_RSV52_SCRATCH_1);

	return tegrabl_snprintf(cmdline, len, "%s=%u.%u.%u ", param,
							rb_ratchet, mb1_ratchet, mts_ratchet);
}

bool is_system_as_root_enabled(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	bool ret = true;
	void *fdt;
	int fw_node, fstab_node, sys_node;

	/* Check dtb to see if system as root is enabled */
	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	err = tegrabl_dt_get_node_with_path(fdt, "/firmware/android", &fw_node);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	err = tegrabl_dt_get_node_with_compatible(fdt, fw_node, "android,fstab", &fstab_node);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	err = tegrabl_dt_get_node_with_compatible(fdt, fstab_node, "android,system", &sys_node);
	if (err == TEGRABL_NO_ERROR) {
		/* if DT node exists, system as root is disabld hence no extra cmdline param */
		ret = false;
	}
fail:
	return ret;
}

#if defined(CONFIG_ENABLE_SYSTEM_AS_ROOT)
static int add_boot_recovery_info(char *cmdline, int len, char *param,
								  void *priv)
{
	tegrabl_binary_type_t bin_type;
	int ret = -1;
	bool is_system_root_enabled;

	TEGRABL_UNUSED(priv);

	if (!cmdline || !param) {
		goto done;
	}

	is_system_root_enabled = is_system_as_root_enabled();
	if (!is_system_root_enabled) {
		/* system as root is disabled hence no extra cmdline param */
		ret = 0;
		goto done;
	}

	bin_type = tegrabl_get_kernel_type();
	if (bin_type != TEGRABL_BINARY_RECOVERY_KERNEL) {
		ret = tegrabl_snprintf(cmdline, len, "%s init=/init ", param);
	} else {
		ret = tegrabl_snprintf(cmdline, len, "init=/init ");
	}

done:
	return ret;
}
#endif /* CONFIG_ENABLE_SYSTEM_AS_ROOT */
#endif

static int tegrabl_linuxboot_add_vpr_info(char *cmdline, int len,
										  char *param, void *priv)
{
	uint32_t reg;
	uint64_t base;
	uint64_t size;

	TEGRABL_UNUSED(priv);

	if (!cmdline || !param) {
		return -1;
	}

	reg = mc_read32(MC_VIDEO_PROTECT_BOM_0);
	base = (uint64_t)reg;
	reg = mc_read32(MC_VIDEO_PROTECT_BOM_ADR_HI_0);
	base |= ((uint64_t)reg << 32);
	reg = mc_read32(MC_VIDEO_PROTECT_SIZE_MB_0);
	size = ((uint64_t)reg << 20);

	if (size)
		return tegrabl_snprintf(cmdline, len, "%s=0x%" PRIx64 "@0x%08" PRIx64
								" ", param, size, base);
	else
		return 0;
}

static int tegrabl_linuxboot_add_vprresize_info(char *cmdline, int len,
												char *param, void *priv)
{
	uint32_t reg;
	uint64_t size;

	TEGRABL_UNUSED(priv);

	if (!cmdline || !param) {
		return -1;
	}

	reg = mc_read32(MC_VIDEO_PROTECT_SIZE_MB_0);
	size = ((uint64_t)reg << 20);
	if (size) {
		return 0;
	}

	reg = mc_read32(MC_VIDEO_PROTECT_REG_CTRL_0);
	pr_debug("%s: VIDEO_PROTECT_REG_CTRL:0x%x\n", __func__, reg);
	if (NV_DRF_VAL(MC, VIDEO_PROTECT_REG_CTRL,
				   VIDEO_PROTECT_ALLOW_TZ_WRITE_ACCESS, reg))
		return tegrabl_snprintf(cmdline, len, "%s ", param);
	else
		return 0;
}

static int add_profiler_carveout(char *cmdline, int len,
								 char *param, void *priv)
{
	int ret = 0;
	TEGRABL_UNUSED(priv);

#if !defined(CONFIG_BOOT_PROFILER)
	TEGRABL_UNUSED(cmdline);
	TEGRABL_UNUSED(len);
	TEGRABL_UNUSED(param);
#else
	if (boot_params->global_data.profiling_carveout) {
		ret += tegrabl_snprintf(cmdline, len, "%s=0x%" PRIx32 "@0x%08" PRIx64
								" ", param, TEGRABL_PROFILER_PAGE_SIZE,
								boot_params->global_data.profiling_carveout);
	}
#endif

	return ret;
}

#if defined(CONFIG_ENABLE_NVDEC)
static int tegrabl_linuxboot_add_nvdec_enabled_info(char *cmdline, int len,
	char *param, void *priv)
{
	TEGRABL_UNUSED(priv);
	return tegrabl_snprintf(cmdline, len, "%s=1 ", param);
}
#endif

#if defined(CONFIG_ENABLE_VERIFIED_BOOT)
static const char *cmdline_vb_boot_state;
tegrabl_error_t tegrabl_linuxboot_set_vbstate(const char *vbstate)
{
	cmdline_vb_boot_state = vbstate;
	return TEGRABL_NO_ERROR;
}

static int add_vb_boot_state(char *cmdline, int len, char *param, void *priv)
{
	TEGRABL_UNUSED(priv);

	if (!cmdline || !param) {
		return -1;
	}

	if (!cmdline_vb_boot_state) {
		return 0;
	}

	return tegrabl_snprintf(cmdline, len, "%s=%s ", param,
							cmdline_vb_boot_state);
}

static const char *cmdline_vbmeta_info;
tegrabl_error_t tegrabl_linuxboot_set_vbmeta_info(const char *vbmeta)
{
	cmdline_vbmeta_info = vbmeta;
	return TEGRABL_NO_ERROR;
}

static int add_vbmeta_info(char *cmdline, int len, char *param, void *priv)
{
	TEGRABL_UNUSED(priv);
	TEGRABL_UNUSED(param);

	if (!cmdline || !param) {
		return -1;
	}

	if (!cmdline_vbmeta_info) {
		return 0;
	}

	return tegrabl_snprintf(cmdline, len, "%s ", cmdline_vbmeta_info);
}
#endif

static struct tegrabl_linuxboot_param extra_params[] = {
	{ "tegraid", add_tegraid, NULL },
	{ "maxcpus", add_maxcpus, NULL },
#if !defined(CONFIG_OS_IS_L4T)
	{ "tegra_keep_boot_clocks", tegrabl_linuxboot_add_string, NULL},
	{ "android.kerneltype", add_kerneltype, NULL },
	{ "androidboot.security", add_secure_state, NULL },
	{ "androidboot.bootmode", add_bootmode, NULL },
#if defined(CONFIG_ENABLE_VERIFIED_BOOT)
	{ "androidboot.verifiedbootstate", add_vb_boot_state, NULL },
	{ "androidboot.vbmeta", add_vbmeta_info, NULL },
#endif
#endif	/* !OS_IS_L4T */
#if defined(CONFIG_ENABLE_A_B_SLOT)
#if defined(CONFIG_OS_IS_L4T)
	{ "boot.slot_suffix", add_boot_slot_suffix, NULL },
	{ "boot.ratchetvalues", add_ratchet_values, NULL },
#else
	{ "androidboot.slot_suffix", add_boot_slot_suffix, NULL },
	{ "androidboot.ratchetvalues", add_ratchet_values, NULL },
#if defined(CONFIG_ENABLE_SYSTEM_AS_ROOT)
	{ "skip_initramfs", add_boot_recovery_info, NULL },
#endif /* CONFIG_ENABLE_SYSTEM_AS_ROOT */
#endif
#endif
	{ "vpr", tegrabl_linuxboot_add_vpr_info, NULL },
	{ "vpr_resize", tegrabl_linuxboot_add_vprresize_info, NULL },
	{ "bl_prof_dataptr", add_profiler_carveout, NULL},
	{ "sdhci_tegra.en_boot_part_access", tegrabl_linuxboot_add_string, "1" },
#if defined(CONFIG_ENABLE_NVDEC)
	{ "nvdec_enabled", tegrabl_linuxboot_add_nvdec_enabled_info, NULL },
#endif
	{ NULL, NULL, NULL},
};

#define mpidr_to_cpu_idx(mpidr) ((((mpidr >> 8) & 0x3) * 4) + (mpidr & 0x3))

/* Add reset status under 'chosen/reset/' node in DT */
static tegrabl_error_t add_pmc_reset_info(void *fdt, int nodeoffset)
{
	tegrabl_error_t status = TEGRABL_NO_ERROR;
	int err;
	int node, node_pmc;
	tegrabl_rst_source_t rst_source;
	tegrabl_rst_level_t rst_level;
	char str[12];
	char *pmc_reset_source_table[] = {
									  "SYS_RESET_N",
									  "AOWDT",
									  "BCCPLEXWDT",
									  "BPMPWDT",
									  "SCEWDT",
									  "SPEWDT",
									  "APEWDT",
									  "LCCPLEXWDT",
									  "SENSOR",
									  "AOTAG",
									  "VFSENSOR",
									  "MAINSWRST",
									  "SC7",
									  "HSM",
									  "CSITE",
		};

	/* Check if the property is already present under 'chosen' node */
	node = tegrabl_add_subnode_if_absent(fdt, nodeoffset, "reset");
	if (node < 0) {
		status = TEGRABL_ERROR(TEGRABL_ERR_ADD_FAILED, 0);
		goto fail;
	}

	/* Check if the property is already present under 'chosen/reset' node */
	node_pmc = tegrabl_add_subnode_if_absent(fdt, node, "pmc-reset-reason");
	if (node_pmc < 0) {
		status = TEGRABL_ERROR(TEGRABL_ERR_ADD_FAILED, 0);
		goto fail;
	}

	/* Obtain pmc reset reason */
	status = tegrabl_get_rst_status(&rst_source, &rst_level);
	if (status != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(status);
		pr_error("Unable to set pmc-reset-reason\n");
		goto fail;
	}

	memset(str, '\0', sizeof(str));

	if (sizeof(str) >= strlen(pmc_reset_source_table[rst_source]) + 1U)
		strcpy(str, pmc_reset_source_table[rst_source]);
	else {
		status = TEGRABL_ERROR(TEGRABL_ERR_NAME_TOO_LONG, 0);
		goto fail;
	}

	err = fdt_setprop_string(fdt, node_pmc, "reset-source", str);
	if (err < 0) {
		pr_error("Unable to set pmc-reset-reason (%s)\n",
				 fdt_strerror(err));
		status = TEGRABL_ERROR(TEGRABL_ERR_ADD_FAILED, 0);
		goto fail;
	}

	memset(str, '\0', sizeof(str));
	tegrabl_snprintf(str, sizeof("x"), "%x", rst_level);
	err = fdt_setprop_string(fdt, node_pmc, "reset-level", str);
	if (err < 0) {
		pr_error("Unable to set pmc-reset-reason (%s)\n",
				 fdt_strerror(err));
		status = TEGRABL_ERROR(TEGRABL_ERR_ADD_FAILED, 0);
		goto fail;
	}

	pr_debug("Updated %s info to DT\n", "chosen/reset/pmc-reset-reason");
	return TEGRABL_NO_ERROR;

fail:
	return status;
}

/* check whether n is power of 2 or not */
static uint8_t is_power_of_two(uint8_t n)
{
	return n && (!(n & (n-1)));
}

/* Returns position of the only set bit in 'n' */
static uint8_t find_set_position(uint8_t n)
{
	/* Note - Pos starts from 1. 0 is considered Invalid */
	unsigned i = 1, pos = 1;

	if (!is_power_of_two(n)) {
		return 0;
	}

	/* Iterate till 'i' and 'n' have a set bit at same position */
	while (!(i & n)) {
		i = i << 1;
		++pos;
	}

	return pos;
}

/* Add reset status under 'chosen/reset/' node in DT */
static tegrabl_error_t add_pmic_reset_info(void *fdt, int nodeoffset)
{
	tegrabl_error_t status = TEGRABL_NO_ERROR;
	int err;
	int node, node_pmic;
	uint32_t reset_status = 0;
	char str[25];
	char *pmic_reset_reason_table[] = {
									   "NIL_OR_MORE_THAN_1_BIT",
									   "SHDN",     /* B0 */
									   "WDOG",
									   "HDRST",
									   "TOVLD",
									   "MBSLD",
									   "MBO",
									   "MBU",
									   "RSTIN",    /* B7 */
	};

	/* Check if the property is already present under 'chosen' node */
	node = tegrabl_add_subnode_if_absent(fdt, nodeoffset, "reset");
	if (node < 0) {
		status = TEGRABL_ERROR(TEGRABL_ERR_ADD_FAILED, 0);
		goto fail;
	}

	/* Check if the property is already present under 'chosen/reset' node */
	node_pmic = tegrabl_add_subnode_if_absent(fdt, node, "pmic-reset-reason");
	if (node_pmic < 0) {
		status = TEGRABL_ERROR(TEGRABL_ERR_ADD_FAILED, 0);
		goto fail;
	}

	/* Set 'pmic reset reason' */
	reset_status = boot_params->global_data.pmic_rst_reason;

	memset(str, '\0', sizeof(str));
	tegrabl_snprintf(str, sizeof("0xAB"), "0x%02x", reset_status);
	err = fdt_setprop_string(fdt, node_pmic, "register-value", str);
	if (err < 0) {
		pr_error("Unable to set pmic-reset-reason (%s)\n",
				 fdt_strerror(err));
		status = TEGRABL_ERROR(TEGRABL_ERR_ADD_FAILED, 0);
		goto fail;
	}

	memset(str, '\0', sizeof(str));

	if (sizeof(str) >=
		strlen(pmic_reset_reason_table[find_set_position(reset_status)]) + 1U)
		strcpy(str, pmic_reset_reason_table[find_set_position(reset_status)]);
	else {
		status = TEGRABL_ERROR(TEGRABL_ERR_NAME_TOO_LONG, 1);
		goto fail;
	}

	err = fdt_setprop_string(fdt, node_pmic, "reason", str);
	if (err < 0) {
		pr_error("Unable to set pmic-reset-reason (%s)\n",
				 fdt_strerror(err));
		status = TEGRABL_ERROR(TEGRABL_ERR_ADD_FAILED, 2);
		goto fail;
	}

	pr_debug("Updated %s info to DT\n", "chosen/reset/pmic-reset-reason");
	return TEGRABL_NO_ERROR;

fail:
	return status;
}
static tegrabl_error_t disable_floorswept_cpus(void *fdt, int nodeoffset)
{
	uint32_t enabled_cores_mask;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t cpu;
	uint32_t mpidr;
	const void *p_reg;
	int offset;
	int dterr;

	err = tegrabl_fuse_read(FUSE_ENABLED_CPU_CORES,
							&enabled_cores_mask, sizeof(uint32_t));
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		return err;
	}

	offset = nodeoffset;
	pr_debug("/cpus node offset: %d\n", nodeoffset);
	while (offset > 0) {
		/* Look for nodes with 'device_type = "cpu"' property */
		offset = fdt_node_offset_by_prop_value(fdt, offset,
											   "device_type",
											   "cpu", strlen("cpu")+1);
		pr_debug("/cpus/cpu node offset: %d\n", offset);
		if (offset > nodeoffset) {
			p_reg = fdt_getprop(fdt, offset, "reg", NULL);
			if (!p_reg) {
				pr_error("couldn't find reg property in cpu node\n");
				return TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
			}

			/* reg property of the node is of the form <0 mpidr> */
			mpidr = fdt32_to_cpu(*((uint32_t *)p_reg + 1));
			pr_debug("mpidr: 0x%x\n", mpidr);
			/* convert mpidr to cpu-index */
			cpu = mpidr_to_cpu_idx(mpidr);
			pr_debug("cpu: %u\n", cpu);

			/* if this cpu is not in enabled_cores_mask,
			 * mark the DT-node as disabled */
			if (!(enabled_cores_mask & (1 << cpu))) {
				dterr = fdt_setprop(fdt, offset, "status",
									"disabled", strlen("disabled") + 1);
				if (dterr < 0) {
					pr_error("Failed to disable cpu node: %s\n",
							 fdt_strerror(dterr));
					return TEGRABL_ERROR(TEGRABL_ERR_ADD_FAILED, 0);
				}
				pr_info("Disabled cpu-%u node in FDT\n", cpu);
			}
		}
	}

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t update_vpr_info(void *fdt, int nodeoffset)
{
	int node;
	uint32_t reg;

	/*
	 * If BL did not carveout vpr and vpr-resize is allowed, then
	 * allow vpr DT node as it is the only way to specify vpr carveout size.
	 */
	reg = mc_read32(MC_VIDEO_PROTECT_REG_CTRL_0);
	if (NV_DRF_VAL(MC, VIDEO_PROTECT_REG_CTRL,
				   VIDEO_PROTECT_ALLOW_TZ_WRITE_ACCESS, reg)) {
		reg = mc_read32(MC_VIDEO_PROTECT_SIZE_MB_0);
		if (!reg) {
			return TEGRABL_NO_ERROR;
		}
	}

	/* Otherwise, remove vpr DT node. */
	node = fdt_subnode_offset(fdt, nodeoffset, "vpr-carveout");
	if (node < 0) {
		return TEGRABL_NO_ERROR; /* vpr DT node not present. we are good */
	}

	fdt_delprop(fdt, node, "compatible");
	fdt_delprop(fdt, node, "reg");
	fdt_delprop(fdt, node, "size");
	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t update_ramoops_info(void *fdt, int nodeoffset)
{
	int node;
	int dterr;
	uint64_t reg[2];
	uint64_t ramoops_addr;
	uint64_t ramoops_size = TEGRABL_CARVEOUT_RAMOOPS_SIZE;

	ramoops_addr = boot_params->global_data.carveout[CARVEOUT_CPUBL_PARAMS].base;
	ramoops_addr += TEGRABL_RAMOOPS_OFFSET;

	reg[0] = cpu_to_fdt64(ramoops_addr);
	reg[1] = cpu_to_fdt64(ramoops_size);

	node = fdt_subnode_offset(fdt, nodeoffset, "ramoops_carveout");
	if (node < 0) {
		/* console-ramoops DT node not present. so return */
		return TEGRABL_NO_ERROR;
	}

	fdt_delprop(fdt, node, "size");
	fdt_delprop(fdt, node, "alloc-ranges");

	dterr = fdt_setprop(fdt, node, "reg", reg, 2 * sizeof(uint64_t));
	if (dterr < 0) {
		pr_error("Failed to set reg base for ramoops_carveout node: %s\n",
				 fdt_strerror(dterr));
		return TEGRABL_ERROR(TEGRABL_ERR_ADD_FAILED, 0);
	}

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t update_gamedata_info(void *fdt, int nodeoffset)
{
	int node;
	int dterr;
	uint64_t reg[2];
	uint64_t gamedata_addr;
	uint64_t gamedata_size = TEGRABL_CARVEOUT_GAMEDATA_SIZE;

	gamedata_addr = boot_params->global_data.carveout
				[CARVEOUT_CPUBL_PARAMS].base;
	gamedata_addr += TEGRABL_GAMEDATA_OFFSET;

	reg[0] = cpu_to_fdt64(gamedata_addr);
	reg[1] = cpu_to_fdt64(gamedata_size);

	node = fdt_subnode_offset(fdt, nodeoffset, "gamedata_carveout");
	if (node < 0) {
		/* DT node not present. so return */
		return TEGRABL_NO_ERROR;
	}

	fdt_delprop(fdt, node, "size");
	fdt_delprop(fdt, node, "alloc-ranges");

	dterr = fdt_setprop(fdt, node, "reg", reg, 2 * sizeof(uint64_t));
	if (dterr < 0) {
		pr_error("Failed to set reg base for gamedata_carveout node: %s\n",
				 fdt_strerror(dterr));
		return TEGRABL_ERROR(TEGRABL_ERR_ADD_FAILED, 0);
	}

	return TEGRABL_NO_ERROR;
}

#define ECID_STR_SIZE 64
static tegrabl_error_t add_ecid_info(void *fdt, int nodeoffset)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	char ecid_str[ECID_STR_SIZE] = {'\0'};
	int32_t fdt_err;

	err = tegrabl_get_ecid_str(ecid_str, ECID_STR_SIZE);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("Failed to read ecid (err = %x), skip adding to DT ...\n", err);
		err = TEGRABL_NO_ERROR;
		goto fail;
	}

	fdt_err = fdt_setprop_string(fdt, nodeoffset, "ecid", ecid_str);
	if (fdt_err < 0) {
		pr_error("Failed to add ecid in DT\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_ADD_FAILED, 0);
		goto fail;
	}

	pr_info("Adding ecid(%s) to DT\n", ecid_str);

fail:
	return err;
}

static struct tegrabl_linuxboot_dtnode_info extra_nodes[] = {
	{ "chosen", add_pmc_reset_info},
	{ "chosen", add_pmic_reset_info},
	{ "chosen", add_ecid_info},
	{ "cpus" , disable_floorswept_cpus },
	{ "reserved-memory", update_vpr_info},
	{ "reserved-memory", update_ramoops_info},
	{ "reserved-memory", update_gamedata_info},
	{ NULL, NULL},
};

static int bom_compare(const uint32_t a, const uint32_t b)
{
	p_carveout = (struct tegrabl_carveout_info *)(boot_params->global_data.carveout);

	if (p_carveout[a].base < p_carveout[b].base)
		return -1;
	else if (p_carveout[a].base > p_carveout[b].base)
		return 1;
	else
		return 0;
}

static void sort(uint32_t array[], int32_t count)
{
	uint32_t val;
	int32_t i;
	int32_t j;

	if (count < 2)
		return;

	for (i = 1; i < count; i++) {
		val = array[i];

		for (j = (i - 1);
				 (j >= 0) && (bom_compare(val, array[j]) < 0);
				 j--) {
			array[j + 1] = array[j];
		}

		array[j + 1] = val;
	}
}

static void sort_array(uint64_t arr[], uint64_t count)
{
	uint64_t i, j, temp;

	if (count == 0)
		return;

	for (i = 0; i < count-1; i++) {
		for (j = 0; j < count - i - 1; j++) {
			if (arr[j] > arr[j+1]) {
				temp = arr[j];
				arr[j] = arr[j+1];
				arr[j+1] = temp;
			}
		}
	}
}

static struct tegrabl_linuxboot_memblock free_block[CARVEOUT_NUM + 1];
static struct tegrabl_linuxboot_memblock free_dram_block[CARVEOUT_NUM + NUM_DRAM_BAD_PAGES + 1];
static uint32_t free_dram_block_count;

static struct tegrabl_linuxboot_memblock *update_bad_page()
{
	uint64_t *bad_page_arr = NULL;
	uint64_t bad_page_count = 0;
	uint64_t chunk_idx, page_idx, rgn, temp_size;

	bad_page_arr = (uint64_t *)(boot_params->global_data.dram_bad_pages);
	bad_page_count = boot_params->global_data.valid_dram_bad_page_count;

	if (bad_page_count > 1)
		sort_array(bad_page_arr, bad_page_count);

	chunk_idx = 0;
	rgn = 0;

	for (page_idx = 0; page_idx < bad_page_count; page_idx++) {
		while (chunk_idx < free_dram_block_count) {
			/*
			 * free_dram_block is 0 initialized in BSS
			 * if the free_dram_block was split due to a bad page,
			 * free_dram_block would be non zero
			 */
			if ((free_dram_block[rgn].base == 0) &&
					(free_dram_block[rgn].size == 0)) {
				free_dram_block[rgn].base = free_block[chunk_idx].base;
				free_dram_block[rgn].size = free_block[chunk_idx].size;
			}
			if (bad_page_arr[page_idx] == free_dram_block[rgn].base) {
				free_dram_block[rgn].base = free_dram_block[rgn].base + PAGE_SIZE;
				free_dram_block[rgn].size = free_dram_block[rgn].size - PAGE_SIZE;
				break;
			}
			if (bad_page_arr[page_idx] ==
					free_dram_block[rgn].base + free_dram_block[rgn].size - PAGE_SIZE) {
				free_dram_block[rgn].size = free_dram_block[rgn].size - PAGE_SIZE;
				break;
			}
			if (bad_page_arr[page_idx] > free_dram_block[rgn].base &&
			    bad_page_arr[page_idx] < free_dram_block[rgn].base + free_dram_block[rgn].size) {
				temp_size = free_dram_block[rgn].size;
				free_dram_block[rgn].size = bad_page_arr[page_idx] - free_dram_block[rgn].base;

				rgn += 1;
				free_dram_block[rgn].base =
					free_dram_block[rgn - 1].base + free_dram_block[rgn - 1].size + PAGE_SIZE;
				free_dram_block[rgn].size = temp_size - free_dram_block[rgn - 1].size - PAGE_SIZE;
				break;
			}
			chunk_idx++;
			rgn++;
		}
	}
	/* If there was at least one bad page, move to next chunk and rgn */
	if (bad_page_count > 0) {
		chunk_idx++;
		rgn++;
	}
	/* Add rest of the memory chunks */
	while (chunk_idx < free_dram_block_count) {
		free_dram_block[rgn].base = free_block[chunk_idx].base;
		free_dram_block[rgn].size = free_block[chunk_idx].size;
		chunk_idx++;
		rgn++;
	}

	free_dram_block_count = rgn;
	return free_dram_block;
}

static uint32_t calculate_free_dram_regions(struct tegrabl_linuxboot_memblock
		**free_dram_regions)
{
	carve_out_type_t cotype;
	int32_t i, rgn;
	int32_t count;
	uint64_t cur_start, cur_end, sdram_size;
	uint32_t perm_carveouts[CARVEOUT_NUM];

	if (p_carveout != NULL) {
		/* We calculate all free DRAM regions at once,
		 * If called again, just return*/
		*free_dram_regions = free_dram_block;
		return free_dram_block_count;
	}

	p_carveout = (struct tegrabl_carveout_info *)(boot_params->global_data.carveout);
	count = 0;

	/* Prepare a list of permanent DRAM carveouts */
	for (cotype = CARVEOUT_NVDEC; cotype < CARVEOUT_NUM; cotype++) {
		if ((p_carveout[cotype].base < SDRAM_START_ADDRESS) ||
			(p_carveout[cotype].size == 0)) {
			continue;
		}

		switch (cotype) {
		/* Skip the temporary/invalid carveouts */
		case CARVEOUT_MB2:
		case CARVEOUT_CPUBL:
		case CARVEOUT_RESERVED1:
		case CARVEOUT_PRIMARY:
		case CARVEOUT_EXTENDED:
		case CARVEOUT_MB2_HEAP:
		case CARVEOUT_BO_MTS_PACKAGE:
			break;
		default:
			perm_carveouts[count] = (uint32_t)cotype;
			count++;
			break;
		}
	}

	/* Sort the carveouts in increasing order of their base */
	sort(perm_carveouts, count);

	/* Determine the free regions */
	cur_start = SDRAM_START_ADDRESS;
	rgn = 0;

	for (i = 0; i < count; i++) {
		cur_end = p_carveout[perm_carveouts[i]].base;
		if (cur_end > cur_start) {
			pr_debug("[%d] START: 0x%"PRIx64", END: 0x%"PRIx64"\n",
					 rgn, cur_start, cur_end);
			free_block[rgn].base = cur_start;
			free_block[rgn].size = cur_end - cur_start;
			rgn++;
		}
		cur_start = p_carveout[perm_carveouts[i]].base +
			p_carveout[perm_carveouts[i]].size;
	}

	sdram_size = NV_READ32(NV_ADDRESS_MAP_MCB_BASE + MC_EMEM_CFG_0);
	sdram_size = sdram_size << 20;
	cur_end = SDRAM_START_ADDRESS + sdram_size;
	if (cur_end > cur_start) {
		pr_debug("[%d] START: 0x%"PRIx64", END: 0x%"PRIx64"\n",
				 rgn, cur_start, cur_end);
		free_block[rgn].base = cur_start;
		free_block[rgn].size = cur_end - cur_start;
		rgn++;
	}
	free_dram_block_count = rgn;
	*free_dram_regions = update_bad_page();

	return free_dram_block_count;
}

uint32_t get_free_dram_regions_info(struct tegrabl_linuxboot_memblock
		**free_dram_regions)
{
	return calculate_free_dram_regions(free_dram_regions);
}

#if defined(CONFIG_DYNAMIC_LOAD_ADDRESS)
/* Structure used for accounting free dram region allocation */
struct dram_alloc {
	void *start;
	uint64_t size;
	struct dram_alloc *next;
};

/* Base of linked list used for keeping track of used free dram region */
static struct dram_alloc dram_alloc_base;

/*
 * Returns true if provided addr region can be used as is.
 * Returns false if addr is already allocated. available_addr will be updated
 * with an address that is not allocated.
 */
static bool is_dram_address_free(void *addr, uint64_t size, void **available_addr)
{
	struct dram_alloc *ptr_alloc = &dram_alloc_base;
	void *alloc_addr_start;
	void *alloc_addr_end;
	void *addr_end;
	bool ret = true;

	/* Loop through all allocated regions */
	while (ptr_alloc) {
		alloc_addr_start = ptr_alloc->start;
		alloc_addr_end = alloc_addr_start + ptr_alloc->size;
		addr_end = addr + size;

		/*
		 * Check if the addresses overlap
		 * 1. Is allocated address region overlapping with input range
		 * 2. Is input region part of any allocated address region
		 */
		if ((((alloc_addr_start >= addr) && (alloc_addr_start <= addr_end)) ||
				((alloc_addr_end >= addr) && (alloc_addr_end <= addr_end)))	||
		    (((addr >= alloc_addr_start) && (addr <= alloc_addr_end))		||
				((addr_end >= alloc_addr_start) && (addr_end <= alloc_addr_end)))) {
			ret = false;
			/* If addr is allocated, suggest next page after this region */
			addr = alloc_addr_end + PAGE_SIZE;
		}
		ptr_alloc = ptr_alloc->next;
	}
	if (available_addr != NULL) {
		*available_addr = addr;
	}

	return ret;
}

/* Add dram address range to a sorted linked list */
static void alloc_dram_free_region(void *addr, uint64_t size)
{
	struct dram_alloc *ptr_alloc = &dram_alloc_base;
	struct dram_alloc *ptr_temp = NULL;

	if (dram_alloc_base.start != NULL) {
		/* Find the right position in sorted list */
		while ((addr > (ptr_alloc->start + ptr_alloc->size)) && (ptr_alloc->next)) {
			ptr_alloc = ptr_alloc->next;
		}
		ptr_temp = ptr_alloc->next;
		ptr_alloc->next = tegrabl_malloc(sizeof(struct dram_alloc));
		ptr_alloc = ptr_alloc->next;
	}
	ptr_alloc->start = addr;
	ptr_alloc->size = size;
	ptr_alloc->next = ptr_temp;
}

uint64_t tegrabl_get_free_dram_address(uint64_t size)
{
	void *address = NULL;
	uint64_t i;

	for (i = 0; i < free_dram_block_count; i++) {
		if (free_dram_block[i].size >= size) {
			/*
			 * Check if free dram block is unallocated.
			 * If not update address with free region available
			 */
			if (is_dram_address_free((void *)free_dram_block[i].base, size, &address)) {
				alloc_dram_free_region((void *)address, size);
				break;
			} else {
				/*
				 * If free region updated in address is within
				 * same free dram block, use it
				 */
				if ((address + size) < (void *)(free_dram_block[i].base + free_dram_block[i].size)) {
					alloc_dram_free_region((void *)address, size);
					break;
				} else {
					address = NULL;
				}
			}
		}
	}
	return (uint64_t)address;
}

/* Free memory allocations made while allocating dram free region */
void tegrabl_dealloc_free_dram_region(void)
{
	struct dram_alloc *ptr_alloc = dram_alloc_base.next;

	while (ptr_alloc) {
		tegrabl_free(ptr_alloc);
		ptr_alloc = ptr_alloc->next;
	}
}

void tegrabl_dealloc_dram_address(uint64_t addr)
{
	struct dram_alloc *ptr_alloc = &dram_alloc_base;
	struct dram_alloc *ptr_prev = NULL;
	void *address = (void *)addr;

	while (ptr_alloc) {
		if (ptr_alloc->start == address) {
			if (ptr_prev != NULL) {
				ptr_prev->next = ptr_alloc->next;
				tegrabl_free(ptr_alloc);
			} else {
				if (ptr_alloc->next != NULL) {
					dram_alloc_base.start = ptr_alloc->next->start;
					dram_alloc_base.size = ptr_alloc->next->size;
					dram_alloc_base.next = ptr_alloc->next->next;
					tegrabl_free(ptr_alloc->next);
				} else {
					dram_alloc_base.start = NULL;
					dram_alloc_base.size = 0;
					dram_alloc_base.next = NULL;
				}
			}
			break;
	}
	ptr_prev = ptr_alloc;
	ptr_alloc = ptr_alloc->next;
	}
}

tegrabl_error_t tegrabl_alloc_u_boot_top(uint64_t size)
{
	uint64_t block = free_dram_block_count;
	uint64_t block_size = 0;
	uint64_t block_base = 0;
	tegrabl_error_t err = TEGRABL_ERR_NO_MEMORY;
	uint64_t i;

	/* align size to 2MB */
	size += MEM_SZ_2MB;
	for (i = 0; i < free_dram_block_count; i++) {
		if (free_dram_block[i].base > U_BOOT_TOP) {
			if (i > 0) {
				block = i - 1;
				block_size = free_dram_block[block].size;
				block_base = free_dram_block[block].base + block_size - size;
			}
			break;
		}
		if ((free_dram_block[i].base + free_dram_block[i].size) > U_BOOT_TOP) {
			block = i;
			block_size = U_BOOT_TOP - free_dram_block[i].base;
			block_base = U_BOOT_TOP - size;
			break;
		}
	}
	if ((block < free_dram_block_count) && (block_size > size)) {
		/* Add memory region to used regions list */
		if (is_dram_address_free((void *)block_base, size, NULL)) {
			err = TEGRABL_NO_ERROR;
			alloc_dram_free_region((void *)block_base, size);
			pr_info("Reserved memory at 0x%lx for U-Boot relocation\n", block_base);
		}
	}
	return err;
}
#endif /* CONFIG_DYNAMIC_LOAD_ADDRESS */

tegrabl_error_t tegrabl_linuxboot_helper_get_info(
					tegrabl_linux_boot_info_t info,
					const void *in_data, void *out_data)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_linuxboot_memblock *memblock;
	struct tegrabl_linuxboot_memblock *free_dram_regions = NULL;
	uint32_t temp32;
	uint64_t addr;

	/* Note: in_data is not mandatory for all info-types */
	if (out_data == NULL) {
		pr_error("out_data is NULL\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	pr_debug("Got request for %u info\n", info);

	switch (info) {
	case TEGRABL_LINUXBOOT_INFO_EXTRA_CMDLINE_PARAMS:
		pr_debug("%s: extra_params: %p\n", __func__, extra_params);
		*(struct tegrabl_linuxboot_param **)out_data = extra_params;
		break;

	case TEGRABL_LINUXBOOT_INFO_EXTRA_DT_NODES:
		pr_debug("%s: extra_nodes: %p\n", __func__, extra_nodes);
		*(struct tegrabl_linuxboot_dtnode_info **)out_data = extra_nodes;
		break;

	case TEGRABL_LINUXBOOT_INFO_DEBUG_CONSOLE:
		if (boot_params->enable_log == 0) {
			*(tegrabl_linuxboot_debug_console_t *)out_data =
				TEGRABL_LINUXBOOT_DEBUG_CONSOLE_NONE;
		} else {
			*(tegrabl_linuxboot_debug_console_t *)out_data =
				TEGRABL_LINUXBOOT_DEBUG_CONSOLE_UARTA +
				boot_params->uart_instance;
		}
		pr_debug("%s: console = %u\n", __func__,
				 *((tegrabl_linuxboot_debug_console_t *)out_data));
		break;

	case TEGRABL_LINUXBOOT_INFO_EARLYUART_BASE:
		if (tegrabl_uart_get_address(boot_params->uart_instance, &addr) !=
			TEGRABL_NO_ERROR)
			*(uint64_t *)out_data = 0;
		else
			*(uint64_t *)out_data = addr;
		pr_debug("%s: early-uartbase = 0x%lx\n", __func__,
				 *((uint64_t *)out_data));
		break;

	case TEGRABL_LINUXBOOT_INFO_CARVEOUT:
		if (in_data == NULL) {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}
		temp32 = *((uint32_t *)in_data);
		memblock = (struct tegrabl_linuxboot_memblock *)out_data;
		switch (temp32) {
		case TEGRABL_LINUXBOOT_CARVEOUT_VPR:
			memblock->base =
				boot_params->global_data.carveout[CARVEOUT_VPR].base;
			memblock->size =
				boot_params->global_data.carveout[CARVEOUT_VPR].size;
			break;
		case TEGRABL_LINUXBOOT_CARVEOUT_BPMPFW:
			memblock->base =
				boot_params->global_data.carveout[CARVEOUT_BPMP].base;
			memblock->size =
				boot_params->global_data.carveout[CARVEOUT_BPMP].size;
			break;
		case TEGRABL_LINUXBOOT_CARVEOUT_LP0:
			memblock->base =
				boot_params->global_data.carveout[CARVEOUT_SC7_RESUME_FW].base;
			memblock->size =
				boot_params->global_data.carveout[CARVEOUT_SC7_RESUME_FW].size;
			break;
		case TEGRABL_LINUXBOOT_CARVEOUT_NVDUMPER:
			memblock->base =
				boot_params->global_data.carveout[CARVEOUT_RAMDUMP].base;
			memblock->size =
				boot_params->global_data.carveout[CARVEOUT_RAMDUMP].size;
			break;
		default:
			memblock->base = 0x0;
			memblock->size = 0x0;
		}
		break;

	case TEGRABL_LINUXBOOT_INFO_MEMORY:
		if (in_data == NULL) {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}
		temp32 = *((uint32_t *)in_data);
		memblock = (struct tegrabl_linuxboot_memblock *)out_data;

		free_dram_block_count = calculate_free_dram_regions(&free_dram_regions);

		if (temp32 >= free_dram_block_count) {
			memblock->base = 0;
			memblock->size = 0;
		} else {
			memblock->base = free_dram_regions[temp32].base;
			memblock->size = free_dram_regions[temp32].size;
		}

		pr_debug("%s: memblock(%u) (base:0x%"PRIx64", size:0x%"PRIx64")\n",
				 __func__, *((uint32_t *)in_data),
				 memblock->base, memblock->size);
		break;

	case TEGRABL_LINUXBOOT_INFO_INITRD:
		memblock = (struct tegrabl_linuxboot_memblock *)out_data;
		tegrabl_get_ramdisk_info(&memblock->base, &memblock->size);
		pr_debug("%s: ramdisk (base:0x%lx, size:0x%lx)\n",
				 __func__, memblock->base, memblock->size);
		break;

	case TEGRABL_LINUXBOOT_INFO_BOOTIMAGE_CMDLINE:
		*(char **)out_data = tegrabl_get_bootimg_cmdline();
		break;

	case TEGRABL_LINUXBOOT_INFO_SECUREOS:
		*(uint32_t *)out_data = boot_params->secureos_type;
		break;

	case TEGRABL_LINUXBOOT_INFO_BOARD:
	default:
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		break;
	};

fail:
	return err;
}

#if defined(CONFIG_DYNAMIC_LOAD_ADDRESS)
tegrabl_error_t tegrabl_get_nct_load_addr(void **load_addr)
{
	uint64_t addr;
	addr = tegrabl_get_free_dram_address(NCT_PART_SIZE + MEM_SZ_64KB);
	*load_addr = (void *)MEM_ALIGN(addr, MEM_SZ_64KB);
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_get_boot_img_load_addr(void **load_addr)
{
	uint64_t addr, partition_size;
	void *addr_align;
	uint64_t bootimg_size = 0;
	uint64_t loading_total_size;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	char partition_name[TEGRABL_GPT_MAX_PARTITION_NAME + 1];
	struct tegrabl_partition partition;
	uint8_t *ptr;
	union tegrabl_bootimg_header *bootimg_header = NULL;

	err = tegrabl_get_partition_name(TEGRABL_BINARY_KERNEL, 0, partition_name);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get bootimage partition name\n");
		goto exit;
	}
	err = tegrabl_partition_open(partition_name, &partition);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed while opening bootimage partition\n");
		goto exit;
	}
	partition_size = tegrabl_partition_size(&partition);
	addr = tegrabl_get_free_dram_address(partition_size + MEM_SZ_2MB);
	addr_align = (void *)MEM_ALIGN(addr, MEM_SZ_2MB);
	if (!U_BOOT_LOAD_ADDRESS_IS_VALID((uint64_t)addr_align, HEADERS_TOTAL_SIZE)) {
		tegrabl_dealloc_dram_address(addr);
		err = TEGRABL_ERR_NO_MEMORY;
		goto exit;
	}
	err = tegrabl_partition_read(&partition, (void *)addr_align, HEADERS_TOTAL_SIZE);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to read bootimage partition\n");
		goto exit;
	}
	ptr = addr_align;
	if ((strncmp((const char *)ptr, "GSHV", 4) == 0)
		|| (strncmp((const char *)ptr, "NVDA", 4) == 0)) {
		ptr += sizeof(struct tegrabl_sigheader);
	}
	if (strncmp((const char *)ptr, "ANDROID", 7) == 0) {
		/*
		 * Get the total size of kernel part and ramdisk part from
		 * "tegrabl_bootimg_header" structure
		 * For kernel only case or uboot only case, the ramdisk
		 * size is 0
		 */
		bootimg_header = (union tegrabl_bootimg_header *)ptr;
		bootimg_size += bootimg_header->kernelsize;
		bootimg_size += bootimg_header->ramdisksize;
		pr_info("Boot image size read from image header: %lx\n", bootimg_size);
	}
	tegrabl_partition_close(&partition);
	if (bootimg_size > 0) {
		loading_total_size = HEADERS_TOTAL_SIZE + bootimg_size;
		tegrabl_dealloc_dram_address(addr);
		addr = tegrabl_get_free_dram_address(loading_total_size + MEM_SZ_2MB);
		addr_align = (void *)MEM_ALIGN(addr, MEM_SZ_2MB);
		if (!U_BOOT_LOAD_ADDRESS_IS_VALID((uint64_t)addr_align, loading_total_size)) {
			tegrabl_dealloc_dram_address(addr);
			err = TEGRABL_ERR_NO_MEMORY;
			goto exit;
		}
	}
	*load_addr = addr_align;
	pr_info("Boot image load address: %p\n", addr_align);
exit:
	return err;
}

uint64_t tegrabl_get_kernel_load_addr(void)
{
	uint64_t addr;
	addr = tegrabl_get_free_dram_address(MAX_KERNEL_IMAGE_SIZE + MEM_SZ_2MB);
	return MEM_ALIGN(addr, MEM_SZ_2MB);
}

uint64_t tegrabl_get_dtb_load_addr(void)
{
	uint64_t addr;
	addr = tegrabl_get_free_dram_address(DTB_MAX_SIZE + MEM_SZ_2MB);
	return MEM_ALIGN(addr, MEM_SZ_2MB);
}

uint64_t tegrabl_get_ramdisk_load_addr(void)
{
	uint64_t addr;
	addr = tegrabl_get_free_dram_address(RAMDISK_MAX_SIZE + MEM_SZ_64KB);
	return MEM_ALIGN(addr, MEM_SZ_64KB);
}

#else /* CONFIG_DYNAMIC_LOAD_ADDRESS */

tegrabl_error_t tegrabl_get_nct_load_addr(void **load_addr)
{
	*load_addr = (void *)NCT_PART_LOAD_ADDRESS;
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_get_boot_img_load_addr(void **load_addr)
{
	*load_addr = (void *)BOOT_IMAGE_LOAD_ADDRESS;
	return TEGRABL_NO_ERROR;
}

uint64_t tegrabl_get_kernel_load_addr(void)
{
	return LINUX_LOAD_ADDRESS;
}

uint64_t tegrabl_get_dtb_load_addr(void)
{
	return DTB_LOAD_ADDRESS;
}

uint64_t tegrabl_get_ramdisk_load_addr(void)
{
	return RAMDISK_ADDRESS;
}
#endif /* CONFIG_DYNAMIC_LOAD_ADDRESS */

#if defined(CONFIG_ENABLE_L4T_RECOVERY)
tegrabl_error_t tegrabl_get_recovery_img_load_addr(void **load_addr)
{
	*load_addr = (void *)BOOT_IMAGE_LOAD_ADDRESS;
	return TEGRABL_NO_ERROR;
}
#endif

uint64_t tegrabl_get_kernel_text_offset(void)
{
	return 0;
}

bool tegrabl_do_ratchet_check(uint8_t bin_type, void * const addr)
{
	TEGRABL_UNUSED(bin_type);
	TEGRABL_UNUSED(addr);
	return true;
}
