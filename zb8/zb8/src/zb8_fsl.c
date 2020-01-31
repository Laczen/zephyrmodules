/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zb8/zb8_fsl.h>
#include <drivers/flash.h>
#include <soc.h>
#include <sys/crc.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(zb8_fsl);

static void zb_fsl_blupgrade(void)
{
#if IS_ENABLED(CONFIG_ZB_EIGHT_IS_FSL)
	struct device *bl_dev, *run_dev;
	u32_t offset;
	u8_t buf[32];

	bl_dev = device_get_binding(DT_FLASH_AREA_BOOT_DEV);
	run_dev = device_get_binding(DT_FLASH_AREA_RUN_0_DEV);
	/* update the bootloader */
	(void)flash_write_protection_set(bl_dev, 0);

	LOG_INF("Erasing old bootloader");
	(void)flash_erase(bl_dev, DT_FLASH_AREA_BOOT_OFFSET,
			  DT_FLASH_AREA_BOOT_SIZE);

	LOG_INF("Writing new bootloader");
	offset = DT_FLASH_AREA_BOOT_SIZE;
	while (offset) {
		offset -= sizeof(buf);
		(void)flash_read(run_dev, DT_FLASH_AREA_RUN_0_OFFSET + offset,
				 &buf, sizeof(buf));
		(void)flash_write(bl_dev, DT_FLASH_AREA_BOOT_OFFSET + offset,
				  &buf, sizeof(buf));
	}

	(void)flash_write_protection_set(bl_dev, 1);
	LOG_INF("Update done");
#endif
}

static void zb_fsl_jump(u32_t offset)
{
#if IS_ENABLED(CONFIG_ARM)

	struct arm_vector_table {
		u32_t msp;
		u32_t reset;
	} *vt;

	vt = (struct arm_vector_table *)(offset);

	irq_lock();

	__set_MSP(vt->msp);
	((void (*)(void))vt->reset)();

#endif
}

void zb_fsl_jump_fsl(void)
{
	zb_fsl_jump(DT_FLASH_BASE_ADDRESS);
}

void zb_fsl_jump_boot(void)
{
	struct zb_fsl_hdr hdr;
	struct device *fl_dev;
	u32_t offset;

	fl_dev = device_get_binding(DT_FLASH_AREA_BOOT_DEV);
	offset = DT_FLASH_AREA_BOOT_OFFSET;

	if (!fl_dev) {
		return;
	}

	(void)flash_read(fl_dev, offset, &hdr, sizeof(struct zb_fsl_hdr));
	offset += hdr.hdr_info.size;

	LOG_INF("Booting image at %x", offset);
	zb_fsl_jump(offset);
}

void zb_fsl_jump_run(void)
{
	struct zb_fsl_hdr hdr;
	struct device *fl_dev;
	u32_t offset;

	fl_dev = device_get_binding(DT_FLASH_AREA_RUN_0_DEV);
	offset = DT_FLASH_AREA_RUN_0_OFFSET;

	if (!fl_dev) {
		return;
	}

	(void)flash_read(fl_dev, offset, &hdr, sizeof(struct zb_fsl_hdr));
	offset += hdr.hdr_info.size;

	LOG_INF("Booting image at %x", offset);
	zb_fsl_jump(offset);
}

void zb_fsl_boot(void)
{
	struct zb_fsl_hdr hdr;
	struct zb_fsl_verify_hdr ver;
	struct device *fl_dev;
	u32_t off;

	fl_dev = device_get_binding(DT_FLASH_AREA_RUN_0_DEV);

	off = DT_FLASH_AREA_RUN_0_OFFSET;
	(void)flash_read(fl_dev, off, &hdr, sizeof(struct zb_fsl_hdr));

	off += hdr.hdr_info.size - sizeof(struct zb_fsl_verify_hdr);
	(void)flash_read(fl_dev, off, &ver, sizeof(struct zb_fsl_verify_hdr));

	off += sizeof(struct zb_fsl_verify_hdr);

	if ((ver.magic == FSL_VER_MAGIC) &&
	    (zb_fsl_crc32(fl_dev, off, hdr.size) == ver.crc32)) {
		if ((hdr.run_offset - hdr.hdr_info.size) ==
		    DT_FLASH_AREA_BOOT_OFFSET) {
			zb_fsl_blupgrade();
		} else {
			zb_fsl_jump(off);
		}
	}

	fl_dev = device_get_binding(DT_FLASH_AREA_BOOT_DEV);

	off = DT_FLASH_AREA_BOOT_OFFSET;
	(void)flash_read(fl_dev, off, &hdr, sizeof(struct zb_fsl_hdr));

	off += hdr.hdr_info.size;

	zb_fsl_jump(off);
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