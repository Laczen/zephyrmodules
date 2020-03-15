/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zb8/zb8_fsl.h>
#include <drivers/flash.h>
#include <soc.h>
#include <sys/crc.h>
#include <irq.h>

#include <logging/log.h>
LOG_MODULE_DECLARE(zb8, CONFIG_ZB_EIGHT_LOG_LEVEL);

#define FSL_VERIFY_OFFSET\
	(DT_FLASH_AREA_RUN_0_OFFSET + DT_FLASH_AREA_RUN_0_SIZE)

#define FSL_DEV DT_FLASH_AREA_RUN_0_DEV

enum moveslot{swpr, ldr};

static void zb_fsl_move(enum moveslot slot)
{
	struct device *fl_dev;
	u32_t ori_off, dst_off, dst_size;
	u8_t buf[32];

	fl_dev = device_get_binding(FSL_DEV);
	dst_off = DT_FLASH_AREA_SWPR_OFFSET;
	dst_size = DT_FLASH_AREA_SWPR_SIZE;

#ifdef DT_FLASH_AREA_LDR_OFFSET
	if (slot == ldr) {
		dst_off = DT_FLASH_AREA_LDR_OFFSET;
		dst_size = DT_FLASH_AREA_LDR_SIZE;
	}
#endif

	ori_off = DT_FLASH_AREA_RUN_0_OFFSET;

	/* update the swapper */
	LOG_INF("Updating swapper or loader");

	(void)flash_write_protection_set(fl_dev, 0);
	(void)flash_erase(fl_dev, dst_off, dst_size);

	while (ori_off < (DT_FLASH_AREA_RUN_0_OFFSET + dst_size)) {
		(void)flash_read(fl_dev, ori_off, &buf, sizeof(buf));
		(void)flash_write(fl_dev, dst_off, &buf, sizeof(buf));
		ori_off += sizeof(buf);
		dst_off += sizeof(buf);
	}

	ori_off = DT_FLASH_AREA_RUN_0_OFFSET;
	(void)flash_erase(fl_dev, ori_off, dst_size);

	(void)flash_write_protection_set(fl_dev, 1);
	LOG_INF("Finished update");

}

#if IS_ENABLED(CONFIG_SYS_CLOCK_EXISTS)
extern void sys_clock_disable(void);
#endif

static void zb_fsl_jump(u32_t offset)
{
#if IS_ENABLED(CONFIG_ARM)
	struct arm_vector_table {
		u32_t msp;
		u32_t reset;
	} *vt;

	vt = (struct arm_vector_table *)(offset);

 	irq_lock();
#if IS_ENABLED(CONFIG_SYS_CLOCK_EXISTS)
 	sys_clock_disable();
#endif

#if !IS_ENABLED(CONFIG_ZB_EIGHT_IS_FSL)
	for (int irq = 0; irq < CONFIG_NUM_IRQS; irq++) {
		if (irq_is_enabled(irq)) {
			irq_disable(irq);
		}
	}
#endif
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

	fl_dev = device_get_binding(FSL_DEV);
	off = FSL_VERIFY_OFFSET;
	(void)flash_read(fl_dev, off, &ver, sizeof(struct zb_fsl_verify_hdr));

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
			off = hdr.run_offset - hdr.hdr_info.size;
			if (off == DT_FLASH_AREA_RUN_0_OFFSET) {
				zb_fsl_jump(hdr.run_offset);
			}
			if (off == DT_FLASH_AREA_SWPR_OFFSET) {
				zb_fsl_move(swpr);
			}
#ifdef DT_FLASH_AREA_LDR_OFFSET
			if (off == DT_FLASH_AREA_LDR_OFFSET) {
				zb_fsl_move(ldr);
			}
#endif
		}
	}

	off = DT_FLASH_AREA_SWPR_OFFSET;
	(void)flash_read(fl_dev, off, &hdr, sizeof(struct zb_fsl_hdr));
	if (hdr.magic == FSL_MAGIC) {
		off += hdr.hdr_info.size;
		if (off == hdr.run_offset) {
			zb_fsl_jump(hdr.run_offset);
		}
	}
}