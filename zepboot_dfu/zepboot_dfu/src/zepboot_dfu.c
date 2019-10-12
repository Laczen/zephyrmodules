/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>

#include <zephyr/types.h>
#include <misc/byteorder.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include <power/reboot.h>

#include <zb_flash.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(zepboot_dfu);

#include "../include/zepboot_dfu.h"
#include "../include/nrf_dfu.h"

/* 23A07C4E-9E3B-4379-BD0D-2B03381274A4 */
static struct bt_uuid_128 zepboot_dfu_uuid = BT_UUID_INIT_128(
	0xA4, 0x74, 0x12, 0x38, 0x03, 0x2B, 0x0D, 0xBD,
	0x79, 0x43, 0x3B, 0x9E, 0x4E, 0x7C, 0xA0, 0x23
);

#if !IS_ENABLED(CONFIG_ZEPBOOT_DFU_RESET_ONLY)
static struct bt_uuid_128 zepboot_dfu_control = BT_UUID_INIT_128(
	0xA4, 0x74, 0x12, 0x38, 0x03, 0x2B, 0x0D, 0xBD,
	0x79, 0x43, 0x3B, 0x9E, 0x4E, 0x7C, 0xA0, 0x23
);
#endif

static struct bt_uuid_128 zepboot_dfu_reboot = BT_UUID_INIT_128(
	0xA4, 0x74, 0x12, 0x38, 0x03, 0x2B, 0x0D, 0xBD,
	0x79, 0x43, 0x3B, 0x9E, 0x4E, 0x7C, 0xA0, 0x23
);

static struct k_delayed_work delayed_reboot;
static void do_reboot(struct k_work *work) {
	sys_reboot(0);
}

static void schedule_reboot(void)
{
	k_delayed_work_init(&delayed_reboot, do_reboot);
	k_delayed_work_submit(&delayed_reboot, 1000U);
	LOG_INF("Doing a reboot");
}

/* Characteristic for reboot
 * Write 0: reboot
 * Write 1: boot image in slot 0
 * Write 2: boot image in slot 1 (if applicable)
 */
