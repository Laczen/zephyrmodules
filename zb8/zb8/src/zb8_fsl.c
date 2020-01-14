/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zb8/zb8_fsl.h>
#include <flash.h>
#include <soc.h>
#include <sys/crc.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(zb8_fsl);

void zb_fsl_blupgrade(struct device *run_dev, struct zb_fsl_hdr *run_hdr)
{
#if IS_ENABLED(CONFIG_ZEPBOOT_IS_FSL)
	struct device *bl_dev;
	u32_t offset;
	u8_t buf[32];

	bl_dev = device_get_binding(DT_FLASH_AREA_BOOT_DEV);
	offset = 0;
	/* update the bootloader */
	(void)flash_write_protection_set(bl_dev, 0);

	LOG_INF("Erasing old bootloader");
	(void)flash_erase(bl_dev, DT_FLASH_AREA_BOOT_OFFSET,
			  DT_FLASH_AREA_BOOT_SIZE);

	LOG_INF("Writing new bootloader");
	while (run_hdr->size - offset) {
		(void)flash_read(run_dev, DT_FLASH_AREA_RUN_0_OFFSET + offset,
				 &buf, sizeof(buf));
		(void)flash_write(bl_dev, DT_FLASH_AREA_BOOT_OFFSET + offset,
				  &buf, sizeof(buf));
		offset += sizeof(buf);
	}

	LOG_INF("Update done");
#endif
}

void zb_fsl_boot(enum bt_type type)
{
	struct zb_fsl_hdr hdr;
	struct device *fl_dev;
	u32_t hdr_offset;

	switch(type) {
	case FSL_SLT:
		hdr_offset = DT_FLASH_BASE_ADDRESS;
		fl_dev = NULL;
		break;
	case BOOT_SLT:
		hdr_offset = DT_FLASH_AREA_BOOT_OFFSET;
		fl_dev = device_get_binding(DT_FLASH_AREA_BOOT_DEV);
		break;
	case RUN_SLT:
		hdr_offset = DT_FLASH_AREA_RUN_0_OFFSET;
		fl_dev = device_get_binding(DT_FLASH_AREA_RUN_0_DEV);
		break;
	default:
		return;
	}

	if (fl_dev) {
		(void)flash_read(fl_dev, hdr_offset, &hdr,
				 sizeof(struct zb_fsl_hdr));
		if (hdr.hdr_info.size == 0xffff) {
			return;
		}
		hdr_offset += hdr.hdr_info.size;

	}

	LOG_INF("Booting image at %x", hdr_offset);

#if IS_ENABLED(CONFIG_ARM)

	struct arm_vector_table {
		u32_t msp;
		u32_t reset;
	} *vt;

	vt = (struct arm_vector_table *)(hdr_offset);

	irq_lock();

	__set_MSP(vt->msp);
	((void (*)(void))vt->reset)();

#endif
}

enum img_type zb_fsl_get_type(struct device *fl_dev, struct zb_fsl_hdr *hdr)
{
	u32_t off;
	struct zb_fsl_verify_hdr ver;

	off = hdr->run_offset;
	(void)flash_read(fl_dev, off, hdr, sizeof(struct zb_fsl_hdr));

	off += hdr->hdr_info.size - sizeof(struct zb_fsl_verify_hdr);
	(void)flash_read(fl_dev, off, &ver, sizeof(struct zb_fsl_verify_hdr));

	off += sizeof(struct zb_fsl_verify_hdr);

	if ((ver.magic != FSL_VER_MAGIC) ||
	    (zb_fsl_crc32(fl_dev, off, hdr->size) != ver.crc32)) {
		return INVALID_IMG;
	}

	if (hdr->run_offset == DT_FLASH_AREA_BOOT_OFFSET) {
		return BOOT_IMG;
	}
	return RUN_IMG;
}

u32_t zb_fsl_crc32(struct device *fl_dev, u32_t off, size_t len)
{
	u8_t buf[32];
	u32_t crc32;

	crc32 = 0U;
	while (len) {
		size_t rdlen = MIN(len, sizeof(buf));
		(void)flash_read(fl_dev, off, &buf, rdlen);
		crc32 = crc32_ieee_update(crc32, buf, rdlen);
		len -= rdlen;
		off += rdlen;
	}

	return crc32;
}