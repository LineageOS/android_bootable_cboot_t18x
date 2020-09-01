/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <nvcommon.h>
#include <vic/dev_vic_csb.h>
#include <vic/vic_enums.h>
#include <vic/dev_vic.h>
#include <vic/vic_fce_ucode.h>
#include <tegrabl_vic.h>
#include <tegrabl_timer.h>
#include <tegrabl_clock.h>
#include <tegrabl_io.h>

#define CB_VIC_PRIV_WR(off, data)	\
	NV_WRITE32((NV_ADDRESS_MAP_VIC_BASE + off), (data))

static void cb_vic_priv_write_extended(uint32_t adr, uint32_t data)
{
	/*
	 * Need to write LSB values into the PVIC_FALCON_ADR if addr is not 256 byte
	 * aligned.For VIC one priv register is mapping to multiple csb register
	 * FALCON_ADDR are used to select which csb register we are accessing, so
	 * we need set FALCON_ADDR first, then access register
	*/

	if ((adr & 0xff) != 0) {
		CB_VIC_PRIV_WR(NV_PVIC_FALCON_ADDR, (adr & 0xff) >> 2);
	}

	CB_VIC_PRIV_WR(0x1000 + (adr >> 6), data);

	if ((adr & 0xff) != 0) {
		CB_VIC_PRIV_WR(NV_PVIC_FALCON_ADDR, 0);
	}
}

static void cb_vic_fw_load(void)
{
	uint32_t count;

	/* Program FCE ucode - needs to be done once after vic is reset */
	for (count = 0; count < sizeof(fce_ucode_data_vic) / sizeof(uint32_t);
			count++) {
		cb_vic_priv_write_extended(NV_CVIC_FC_FCE_UCODE_ADDR, (count * 4));
		cb_vic_priv_write_extended(NV_CVIC_FC_FCE_UCODE_INST,
						fce_ucode_data_vic[count]);
	}
	pr_debug("cb_vic_fw_load successfull\n");
}

static void cb_vic_boot(void)
{
	cb_vic_fw_load();

	cb_vic_priv_write_extended(NV_CVIC_FC_FCE_CTRL,
			VIC_DRF_DEF(_CVIC_FC, _FCE_CTRL, _START, _TRIGGER));
	pr_debug("cb_vic_boot Successfull\n");
}

