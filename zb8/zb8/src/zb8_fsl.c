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

#define FSL_VERIFY_OFFSET\
	(DT_FLASH_AREA_RUN_0_OFFSET + DT_FLASH_AREA_RUN_0_SIZE)

#ifdef DT_FLASH_AREA_MOVE_0_OFFSET
#define FSL_VERIFY_DEV DT_FLASH_AREA_MOVE_0_DEV
#define FSL_VERIFY_SIZE\
	(DT_FLASH_AREA_MOVE_0_OFFSET - DT_FLASH_AREA_RUN_0_OFFSET)
#else
#define FSL_VERIFY_DEV DT_FLASH_AREA_UPGRADE_0_DEV
#define FSL_VERIFY_SIZE\
	(DT_FLASH_AREA_UPGRADE_0_OFFSET - DT_FLASH_AREA_RUN_0_OFFSET)
#endif

/*
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#pragma message("content of FSL_VERIFY_DEV: " STR(FSL_VERIFY_DEV))
#pragma message("content of FSL_VERIFY_OFFSET: " STR(FSL_VERIFY_OFFSET))
#pragma message("content of FSL_VERIFY_SIZE: " STR(FSL_VERIFY_SIZE))
*/

enum moveslot{swpr, ldr};

static void zb_fsl_move(enum moveslot slot)
{
	struct device *ori_fl_dev, *dst_fl_dev;
	u32_t ori_off, dst_off, dst_size;
	u8_t buf[32];

	dst_fl_dev = device_get_binding(DT_FLASH_AREA_SWPR_DEV);
	dst_off = DT_FLASH_AREA_SWPR_OFFSET;
	dst_size = DT_FLASH_AREA_SWPR_SIZE;

#ifdef DT_FLASH_AREA_LDR_OFFSET
	if (slot == ldr) {
		dst_fl_dev = device_get_binding(DT_FLASH_AREA_LDR_DEV);
		dst_off = DT_FLASH_AREA_LDR_OFFSET;
		dst_size = DT_FLASH_AREA_LDR_SIZE;
	}
#endif

	ori_fl_dev = device_get_binding(DT_FLASH_AREA_RUN_0_DEV);
	ori_off = DT_FLASH_AREA_RUN_0_OFFSET;

	/* update the swapper */
	LOG_INF("Updating image swapper");
	(void)flash_write_protection_set(dst_fl_dev, 0);
	(void)flash_erase(dst_fl_dev, dst_off, dst_size);
	while (dst_size) {
		(void)flash_read(ori_fl_dev, ori_off, &buf, sizeof(buf));
		(void)flash_write(dst_fl_dev, dst_off, &buf, sizeof(buf));
		ori_off += sizeof(buf);
		dst_off += sizeof(buf);
		dst_size -= sizeof(buf);
	}
	(void)flash_write_protection_set(dst_fl_dev, 1);
	LOG_INF("Finished updating image swapper");

	ori_off = DT_FLASH_AREA_RUN_0_OFFSET;
	(void)flash_write_protection_set(ori_fl_dev, 0);
	(void)flash_erase(ori_fl_dev, ori_off, dst_size);
	(void)flash_write_protection_set(ori_fl_dev, 1);

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
	LOG_INF("Booting image at %x", DT_FLASH_AREA_FSL_OFFSET);
	zb_fsl_jump(DT_FLASH_AREA_FSL_OFFSET);
}

void zb_fsl_jump_swpr(void)
{
	struct device *fl_dev;
	struct zb_fsl_hdr hdr;
	u32_t offset;

	fl_dev = device_get_binding(DT_FLASH_AREA_SWPR_DEV);
	offset = DT_FLASH_AREA_SWPR_OFFSET;

	(void)flash_read(fl_dev, offset, &hdr, sizeof(struct zb_fsl_hdr));
	offset += hdr.hdr_info.size;

	if (hdr.run_offset == offset) {
		LOG_INF("Booting image at %x", offset);
		zb_fsl_jump(offset);
	}
}

void zb_fsl_jump_ldr(void)
{
#ifdef DT_FLASH_AREA_LDR_OFFSET
	struct device *fl_dev;
	struct zb_fsl_hdr hdr;
	u32_t offset;

	fl_dev = device_get_binding(DT_FLASH_AREA_LDR_DEV);
	offset = DT_FLASH_AREA_LDR_OFFSET;

	(void)flash_read(fl_dev, offset, &hdr, sizeof(struct zb_fsl_hdr));
	offset += hdr.hdr_info.size;

	if (hdr.run_offset == offset) {
		LOG_INF("Booting image at %x", offset);
		zb_fsl_jump(offset);
	}
#endif
}

void zb_fsl_jump_run(void)
{
	struct device *fl_dev;
	struct zb_fsl_hdr hdr;
	u32_t offset;

	fl_dev = device_get_binding(DT_FLASH_AREA_RUN_0_DEV);
	offset = DT_FLASH_AREA_RUN_0_OFFSET;

	(void)flash_read(fl_dev, offset, &hdr, sizeof(struct zb_fsl_hdr));
	offset += hdr.hdr_info.size;

	if (hdr.run_offset == offset) {
		LOG_INF("Booting image at %x", offset);
		zb_fsl_jump(offset);
	}
}

void zb_fsl_boot(void)
{
	struct zb_fsl_hdr hdr;
	struct zb_fsl_verify_hdr ver;
	struct device *fl_dev;
	u32_t crc32 = 0U, off, size;
	u8_t buf[32];

	fl_dev = device_get_binding(FSL_VERIFY_DEV);
	off = FSL_VERIFY_OFFSET;
	(void)flash_read(fl_dev, off, &ver, sizeof(struct zb_fsl_verify_hdr));

	fl_dev = device_get_binding(DT_FLASH_AREA_RUN_0_DEV);
	off = DT_FLASH_AREA_RUN_0_OFFSET;
	(void)flash_read(fl_dev, off, &hdr, sizeof(struct zb_fsl_hdr));

	if (hdr.magic == FSL_MAGIC) {
		size = hdr.hdr_info.size + hdr.size;
		while (size) {
			size_t rdlen = MIN(size, sizeof(buf));
			(void)flash_read(fl_dev, off, &buf, rdlen);
			crc32 = crc32_ieee_update(crc32, buf, rdlen);
			size -= rdlen;
			off += rdlen;
		}

		if (crc32 == ver.crc32) {
			hdr.run_offset -= hdr.hdr_info.size;
			if (hdr.run_offset == DT_FLASH_AREA_RUN_0_OFFSET) {
				zb_fsl_jump(hdr.run_offset + hdr.hdr_info.size);
			}
			if (hdr.run_offset == DT_FLASH_AREA_SWPR_OFFSET) {
				zb_fsl_move(swpr);
			}
#ifdef DT_FLASH_AREA_LDR_OFFSET
			if (hdr.run_offset == DT_FLASH_AREA_LDR_OFFSET) {
				zb_fsl_move(ldr);
			}
#endif
		}
	}

	fl_dev = device_get_binding(DT_FLASH_AREA_SWPR_DEV);
	off = DT_FLASH_AREA_SWPR_OFFSET;
	(void)flash_read(fl_dev, off, &hdr, sizeof(struct zb_fsl_hdr));
	if (hdr.magic == FSL_MAGIC) {
		hdr.run_offset -= hdr.hdr_info.size;
		if (hdr.run_offset == DT_FLASH_AREA_SWPR_OFFSET) {
			zb_fsl_jump(hdr.run_offset + hdr.hdr_info.size);
		}
	}
}