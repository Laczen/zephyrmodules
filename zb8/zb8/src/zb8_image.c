/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zb8/zb8_image.h>
#include <zb8/zb8_flash.h>
#include <zb8/zb8_crypto.h>
#include <zb8/zb8_tlv.h>
#include <zb8/zb8_slot.h>

#include <errno.h>
#include <device.h>
#include <flash.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(zb8_image);

#define TLV_AREA_MIN_SIZE 256
#define TLV_AREA_MAX_SIZE 1024
#define TLV_AREA_SIGN_SIZE SIGNATURE_BYTES

static int img_check_dep(struct zb_img_dep *dep)
{
	int rc;
	u8_t cnt;
	struct zb_fsl_hdr hdr;
	struct zb_slt_info slt;
	struct device *fl_dev = NULL;

	LOG_INF("Dependency offset %x", dep->offset);

	if (dep->offset == DT_FLASH_AREA_BOOT_OFFSET) {
		fl_dev = device_get_binding(DT_FLASH_AREA_BOOT_DEV);
	} else {
		cnt = zb_slt_area_cnt();
		while (cnt>0) {
			(void)zb_slt_open(&slt, --cnt, RUN);
			if (slt.offset == dep->offset) {
				fl_dev = slt.fl_dev;
			}
		}
	}

	if (!fl_dev) {
		LOG_INF("Bad dependency");
		return -EFAULT;
	}

	rc = flash_read(fl_dev, dep->offset, &hdr, sizeof(struct zb_fsl_hdr));
	if (rc) {
		return rc;
	}

	if (hdr.magic != FSL_MAGIC) {
		hdr.version.minor = 0x00;
		hdr.version.major = 0x00;
	}

	if ((dep->ver_min.major <= hdr.version.major) &&
	    (hdr.version.major <= dep->ver_max.major) &&
	    (dep->ver_min.minor <= hdr.version.minor) &&
	    (hdr.version.minor <= dep->ver_max.minor)) {
		return 0;
	}
	return -EFAULT;
}

static int img_get_info(struct zb_img_info *info, struct zb_slt_info *slt_info,
			struct zb_slt_info *dst_slt_info)
{
	int rc;
	u32_t off;
	struct tlv_entry entry;
	struct zb_fsl_hdr hdr;
	struct zb_fsl_verify_hdr ver;
	struct zb_img_dep *dep;
	size_t tsize;
	u8_t tlv[TLV_AREA_MAX_SIZE];
	u8_t hdrhash[HASH_BYTES];
	u8_t imghash[HASH_BYTES];
	u8_t sign[SIGNATURE_BYTES];

	rc = zb_read(slt_info, 0U, &hdr, sizeof(struct zb_fsl_hdr));
	if (rc) {
		return rc;
	}

	LOG_DBG("Magic [%x] Size [%u] HdrSize [%u]",
		hdr.magic, hdr.size, hdr.hdr_info.size);
	if ((hdr.magic != FSL_MAGIC) || (hdr.hdr_info.sigtype !=0) ||
	    (hdr.hdr_info.siglen != sizeof(sign))) {
		return -EFAULT;
	}

	tsize = hdr.hdr_info.size - sizeof(struct zb_fsl_verify_hdr);
	rc = zb_read(slt_info, tsize , &ver, sizeof(struct zb_fsl_verify_hdr));
	if (rc) {
		return rc;
	}

	/* Check if the image is confirmed */
	if (ver.magic == FSL_VER_MAGIC) {
		info->confirmed = true;
	}

	/* Check if the image is a bootloader */
	if ((hdr.run_offset - hdr.hdr_info.size) == DT_FLASH_AREA_BOOT_OFFSET) {
		info->is_bootloader = true;
	}

	tsize -= sizeof(sign);
	/* Verify the image header signature */
	if (!info->hdr_ok) {
		rc = zb_hash(hdrhash, slt_info, 0U, tsize);
		if (rc) {
			return rc;
		}
		rc = zb_read(slt_info, tsize, sign, sizeof(sign));
		if (rc) {
			return rc;
		}
		rc = zb_sign_verify(hdrhash, sign);
		if (rc) {
			LOG_DBG("HDR SIGNATURE INVALID");
			return -EFAULT;
		}
		LOG_DBG("HDR SIGNATURE OK");
		info->hdr_ok = true;
	}