static tegrabl_error_t cb_vic_copy(uint64_t dst, uint64_t src, uint32_t size)
{
	uint32_t width;
	uint32_t height;
	uint32_t block_height;
	uint32_t size_alignment;

	/*
	 * Size = (Width * Height)
	 * Width = (n * 64B) and Height = (m * X) (Refer http://nvbugs/200223979)
	 * Size = (n * 64B) * (m * X)  or
	 * Size = (n * 16 Pixels) * (m * X) as bytes per pixel is 4
	*/
	if (size >= SIZE_1M) {
		/*
		 * X = 8 * 2 ^ _BLK_HEIGHT, with _BLK_HEIGHT as 4, we have X = 128 in
		 * this case m value is assumed to derive Width, m is assumed same as X
		 * So Height comes as 16K Bytes (m * X)
		 * Width = Size / Height, Width must also be aligned to 16 Bytes
		 */
		height = VIC_SCRUB_HEIGHT1;
		size_alignment = VIC_SIZE_ALIGN_MASK1;
		block_height = VIC_BLOCK_HEIGHT1;
	} else {
		/*
		 * X = 8 * 2 ^ _BLK_HEIGHT, with _BLK_HEIGHT as 0, we have X = 8 in this
		 * case m value is assumed to derive Width, m is assumed same as X
		 * So Height comes as 64 Bytes (m * X)
		 * Width = Size / Height, Width must also be aligned to 16 Bytes
		*/
		height = VIC_SCRUB_HEIGHT2;
		size_alignment = VIC_SIZE_ALIGN_MASK2;
		block_height = VIC_BLOCK_HEIGHT2;
	}

	/*
	 * Max size, Min Size and size alignment are restricted by Height, Width
	 * requirements described above
	*/
	if ((size > VIC_SRUB_SIZE_MAX) || (size < VIC_SRUB_SIZE_MIN) ||
		(size & size_alignment)) {
		pr_error("%s Size %d not valid,Max = 0x%x,Min = 0x%x or not aligned ",
				__func__, size, VIC_SRUB_SIZE_MAX, VIC_SRUB_SIZE_MIN);
		pr_error("%d bytes\n", (size_alignment + 1));
		return TEGRABL_ERR_BAD_PARAMETER;
	}

	/*
	 * Width and Height need to be programmed in Pixels
	 * Width_in_pixels = (Size_in_Pixels) / (Height_in_Pixel_rows)
	 * Size_in_pixels = (Size / CB_VIC_BYTES_PER_PIXEL)
	*/
	width  = ((size / CB_VIC_BYTES_PER_PIXEL) / height);

	if (width % 16) {
		/* Width must also be aligned to 16 Pixels as Width = n * 16 Pixels */
		pr_error("%s Width %d not aligned to 16 bytes\n", __func__, width);
		return TEGRABL_ERR_BAD_ADDRESS;
	}

	CB_VIC_DBG("%s:Width = 0x%x, Height = 0x%x, Src = 0x%llx, Dst = 0x%llx\n",
			   __func__, width, height, src, dst);

	cb_vic_priv_write_extended(NV_CVIC_SC_SFC0_BASE_LUMA(0), src >> 8);

	cb_vic_priv_write_extended(NV_CVIC_FC_CFG_STRUCT_SLOT_INDEX, 0);

	cb_vic_priv_write_extended(NV_CVIC_FC_CFG_STRUCT_SLOT_CFG0,
						VIC_DRF_DEF(_CVIC_FC, _CFG_STRUCT_SLOT_CFG0,
									_SLOT, _ENABLE) |
						VIC_DRF_DEF(_CVIC_FC, _CFG_STRUCT_SLOT_CFG0,
									_FIELD_CURRENT, _ENABLE));

	cb_vic_priv_write_extended(NV_CVIC_FC_CFG_STRUCT_SLOT_CFG2,
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_SLOT_CFG2,
									_PIXEL_FORMAT, T_A8R8G8B8) |
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_SLOT_CFG2,
									_BLK_KIND, BLK_KIND_GENERIC_16Bx2) |
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_SLOT_CFG2,
									_BLK_HEIGHT, block_height) |
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_SLOT_CFG2,
									_CACHE_WIDTH, 0));

	cb_vic_priv_write_extended(NV_CVIC_FC_CFG_STRUCT_SLOT_SFC_SIZE,
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_SLOT_SFC_SIZE,
									_WIDTH, width - 1) |
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_SLOT_SFC_SIZE,
									_HEIGHT, height - 1));

	cb_vic_priv_write_extended(NV_CVIC_FC_CFG_STRUCT_SLOT_LUMA_SIZE,
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_SLOT_LUMA_SIZE,
									_WIDTH, width - 1) |
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_SLOT_LUMA_SIZE,
									_HEIGHT, height - 1));

	cb_vic_priv_write_extended(NV_CVIC_FC_CFG_STRUCT_SLOT_SRC_RECT_LEFT,
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_SLOT_SRC_RECT_LEFT,
									_VAL, 0));

	cb_vic_priv_write_extended(NV_CVIC_FC_CFG_STRUCT_SLOT_SRC_RECT_RIGHT,
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_SLOT_SRC_RECT_RIGHT,
									_VAL, (width - 1) << 16));

	cb_vic_priv_write_extended(NV_CVIC_FC_CFG_STRUCT_SLOT_SRC_RECT_TOP,
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_SLOT_SRC_RECT_TOP,
									_VAL, 0));

	cb_vic_priv_write_extended(NV_CVIC_FC_CFG_STRUCT_SLOT_SRC_RECT_BOTTOM,
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_SLOT_SRC_RECT_BOTTOM,
									_VAL, (height - 1) << 16));

	cb_vic_priv_write_extended(NV_CVIC_FC_CFG_STRUCT_SLOT_DST_RECT_LR,
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_SLOT_DST_RECT_LR,
									_LEFT, 0) |
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_SLOT_DST_RECT_LR,
									_RIGHT, width - 1));

	cb_vic_priv_write_extended(NV_CVIC_FC_CFG_STRUCT_SLOT_DST_RECT_TB,
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_SLOT_DST_RECT_TB,
									_TOP, 0) |
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_SLOT_DST_RECT_TB,
									_BOTTOM, height - 1));

	cb_vic_priv_write_extended(NV_CVIC_FC_SLOT_MAP(0), 0xfffffff0);
	cb_vic_priv_write_extended(NV_CVIC_FC_SLOT_MAP(1), 0xffffffff);

	cb_vic_priv_write_extended(NV_CVIC_FC_CFG_STRUCT_TGT_RECT_LR,
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_TGT_RECT_LR,
									_LEFT, 0) |
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_TGT_RECT_LR,
									_RIGHT, width-1));

	cb_vic_priv_write_extended(NV_CVIC_FC_CFG_STRUCT_TGT_RECT_TB,
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_TGT_RECT_TB,
									_TOP, 0) |
						VIC_DRF_NUM(_CVIC_FC, _CFG_STRUCT_TGT_RECT_TB,
									_BOTTOM, height - 1));

	cb_vic_priv_write_extended(NV_CVIC_SL_CFG_STRUCT_SLOT_INDEX, 0);
	cb_vic_priv_write_extended(NV_CVIC_SL_CFG_STRUCT_SLOT_CFG0,
						VIC_DRF_DEF(_CVIC_SL, _CFG_STRUCT_SLOT_CFG0,
									_SLOT, _ENABLE));

	cb_vic_priv_write_extended(NV_CVIC_SL_CFG_STRUCT_SLOT_DST_RECT_LR,
						VIC_DRF_NUM(_CVIC_SL, _CFG_STRUCT_SLOT_DST_RECT_LR,
									_LEFT, 0) |
						VIC_DRF_NUM(_CVIC_SL, _CFG_STRUCT_SLOT_DST_RECT_LR,
									_RIGHT, width - 1));

	cb_vic_priv_write_extended(NV_CVIC_SL_CFG_STRUCT_SLOT_DST_RECT_TB,
						VIC_DRF_NUM(_CVIC_SL, _CFG_STRUCT_SLOT_DST_RECT_TB,
									_TOP, 0) |
						VIC_DRF_NUM(_CVIC_SL, _CFG_STRUCT_SLOT_DST_RECT_TB,
									_BOTTOM, height - 1));

	cb_vic_priv_write_extended(NV_CVIC_SL_CFG_STRUCT_TGT_RECT_LR,
						VIC_DRF_NUM(_CVIC_SL, _CFG_STRUCT_TGT_RECT_LR,
									_LEFT, 0) |
						VIC_DRF_NUM(_CVIC_SL, _CFG_STRUCT_TGT_RECT_LR,
									_RIGHT, width - 1));

	cb_vic_priv_write_extended(NV_CVIC_SL_CFG_STRUCT_TGT_RECT_TB,
						VIC_DRF_NUM(_CVIC_SL, _CFG_STRUCT_TGT_RECT_TB,
									_TOP, 0) |
						VIC_DRF_NUM(_CVIC_SL, _CFG_STRUCT_TGT_RECT_TB,
									_BOTTOM, height - 1));

	cb_vic_priv_write_extended(NV_CVIC_BL_TARGET_BASADR, dst >> 8);

	cb_vic_priv_write_extended(NV_CVIC_BL_CFG_STRUCT_CFG0,
						VIC_DRF_NUM(_CVIC_BL, _CFG_STRUCT_CFG0,
									_PIXEL_FORMAT, T_A8R8G8B8) |
						VIC_DRF_NUM(_CVIC_BL, _CFG_STRUCT_CFG0,
									_BLK_KIND, BLK_KIND_GENERIC_16Bx2) |
						VIC_DRF_NUM(_CVIC_BL, _CFG_STRUCT_CFG0,
									_BLK_HEIGHT, block_height));

	cb_vic_priv_write_extended(NV_CVIC_BL_CFG_STRUCT_SFC_SIZE,
						VIC_DRF_NUM(_CVIC_BL, _CFG_STRUCT_SFC_SIZE,
									_WIDTH, width - 1) |
						VIC_DRF_NUM(_CVIC_BL, _CFG_STRUCT_SFC_SIZE,
									_HEIGHT, height - 1));

	cb_vic_priv_write_extended(NV_CVIC_BL_CFG_STRUCT_LUMA_SIZE,
						VIC_DRF_NUM(_CVIC_BL, _CFG_STRUCT_LUMA_SIZE,
									_WIDTH, width - 1) |
						VIC_DRF_NUM(_CVIC_BL, _CFG_STRUCT_LUMA_SIZE,
									_HEIGHT, height - 1));

	cb_vic_priv_write_extended(NV_CVIC_BL_CFG_STRUCT_TGT_RECT_LR,
						VIC_DRF_NUM(_CVIC_BL, _CFG_STRUCT_TGT_RECT_LR,
									_LEFT, 0) |
						VIC_DRF_NUM(_CVIC_BL, _CFG_STRUCT_TGT_RECT_LR,
									_RIGHT, width - 1));

	cb_vic_priv_write_extended(NV_CVIC_BL_CFG_STRUCT_TGT_RECT_TB,
						VIC_DRF_NUM(_CVIC_BL, _CFG_STRUCT_TGT_RECT_TB,
									_TOP, 0) |
						VIC_DRF_NUM(_CVIC_BL, _CFG_STRUCT_TGT_RECT_TB,
									_BOTTOM, height - 1));

	/* Init value of bl_config with trigger enabled */
	cb_vic_priv_write_extended(NV_CVIC_BL_CONFIG, CB_CVIC_BL_CONFIG);
	cb_vic_priv_write_extended(NV_CVIC_FC_COMPOSE, CB_CVIC_FC_COMPOSE);

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t cb_dram_ecc_vic_scrub_wait_for_complete(void)
{
	uint32_t timeout_count = 0;

	/*
	 * Incase of ASYNC transfer, while we come back for checking transfer
	 * complete, it is possible that trasnfer is already complete, for which
	 * we don't have to wait. So wait only based on transfer status
	*/
	while (NV_READ32(NV_ADDRESS_MAP_VIC_BASE + NV_PVIC_FALCON_IDLESTATE) != 0) {
		tegrabl_udelay(50);
		timeout_count++;
		if (timeout_count >= VIC_POLL_DELAY_COUNT) {
			return TEGRABL_ERR_TIMEOUT;
		}
	};

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t cb_vic_scrub(uint32_t instance, uint32_t cmd, void *p_buf)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	TEGRABL_ASSERT(instance < MAX_VIC_CONTROLLERS);

	switch (cmd) {
		/*
		 * * Cache operations at src/dest as required need to be taken care
		 * * outside this ioctl
		*/
	case CB_VIC_TRANSFER: {
		TEGRABL_ASSERT(p_buf);

		struct vic_transfer_config *p_vic_transfer_config =
			(struct vic_transfer_config *)p_buf;

		err = cb_vic_copy(p_vic_transfer_config->dest_addr_phy,
				p_vic_transfer_config->src_addr_phy,
				p_vic_transfer_config->size);

		if (err != TEGRABL_NO_ERROR)
			goto fail;
		break;
	}
	case CB_VIC_WAIT_FOR_TRANSFER_COMPLETE: {
		err = cb_dram_ecc_vic_scrub_wait_for_complete();

		if (err != TEGRABL_NO_ERROR)
			goto fail;
		break;
	}
	default:
			err = TEGRABL_ERR_BAD_PARAMETER;
	}
fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: VIC Scrub Failure, error 0x%x, cmd = 0x%x\n",
				 __func__, err, cmd);
		/* Boot Shall be aborted when there is a failure in scrubbing */
		pr_error("Bug %s:%d (%s)\n", __FILE__, __LINE__, __func__);
		while (true) {
			/* Reboot */
		}
	}
	return err;
}

