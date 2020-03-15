/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zb8/zb8_confirm.h>
#include <zb8/zb8_flash.h>
#include <zb8/zb8_slot.h>
#include <zb8/zb8_fsl.h>

#include <errno.h>
#include <sys/crc.h>

#include <logging/log.h>
LOG_MODULE_DECLARE(zb8, CONFIG_ZB_EIGHT_LOG_LEVEL);

static int zb_slt_crc32(u32_t *crc32, struct zb_slt_info *slt_info)
{
	int rc;
	struct zb_fsl_hdr hdr;
	u32_t size, off = 0;
	u8_t buf[32];

	rc = zb_read(slt_info, 0U, &hdr, sizeof(struct zb_fsl_hdr));
	if (rc) {
		return rc;
	}

	if (hdr.magic != FSL_MAGIC) {
		return -EFAULT;
	}

	size = hdr.hdr_info.size + hdr.size;
	while (size) {
		size_t rdlen = MIN(size, sizeof(buf));
		rc = zb_read(slt_info, off, &buf, rdlen);
		if (rc) {
			return rc;
		}
		*crc32 = crc32_ieee_update(*crc32, buf, rdlen);
		size -= rdlen;
		off += rdlen;
	}

	return 0;
}

int zb_slt_validate(u8_t sm_idx)
{
	int rc;
	struct zb_slt_info run, ver;
	struct zb_fsl_verify_hdr hdr;
	u32_t crc32 = 0;

	rc = zb_slt_open(&run, sm_idx, RUN);
	rc = rc | zb_slt_open(&ver, sm_idx, VERIFY);
	if (rc) {
		return rc;
	}

	rc = zb_slt_crc32(&crc32, &run);
	if (rc) {
		return rc;
	}

	rc = zb_read(&ver, 0U, &hdr, sizeof(struct zb_fsl_verify_hdr));
	if (rc) {
		return rc;
	}

	if (hdr.crc32 != crc32) {
		return -EFAULT;
	}
	LOG_INF("Image succesfully validated");
	return 0;
}

int zb_slt_confirm(u8_t sm_idx)
{
	int rc;
	struct zb_slt_info run, ver;
	struct zb_fsl_verify_hdr hdr;
	u32_t crc32 = 0;

	rc = zb_slt_open(&run, sm_idx, RUN);
	rc = rc | zb_slt_open(&ver, sm_idx, VERIFY);
	if (rc) {
		return rc;
	}

	rc = zb_slt_crc32(&crc32, &run);
	if (rc) {
		return rc;
	}

	rc = zb_read(&ver, 0U, &hdr, sizeof(struct zb_fsl_verify_hdr));
	if (rc) {
		return rc;
	}

	if (hdr.crc32 == crc32) {
		return 0;
	}

	hdr.crc32 = crc32;

	rc = zb_erase(&ver, 0U, ver.size);
	if (rc) {
		return rc;
	}

	rc = zb_write(&ver, 0U, &hdr, sizeof(struct zb_fsl_verify_hdr));
	LOG_INF("Image confirmation %s", rc ? "failed" : "succeeded");

	return rc;
}