/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zb8/zb8_flash.h>

#include <drivers/flash.h>
#include <sys/crc.h>
#include <errno.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(zb8_flash);

extern const struct zb_slt_area slotmap[];
extern const unsigned int slotmap_cnt;

u32_t zb_align_up(struct zb_slt_info *info, size_t len)
{
	u8_t write_block_size;

	if (info->fl_dev) {
		write_block_size = flash_get_write_block_size(info->fl_dev);
	} else { /* No fl_dev is RAM */
		write_block_size = 1U;
	}
	if (write_block_size <= 1) {
		return len;
	}
	return (len + (write_block_size - 1)) & ~(write_block_size - 1);
}

u32_t zb_align_down(struct zb_slt_info *info, size_t len)
{
	u8_t write_block_size;

	if (info->fl_dev) {
		write_block_size = flash_get_write_block_size(info->fl_dev);
	} else { /* No fl_dev is RAM */
		write_block_size = 1U;
	}
	write_block_size = flash_get_write_block_size(info->fl_dev);
	if (write_block_size <= 1) {
		return len;
	}
	return len & ~(write_block_size - 1);
}

int zb_erase(struct zb_slt_info *info, u32_t offset, size_t len)
{
	int rc;

	LOG_DBG("Erasing [%zu] bytes at [%x]", len, offset);

	if (!len) {
		return 0;
	}

	if ((offset + len) > info->size) {
		return -EINVAL;
	}

	offset += info->offset;
	if (!info->fl_dev) { /* No fl_dev is RAM */
		memset(&offset, 0xff, len);
		return 0;
	}

	rc = flash_write_protection_set(info->fl_dev, 0);
	if (rc) {
		/* flash protection set error */
		return rc;
	}
	rc = flash_erase(info->fl_dev, (off_t)offset, len);
	if (rc) {
		/* flash erase error */
		return rc;
	}
	(void) flash_write_protection_set(info->fl_dev, 1);
	return 0;

}

int zb_write(struct zb_slt_info *info, u32_t offset, const void *data,
	     size_t len)
{
	const u8_t *data8 = (const u8_t *)data;
	u8_t buf[ALIGN_BUF_SIZE];
	u8_t wbs;
	size_t blen;
	int rc = 0;

	LOG_DBG("Writing [%zu] bytes at [%x]", len, offset);
	if ((offset + len) > info->size) {
		return -EINVAL;
	}

	offset += info->offset;
	if (!info->fl_dev) { /* No device is equal to RAM */
		memcpy(&offset, data, len);
		return 0;
	}

	rc = flash_write_protection_set(info->fl_dev, 0);
	if (rc) {
		/* flash protection set error */
		return rc;
	}

	blen = zb_align_down(info, len);

	if (blen > 0) {
		rc = flash_write(info->fl_dev, (off_t)offset, data8, blen);
		if (rc) {
			/* flash write error */
			goto end;
		}
		offset += blen;
		data8 += blen;
		len -= blen;
	}
	if (len) {
		wbs = flash_get_write_block_size(info->fl_dev);
		(void)memset(buf, 0xff, wbs);
		memcpy(buf, data8, len);
		rc = flash_write(info->fl_dev, (off_t)offset, buf, (size_t)wbs);
		if (rc) {
			/* flash write error */
			goto end;
		}
	}
end:
	(void) flash_write_protection_set(info->fl_dev, 1);
	return rc;
}

int zb_read(struct zb_slt_info *info, u32_t offset, void *data, size_t len)
{
	LOG_DBG("Reading [%zu] bytes at [%x]", len, offset);
	if ((offset + len) > info->size) {
		return -EINVAL;
	}

	offset += info->offset;
	if (!info->fl_dev) { /* No device is equal to RAM */
		memcpy(data, &offset, len);
		return 0;
	}
	return flash_read(info->fl_dev, (off_t)offset, data, len);
}

bool zb_empty_slot(struct zb_slt_info *info)
{
	u32_t buf;
	u32_t offset = 0U;

	while (offset < info->size) {
		(void)flash_read(info->fl_dev, (off_t)(offset + info->offset),
				 &buf, sizeof(buf));
		if (buf != EMPTY_U32) {
			return false;
		}
		offset += sizeof(buf);
	}
	return true;
}

/* crc8 calculation and verification in one routine:
 * to update the crc8: (void) zb_cmd_crc8(cmd)
 * to check the crc8: zb_cmd_crc8(cmd) returns 0 if ok
 * !!!! the routine always updates the cmd->crc8 !!!!
 */
static int zb_cmd_crc8(struct zb_cmd *cmd)
{
	u8_t crc8;

	crc8=cmd->crc8;
	cmd->crc8 = crc8_ccitt(0xff, cmd, offsetof(struct zb_cmd, crc8));
	if (cmd->crc8 == crc8) {
		return 0;
	}
	return 1;
}

int zb_cmd_read(struct zb_slt_info *info, struct zb_cmd *cmd)
{
	int rc;
	struct zb_cmd re_cmd;
	bool found = false;
	u32_t off, re_cmd_u32;

	LOG_DBG("CMD Read");
	off = 0;

	while (1) {
		rc= zb_read(info, off, &re_cmd, sizeof(struct zb_cmd));
		if (rc) {
			break;
		}

		memcpy(&re_cmd_u32, &re_cmd, 4);
		if (re_cmd_u32 == EMPTY_U32) {
			if (found) {
				rc = 0;
			} else {
				*cmd = re_cmd;
				rc = -ENOENT;
			}
			break;
		}

		if (!zb_cmd_crc8(&re_cmd)) {
			*cmd = re_cmd;
			found = true;
		}

		off += zb_align_up(info, sizeof(struct zb_cmd));
		if (off >= info->size) {
			if (found) {
				rc = 0;
			} else {
				rc = -ENOENT;
			}
			break;
		}
	}

	return rc;
}

int zb_cmd_write(struct zb_slt_info *info, struct zb_cmd *cmd)
{
	int rc;
	struct zb_cmd re_cmd;
	u32_t off, re_cmd_u32;

	LOG_DBG("CMD Write");
	off = 0;

	while (1) {
		rc= zb_read(info, off, &re_cmd, sizeof(struct zb_cmd));
		if (rc) {
			break;
		}
		memcpy(&re_cmd_u32, &re_cmd, 4);
		if (re_cmd_u32 == EMPTY_U32) {
			rc = 0;
			break;
		}

		off += zb_align_up(info, sizeof(struct zb_cmd));
		if (off >= info->size) {
			rc = -ENOSPC;
			break;
		}
	}

	if (!rc) {
		(void) zb_cmd_crc8(cmd);
		rc = zb_write(info, off, cmd, sizeof(struct zb_cmd));
	}

	return rc;
}