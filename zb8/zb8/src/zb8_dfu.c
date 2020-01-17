/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <zb8/zb8_dfu.h>
#include <zb8/zb8_fsl.h>
#include <zb8/zb8_flash.h>
#include <zb8/zb8_slot.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(zb8_dfu);

extern u8_t slotmap_cnt;
extern struct zb_slt_area slot_map[];

static struct flash_img_ctx {
	u8_t sm_idx; /* slotmap identifier */
	u8_t buf[CONFIG_ZB_EIGHT_DFU_BLOCK_BUF_SIZE];
	u16_t buf_offset;
	u32_t wr_offset;
} ctx;

int init_flash_img_ctx(struct flash_img_ctx *ctx)
{
	ctx->buf_offset = 0;
	ctx->wr_offset = 0;
	memset(&ctx->buf, 0xff, CONFIG_ZB_EIGHT_DFU_BLOCK_BUF_SIZE);
	return 0;
}

int flush_flash_img_ctx(struct flash_img_ctx *ctx)
{
	int rc = 0;
	uint8_t cnt;
	uint32_t sectorsize;
	struct zb_fsl_hdr hdr;
	struct zb_slt_info upgrade, swpstat;

	if (!ctx->wr_offset) {
		bool slt_found = false;
		/* received the first buffer, check magic and find out where to
		 * put the image and if the size fits the slot.
		 */
		if (ctx->buf_offset < 32) {
			return -EFAULT;
		}

		memcpy((void *)&hdr, &ctx->buf[0], sizeof(struct zb_fsl_hdr));
		if (hdr.magic != FSL_MAGIC) {
			return -EFAULT;
		}
		cnt = zb_slt_area_cnt();
		while ((cnt--) > 0) {
			rc = zb_slt_open(&upgrade, cnt, UPGRADE);
			if (rc) {
				continue;
			}
			if (hdr.upload_offset == upgrade.offset) {
				slt_found = true;
				break;
			}
		}

		if (!slt_found) {
			LOG_INF("No valid slot found");
			return -EFAULT;
		}
		ctx->sm_idx = cnt;
		rc = zb_slt_open(&swpstat, ctx->sm_idx, SWPSTAT);
		if (rc) {
			return rc;
		}
		rc = zb_erase(&swpstat, 0U, swpstat.size);
		if (rc) {
			return rc;
		}
	}

	rc = zb_slt_open(&upgrade, ctx->sm_idx, UPGRADE);
	if (rc) {
		return rc;
	}

	sectorsize = zb_sectorsize_get(ctx->sm_idx);

	if ((ctx->wr_offset % sectorsize) == 0) {
	 	rc = zb_erase(&upgrade, ctx->wr_offset, sectorsize);
	 	if (rc) {
	 		/* flash erase error */
	 		return rc;
	 	}
	}
	rc = zb_write(&upgrade, ctx->wr_offset, ctx->buf,
		      CONFIG_ZB_EIGHT_DFU_BLOCK_BUF_SIZE);
	if (rc) {
		return rc;
	}
	ctx->wr_offset += ctx->buf_offset;
	ctx->buf_offset = 0;
	memset(&ctx->buf, 0xff, CONFIG_ZB_EIGHT_DFU_BLOCK_BUF_SIZE);
	return 0;
}

int zb_dfu_receive_start(void) {
	return 0;
}

int zb_dfu_receive(u32_t offset, const u8_t *data, size_t len)
{
	size_t plen;

	if (!offset) {
		init_flash_img_ctx(&ctx);
	}

	if (ctx.buf_offset + len >= CONFIG_ZB_EIGHT_DFU_BLOCK_BUF_SIZE) {
		plen = CONFIG_ZB_EIGHT_DFU_BLOCK_BUF_SIZE - ctx.buf_offset;
		memcpy(&ctx.buf[ctx.buf_offset], data, plen);
		data += plen;
		len -= plen;
		ctx.buf_offset += plen;
		if (flush_flash_img_ctx(&ctx)) {
			return -EFAULT;
		}
	}

	if (len) {
		memcpy(&ctx.buf[ctx.buf_offset], data, len);
		ctx.buf_offset +=len;
	}
	return 0;
}

void zb_dfu_receive_flush(void)
{
	if (ctx.buf_offset) {
		flush_flash_img_ctx(&ctx);
	}
}

int zb_dfu_confirm(u8_t sm_idx)
{
	struct zb_slt_info upgrade;
	struct zb_fsl_hdr hdr;
	struct zb_fsl_verify_hdr ver;
	int rc;

	rc = zb_slt_open(&upgrade, sm_idx, UPGRADE);
	if (rc) {
		return rc;
	}
	rc = zb_read(&upgrade, 0U, &hdr, sizeof(struct zb_fsl_hdr));
	if (rc) {
		return rc;
	}
	rc = zb_read(&upgrade, hdr.hdr_info.size, &ver,
		     sizeof(struct zb_fsl_verify_hdr));
	if (rc) {
		return rc;
	}

	if (ver.magic != FSL_VER_MAGIC) {
		ver.magic = FSL_VER_MAGIC;
		rc = zb_write(&upgrade, hdr.hdr_info.size, &ver,
			      sizeof(struct zb_fsl_verify_hdr));
	}
	return 0;
}

void zb_dfu_restart(void)
{
	zb_fsl_jump_fsl();
}

void zb_dfu_blstart(void)
{
	zb_fsl_jump_boot();
}