	info->start = hdr.hdr_info.size;
	info->enc_start = info->start;
	info->end = info->start + hdr.size;
	info->load_address = hdr.run_offset;
    	info->version = (u32_t)(hdr.version.major << 24) +
	    		(u32_t)(hdr.version.minor << 16) +
			(u32_t)(hdr.version.rev);
    	info->build = hdr.build;


	tsize -= sizeof(struct zb_fsl_hdr);
	rc = zb_read(slt_info, sizeof(struct zb_fsl_hdr), tlv, tsize);
	if (rc) {
	 	return rc;
	}

	/* Verify the image hash */
	off = 0;
	while ((!zb_step_tlv(tlv, &off, &entry)) &&
	       (entry.type != TLVE_IMAGE_HASH) &&
	       (entry.length != TLVE_IMAGE_HASH_BYTES));

	if (entry.type != TLVE_IMAGE_HASH) {
		return -EFAULT;
	}

	if (!info->img_ok) {
		rc = zb_hash(imghash, slt_info, info->start, hdr.size);
		if (rc) {
			LOG_DBG("HASH CALC Failure");
			return -EFAULT;
		}
		if (memcmp(entry.value, imghash, HASH_BYTES)) {
			LOG_DBG("IMG HASH INVALID");
			return -EFAULT;
		}
		LOG_DBG("IMG HASH OK");
		info->img_ok = true;
	}

	/* Get the encryption parameters */
	off = 0;
	while ((!zb_step_tlv(tlv, &off, &entry)) &&
	       (entry.type != TLVE_IMAGE_EPUBKEY) &&
	       (entry.length != TLVE_IMAGE_EPUBKEY_BYTES));
	if (!entry.type) {
		info->enc_start = info->end;
	} else {
		if (zb_get_encr_key(info->enc_key, info->enc_nonce, entry.value,
				    AES_KEY_SIZE)) {
			return -EFAULT;
		}
		info->enc_start = info->start;
	}

	if (info->dep_ok) {
		return 0;
	}

	/* Validate the depencies */
	off = 0;
	while (!zb_step_tlv(tlv, &off, &entry)) {
		if ((entry.type != TLVE_IMAGE_DEPS) &&
	       	    (entry.length != TLVE_IMAGE_DEPS_BYTES)) {
			continue;
		}
		dep = (struct zb_img_dep *)entry.value;
		if (!info->confirmed && (dep->offset == dst_slt_info->offset)) {
			dep->ver_min.major = dep->ver_max.major;
			dep->ver_min.minor = dep->ver_max.minor;
		}
		if (img_check_dep(dep)) {
			LOG_DBG("IMG DEPENDENCY FAILURE");
			return -EFAULT;
		}
	}
	LOG_DBG("IMG DEPENDENCIES OK");
	info->dep_ok = true;

	return 0;
}

int zb_val_img_info(struct zb_img_info *info, struct zb_slt_info *slt_info,
		    struct zb_slt_info *dst_slt_info)
{
	info->hdr_ok = false;
	info->img_ok = false;
	info->dep_ok = false;
	info->is_bootloader = false;
	info->confirmed = false;

	return img_get_info(info, slt_info, dst_slt_info);
}

int zb_get_img_info(struct zb_img_info *info, struct zb_slt_info *slt_info)
{
	info->hdr_ok = true;
	info->img_ok = true;
	info->dep_ok = true;
	info->is_bootloader = false;
	info->confirmed = false;

	return img_get_info(info, slt_info, slt_info);
}

bool zb_img_info_valid(struct zb_img_info *info)
{
	return info->hdr_ok & info->img_ok & info->dep_ok;
}

int zb_img_confirm(struct zb_slt_info *slt_info)
{
	int rc;
	struct zb_fsl_hdr hdr;
	struct zb_fsl_verify_hdr ver;
	u32_t off;
	u32_t crc32;

	rc = zb_read(slt_info, 0U, &hdr, sizeof(struct zb_fsl_hdr));
	if (rc) {
		return rc;
	}

	off = hdr.hdr_info.size - sizeof(struct zb_fsl_verify_hdr);
	rc = zb_read(slt_info, off, &ver, sizeof(struct zb_fsl_verify_hdr));
	if (rc) {
		return rc;
	}

	crc32 = zb_fsl_crc32(slt_info->fl_dev, slt_info->offset + off +
			     sizeof(struct zb_fsl_verify_hdr), hdr.size);

	if (ver.crc32 == crc32) {
		return 0;
	}

	ver.magic = FSL_VER_MAGIC;
	ver.crc32 = crc32;
	rc = zb_write(slt_info, off, &ver, sizeof(struct zb_fsl_verify_hdr));return rc;
}