static ssize_t write_reboot(struct bt_conn *conn,
	const struct bt_gatt_attr *attr, const void *buf, u16_t len,
	u16_t offset, u8_t flags)
{
	u8_t tmp;
	struct zb_cmd cmd = {
		.cmd2 = 0U,
		.cmd3 = 0U,
	};
	struct zb_slt_area area;

	if (offset + len > sizeof(tmp)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(&tmp, buf, len);

	if (tmp > 2) {
		return -BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	zb_slt_area_get(&area, 0);

	switch (tmp) {
	case 1:
		cmd.cmd1 = 0x20;
		if (zb_cmd_write_slt1end(&area, &cmd)) {
			zb_erase_slt1end(&area);
			zb_cmd_write_slt1end(&area, &cmd);
		}
		break;
	case 2:
		cmd.cmd1 = 0x00;
		if (zb_cmd_write_slt1end(&area, &cmd)) {
			zb_erase_slt1end(&area);
		}
		break;
	}
	schedule_reboot();
	return len;
}

#if !IS_ENABLED(CONFIG_ZEPBOOT_DFU_RESET_ONLY)
/* mode describes the update that will be performed:
 *  the first nibble describes the update mode:
 *    0: permanent swap
 *    1: temporary swap
 *    2: make a temporary swap permanent
 *  the second nibble describes the image area where the upload will be done
 *    0: image area 0
 *    1: image area 1
 *       ...
 *  the default value is 0x00 so that it does a permanent swap for image area 0,
 *  this should be the correct value for most cases.
 */
static u8_t mode = 0x0;

extern unsigned int slot_map_cnt;
extern struct slt_area slot_map[];

static ssize_t read_mode(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			void *buf, u16_t len, u16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &mode,
				 sizeof(mode));
}

static ssize_t write_mode(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 const void *buf, u16_t len, u16_t offset,
			 u8_t flags)
{
	u8_t tmp_mode;

	if (offset + len > sizeof(mode)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(&tmp_mode, buf, len);

	if ((tmp_mode & 0x0F) > (slot_map_cnt - 1)) {
		return -BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	if ((tmp_mode & 0xF0) > 0x20) {
		return -BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	mode = tmp_mode;
	return len;
}
#endif

BT_GATT_SERVICE_DEFINE(zepboot_dfu_svc,
	BT_GATT_PRIMARY_SERVICE(&zepboot_dfu_uuid),
	BT_GATT_CHARACTERISTIC(&zepboot_dfu_reboot.uuid,
			       BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_WRITE,
			       NULL, write_reboot, NULL),
#if !IS_ENABLED(CONFIG_ZEPBOOT_DFU_RESET_ONLY)
	BT_GATT_CHARACTERISTIC(&zepboot_dfu_control.uuid,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE |
			       BT_GATT_CHRC_INDICATE,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       read_mode, write_mode, NULL),
#endif

);

#if !IS_ENABLED(CONFIG_ZEPBOOT_DFU_RESET_ONLY)
int init_flash_img_ctx(struct flash_img_ctx *ctx)
{
	LOG_INF("Init called");
	memset(&ctx->buf, 0xff, CONFIG_ZEPBOOT_DFU_BLOCK_BUF_SIZE);
	ctx->buf_offset = 0;
	ctx->wr_offset = 0;
	ctx->fl_dev = device_get_binding(slot_map[mode & 0x0F].slt1_devname);
	return 0;
}

int flush_flash_img_ctx(struct flash_img_ctx *ctx)
{
	int rc;
	off_t write_offset = slot_map[mode & 0x0F].slt1_offset;

	if (!ctx->fl_dev) {
		LOG_INF("No flash device");
		return 0;
	}

	if (ctx->wr_offset >= slot_map[mode & 0x0F].slt1_size) {
		LOG_INF("Wr offset error");
		return 0;
	}

	write_offset += ctx->wr_offset;
	if ((ctx->wr_offset % CONFIG_ZEPBOOT_SECTOR_SIZE) == 0) {
		LOG_INF("Flash erase at %x", write_offset);

		rc = zb_flash_erase(ctx->fl_dev, write_offset,
			CONFIG_ZEPBOOT_SECTOR_SIZE);
		if (rc) {
			/* flash erase error */
			return rc;
		}
	}
	LOG_INF("Flash Write at %x len %d", write_offset, ctx->buf_offset);
	rc = zb_flash_write(ctx->fl_dev, write_offset, ctx->buf,
		CONFIG_ZEPBOOT_DFU_BLOCK_BUF_SIZE);
	ctx->wr_offset += ctx->buf_offset;
	ctx->buf_offset = 0;
	memset(&ctx->buf, 0xff, CONFIG_ZEPBOOT_DFU_BLOCK_BUF_SIZE);
	return rc;
}

struct flash_img_ctx ctx;

int receive(u32_t offset, const u8_t *data, size_t len)
{
	size_t plen;

	if (!offset) {
		init_flash_img_ctx(&ctx);
	}

	if (ctx.buf_offset + len >= CONFIG_ZEPBOOT_DFU_BLOCK_BUF_SIZE) {
		plen = CONFIG_ZEPBOOT_DFU_BLOCK_BUF_SIZE - ctx.buf_offset;
		memcpy(&ctx.buf[ctx.buf_offset], data, plen);
		data += plen;
		ctx.buf_offset += plen;
		len -= plen;
		flush_flash_img_ctx(&ctx);
	}

	if (len) {
		memcpy(&ctx.buf[ctx.buf_offset], data, len);
		ctx.buf_offset +=len;
	}
	return 0;
}

void finished(void)
{
	struct zb_cmd cmd = {
		.cmd2 = 0U,
		.cmd3 = 0U,
	};
	struct zb_slt_area area;

	if (ctx.buf_offset) {
		flush_flash_img_ctx(&ctx);
	}

	switch (mode & 0xF0) {
	case 0x00:
		cmd.cmd1 = 0x11;
		break;
	case 0x10:
		cmd.cmd1 = 0x10;
		break;
	case 0x20:
		cmd.cmd1 = 0x01;
		break;
	default:
		cmd.cmd1 = 0x00;
	}

	if (cmd.cmd1) {
		zb_slt_area_get(&area, (mode & 0x0f));
		if (zb_cmd_write_slt1end(&area, &cmd)) {
			zb_erase_slt1end(&area);
			zb_cmd_write_slt1end(&area, &cmd);
		}
	}
	LOG_INF("Finished");
}

#endif

struct nrf_dfu_cb dfu_callbacks = {
#if !IS_ENABLED(CONFIG_ZEPBOOT_DFU_RESET_ONLY)
	.receive = &receive,
	.reset_to_app = &finished,
#endif
};

void zepboot_dfu_init(void)
{
	nrf_dfu_init(&dfu_callbacks, 0U, 0U);
}