static tegrabl_error_t cb_vic_clock_enable(void)
{
	uint32_t vic_clk_set;
	uint32_t host1x_clk_set;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	err = tegrabl_car_rst_clear(TEGRABL_MODULE_HOST1X, 0);
	if (err != TEGRABL_NO_ERROR)
		goto fail;

	err = tegrabl_car_clk_enable(TEGRABL_MODULE_HOST1X, 0, NULL);
	if (err != TEGRABL_NO_ERROR)
		goto fail;

	err = tegrabl_car_set_clk_rate(TEGRABL_MODULE_HOST1X, 0,
			VIC_CLK_FREQUENCY_VAL, &host1x_clk_set);
	if (err != TEGRABL_NO_ERROR)
		goto fail;

	err = tegrabl_car_rst_clear(TEGRABL_MODULE_VIC, 0);
	if (err != TEGRABL_NO_ERROR)
		goto fail;

	err = tegrabl_car_clk_enable(TEGRABL_MODULE_VIC, 0, NULL);
	if (err != TEGRABL_NO_ERROR)
		goto fail;

	err = tegrabl_car_set_clk_rate(TEGRABL_MODULE_VIC, 0,
			VIC_CLK_FREQUENCY_VAL, &vic_clk_set);
	if (err != TEGRABL_NO_ERROR)
		goto fail;
	pr_debug("VIC clock set to %uKHz\n", vic_clk_set);

fail:
	if (err != TEGRABL_NO_ERROR)
		pr_error("%s: VIC clock enable failure, error %x\n", __func__, err);
	return err;
}

tegrabl_error_t cb_vic_init(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	/* Enable Clock for VIC FC */
	err = cb_vic_clock_enable();
	if (err != TEGRABL_NO_ERROR)
		return err;

	/* Load VIC Code at the VIC base address */
	cb_vic_boot();

	pr_debug("VIC FW Initialized\n");
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t cb_vic_exit(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_car_rst_set(TEGRABL_MODULE_VIC, 0);
	if (err != TEGRABL_NO_ERROR)
		goto fail;

	err = tegrabl_car_clk_disable(TEGRABL_MODULE_VIC, 0);
	if (err != TEGRABL_NO_ERROR)
		goto fail;

	err = tegrabl_car_rst_set(TEGRABL_MODULE_HOST1X, 0);
	if (err != TEGRABL_NO_ERROR)
		goto fail;

	err = tegrabl_car_clk_disable(TEGRABL_MODULE_HOST1X, 0);
	if (err != TEGRABL_NO_ERROR)
		goto fail;

	pr_debug("VIC FC closed\n");
fail:
	if (err != TEGRABL_NO_ERROR)
		pr_error("%s: VIC clock Disable Failure, error %x\n", __func__, err);
	return err;
}
