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
//#include <drivers/flash.h>
#include <sys/crc.h>

#include <logging/log.h>
LOG_MODULE_DECLARE(zb8, CONFIG_ZB_EIGHT_LOG_LEVEL);

#define TLV_AREA_MIN_SIZE 256
#define TLV_AREA_MAX_SIZE 1024
#define TLV_AREA_SIGN_SIZE SIGNATURE_BYTES

static int img_check_dep(struct zb_img_dep *dep)
{
	u8_t cnt;
	struct zb_fsl_hdr hdr;
	struct zb_slt_info run;
	bool dep_loc_found = false;

	LOG_INF("Dependency image offset %x", dep->img_offset);

	cnt = zb_slt_area_cnt();
	while (cnt > 0) {
		cnt--;
		(void)zb_slt_open(&run, cnt, RUN);
		if (dep->img_offset == run.offset) {
			dep_loc_found = true;
			break;
		}
	}

	if (!dep_loc_found) {
		run.offset = DT_FLASH_AREA_SWPR_OFFSET;
		run.fl_dev = device_get_binding(DT_FLASH_AREA_SWPR_DEV);
		if (dep->img_offset == run.offset) {
			dep_loc_found = true;
		}
	}

	if (!dep_loc_found) {
		run.offset = DT_FLASH_AREA_LDR_OFFSET;
		run.fl_dev = device_get_binding(DT_FLASH_AREA_LDR_DEV);
		if (dep->img_offset == run.offset) {
			dep_loc_found = true;
		}
	}

	if (!dep_loc_found) {
		LOG_INF("Bad dependency");
		return -EFAULT;
	}

	if (zb_read(&run, 0U, &hdr, sizeof(struct zb_fsl_hdr))) {
		return -EFAULT;
	}

	if (hdr.magic != FSL_MAGIC) {
		return 0;
	}

	if ((dep->ver_min.major <= hdr.version.major) &&
	    (hdr.version.major <= dep->ver_max.major) &&
	    (dep->ver_min.minor <= hdr.version.minor) &&
	    (hdr.version.minor <= dep->ver_max.minor)) {
		LOG_INF("Dependency OK");
		return 0;
	}
	LOG_INF("Dependency failure");
	return -EFAULT;
}

static int img_get_info(struct zb_img_info *info, struct zb_slt_info *slt_info,
			struct zb_slt_info *dst_slt_info, bool full_check)
{
	int rc;
	u32_t off;
	struct tlv_entry entry;
	struct zb_fsl_hdr hdr;
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

	if (!full_check) {
		return 0;
	}

	tsize = hdr.hdr_info.size - sizeof(sign);
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

	/* Get the confirmation status */
	off = 0;
	while ((!zb_step_tlv(tlv, &off, &entry)) &&
	       (entry.type != TLVE_IMAGE_CONF) &&
	       (entry.length != TLVE_IMAGE_CONF_BYTES));
	if ((entry.type == TLVE_IMAGE_CONF) &&
	    (entry.length == TLVE_IMAGE_CONF_BYTES)) {
		LOG_DBG("IMG CONFIRMED");
		info->confirmed = true;
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
		/* No encryption key -> no encryption used */
		LOG_DBG("IMG NOT ENCRYPTED");
		info->enc_start = info->end;
	} else {
		if (!info->key_ok) {
			if (zb_get_encr_key(info->enc_key, info->enc_nonce,
					    entry.value, AES_KEY_SIZE)) {
				return -EFAULT;
			}
			info->key_ok = true;
		}
		LOG_DBG("IMG ENCRYPTED KEY OK");
		info->enc_start = info->start;
	}

	if (info->dep_ok) {
		return 0;
	}

	/* Validate the depencies */
	off = 0;
	while (!zb_step_tlv(tlv, &off, &entry)) {
		if ((entry.type != TLVE_IMAGE_DEPS) ||
	       	    (entry.length != TLVE_IMAGE_DEPS_BYTES)) {
			continue;
		}
		dep = (struct zb_img_dep *)entry.value;
		if (!info->confirmed &&
		   (dep->img_offset == dst_slt_info->offset)) {
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
	LOG_INF("Finshed img validation");
	return 0;
}

void zb_res_img_info(struct zb_img_info *info)
{
	info->hdr_ok = false;
	info->img_ok = false;
	info->dep_ok = false;
	info->key_ok = false;
	info->confirmed = false;
}

int zb_val_img_info(struct zb_img_info *info, struct zb_slt_info *slt_info,
		    struct zb_slt_info *dst_slt_info)
{
	return img_get_info(info, slt_info, dst_slt_info, true);
}

int zb_get_img_info(struct zb_img_info *info, struct zb_slt_info *slt_info)
{
	return img_get_info(info, slt_info, slt_info, true);
}

int zb_slt_has_img_hdr(struct zb_slt_info *slt_info)
{
	struct zb_img_info info;
	return img_get_info(&info, slt_info, slt_info, false);
}

bool zb_img_info_valid(struct zb_img_info *info)
{
	return info->hdr_ok & info->img_ok & info->dep_ok;
}
