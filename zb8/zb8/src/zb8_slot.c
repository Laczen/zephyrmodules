/*
 * Copyright (c) 2019 Laczen
 *
 * Slot map definition: all slot areas are described in this file. A slot area
 * is a region in flash that contains 4 slots: a run slot, a move slot, a
 * upgrade slot and a swap status (swpstat) slot.
 * The run slot is where the image is executed from.
 * The move slot is a slot where the image from the run slot is moved to during
 * an upgrade.
 * The upgrade slot is where a new image is placed.
 * The swpstat slot is used to track the status of the "swapping" process.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zb8/zb8_slot.h>
#include <errno.h>

const struct zb_slt_area slotmap[] = {
	{
    		.run_offset = DT_FLASH_AREA_RUN_0_OFFSET,
		.run_size = DT_FLASH_AREA_RUN_0_SIZE,
		.run_devname = DT_FLASH_AREA_RUN_0_DEV,
		.upgrade_offset = DT_FLASH_AREA_UPGRADE_0_OFFSET,
		.upgrade_size = DT_FLASH_AREA_UPGRADE_0_SIZE,
		.upgrade_devname  = DT_FLASH_AREA_UPGRADE_0_DEV,
#ifdef DT_FLASH_AREA_MOVE_0_OFFSET
		.move_offset = DT_FLASH_AREA_MOVE_0_OFFSET,
		.move_size = DT_FLASH_AREA_MOVE_0_SIZE,
		.move_devname = DT_FLASH_AREA_MOVE_0_DEV,
#else
		.move_offset = DT_FLASH_AREA_UPGRADE_0_OFFSET,
		.move_size = DT_FLASH_AREA_UPGRADE_0_SIZE,
		.move_devname = DT_FLASH_AREA_UPGRADE_0_DEV,
#endif /* DT_FLASH_AREA_MOVE_0_OFFSET */
#ifdef DT_FLASH_AREA_SWPSTAT_0_OFFSET
		.swpstat_offset = DT_FLASH_AREA_SWPSTAT_0_OFFSET,
		.swpstat_size = DT_FLASH_AREA_SWPSTAT_0_SIZE,
		.swpstat_devname = DT_FLASH_AREA_SWPSTAT_0_DEV,
#else
#if DT_FLASH_AREA_MOVE_0_OFFSET != DT_FLASH_AREA_UPGRADE_0_OFFSET
#error("Slot 0 configuration error")
#endif
		.swpstat_offset = DT_FLASH_AREA_RUN_0_OFFSET,
		.swpstat_size = 0,
		.swpstat_devname = DT_FLASH_AREA_RUN_0_DEV,
#endif /* DT_FLASH_AREA_SWPSTAT_0_OFFSET */
	},
#ifdef DT_FLASH_AREA_RUN_1_OFFSET
	{
    		.run_offset = DT_FLASH_AREA_RUN_1_OFFSET,
		.run_size = DT_FLASH_AREA_RUN_1_SIZE,
		.run_devname = DT_FLASH_AREA_RUN_1_DEV,
		.upgrade_offset = DT_FLASH_AREA_UPGRADE_1_OFFSET,
		.upgrade_size = DT_FLASH_AREA_UPGRADE_1_SIZE,
		.upgrade_devname  = DT_FLASH_AREA_UPGRADE_1_DEV,
#ifdef DT_FLASH_AREA_MOVE_1_OFFSET
		.move_offset = DT_FLASH_AREA_MOVE_1_OFFSET,
		.move_size = DT_FLASH_AREA_MOVE_1_SIZE,
		.move_devname = DT_FLASH_AREA_MOVE_1_DEV,
#else
		.move_offset = DT_FLASH_AREA_UPGRADE_1_OFFSET,
		.move_size = DT_FLASH_AREA_UPGRADE_1_SIZE,
		.move_devname = DT_FLASH_AREA_UPGRADE_1_DEV,
#endif /* DT_FLASH_AREA_MOVE_1_OFFSET */
#ifdef DT_FLASH_AREA_SWPSTAT_1_OFFSET
		.swpstat_offset = DT_FLASH_AREA_SWPSTAT_1_OFFSET,
		.swpstat_size = DT_FLASH_AREA_SWPSTAT_1_SIZE,
		.swpstat_devname = DT_FLASH_AREA_SWPSTAT_1_DEV,
#else
#if DT_FLASH_AREA_MOVE_1_OFFSET != DT_FLASH_AREA_UPGRADE_1_OFFSET
#error("Slot 1 configuration error")
#endif
		.swpstat_offset = DT_FLASH_AREA_RUN_1_OFFSET,
		.swpstat_size = 0,
		.swpstat_devname = DT_FLASH_AREA_RUN_1_DEV,
#endif
	},
#endif
#ifdef DT_FLASH_AREA_RUN_2_OFFSET
	{
    		.run_offset = DT_FLASH_AREA_RUN_2_OFFSET,
		.run_size = DT_FLASH_AREA_RUN_2_SIZE,
		.run_devname = DT_FLASH_AREA_RUN_2_DEV,
		.upgrade_offset = DT_FLASH_AREA_UPGRADE_2_OFFSET,
		.upgrade_size = DT_FLASH_AREA_UPGRADE_2_SIZE,
		.upgrade_devname  = DT_FLASH_AREA_UPGRADE_2_DEV,
#ifdef DT_FLASH_AREA_MOVE_2_OFFSET
		.move_offset = DT_FLASH_AREA_MOVE_2_OFFSET,
		.move_size = DT_FLASH_AREA_MOVE_2_SIZE,
		.move_devname = DT_FLASH_AREA_MOVE_2_DEV,
#else
		.move_offset = DT_FLASH_AREA_UPGRADE_2_OFFSET,
		.move_size = DT_FLASH_AREA_UPGRADE_2_SIZE,
		.move_devname = DT_FLASH_AREA_UPGRADE_2_DEV,
#endif /* DT_FLASH_AREA_MOVE_2_OFFSET */
#ifdef DT_FLASH_AREA_SWPSTAT_2_OFFSET
		.swpstat_offset = DT_FLASH_AREA_SWPSTAT_2_OFFSET,
		.swpstat_size = DT_FLASH_AREA_SWPSTAT_2_SIZE,
		.swpstat_devname = DT_FLASH_AREA_SWPSTAT_2_DEV,
#else
#if DT_FLASH_AREA_MOVE_2_OFFSET != DT_FLASH_AREA_UPGRADE_2_OFFSET
#error("Slot 2 configuration error")
#endif
		.swpstat_offset = DT_FLASH_AREA_RUN_2_OFFSET,
		.swpstat_size = 0,
		.swpstat_devname = DT_FLASH_AREA_RUN_2_DEV,
#endif
	},
#endif
#ifdef DT_FLASH_AREA_RUN_3_OFFSET
	{
    		.run_offset = DT_FLASH_AREA_RUN_3_OFFSET,
		.run_size = DT_FLASH_AREA_RUN_3_SIZE,
		.run_devname = DT_FLASH_AREA_RUN_3_DEV,
		.upgrade_offset = DT_FLASH_AREA_UPGRADE_3_OFFSET,
		.upgrade_size = DT_FLASH_AREA_UPGRADE_3_SIZE,
		.upgrade_devname  = DT_FLASH_AREA_UPGRADE_3_DEV,
#ifdef DT_FLASH_AREA_MOVE_3_OFFSET
		.move_offset = DT_FLASH_AREA_MOVE_3_OFFSET,
		.move_size = DT_FLASH_AREA_MOVE_3_SIZE,
		.move_devname = DT_FLASH_AREA_MOVE_3_DEV,
#else
		.move_offset = DT_FLASH_AREA_UPGRADE_3_OFFSET,
		.move_size = DT_FLASH_AREA_UPGRADE_3_SIZE,
		.move_devname = DT_FLASH_AREA_UPGRADE_3_DEV,
#endif /* DT_FLASH_AREA_MOVE_0_OFFSET */
#ifdef DT_FLASH_AREA_SWPSTAT_3_OFFSET
		.swpstat_offset = DT_FLASH_AREA_SWPSTAT_3_OFFSET,
		.swpstat_size = DT_FLASH_AREA_SWPSTAT_3_SIZE,
		.swpstat_devname = DT_FLASH_AREA_SWPSTAT_3_DEV,
#else
#if DT_FLASH_AREA_MOVE_3_OFFSET != DT_FLASH_AREA_UPGRADE_3_OFFSET
#error("Slot 3 configuration error")
#endif
		.swpstat_offset = DT_FLASH_AREA_RUN_3_OFFSET,
		.swpstat_size = 0,
		.swpstat_devname = DT_FLASH_AREA_RUN_3_DEV,
#endif
	},
#endif
};

const u8_t slotmap_cnt = sizeof(slotmap)/sizeof(slotmap[0]);

u8_t zb_slt_area_cnt(void)
{
	return slotmap_cnt;
}

int zb_slt_open(struct zb_slt_info *info, u8_t sm_idx, enum slot slot)
{
	struct device *fl_dev;

	if ((slot != RAM) && (sm_idx >= slotmap_cnt)) {
		return -ENOENT;
	}
	switch(slot) {
	case RUN:
		fl_dev = device_get_binding(slotmap[sm_idx].run_devname);
		info->offset = slotmap[sm_idx].run_offset;
		info->size = slotmap[sm_idx].run_size;
		break;
	case MOVE:
		fl_dev = device_get_binding(slotmap[sm_idx].move_devname);
		info->offset = slotmap[sm_idx].move_offset;
		info->size = slotmap[sm_idx].move_size;
		break;
	case UPGRADE:
		fl_dev = device_get_binding(slotmap[sm_idx].upgrade_devname);
		info->offset = slotmap[sm_idx].upgrade_offset;
		info->size = slotmap[sm_idx].upgrade_size;
		break;
	case SWPSTAT:
		fl_dev = device_get_binding(slotmap[sm_idx].swpstat_devname);
		info->offset = slotmap[sm_idx].swpstat_offset;
		info->size = slotmap[sm_idx].swpstat_size;
		break;
	case RAM:
		fl_dev = NULL;
		//info->offset = DT_SRAM_BASE_ADDRESS;
		//info->size = DT_SRAM_SIZE;
		break;
	default:
		fl_dev = NULL;
	}
	if ((!fl_dev) && (slot != RAM)){
		return -ENOENT;
	}
	info->fl_dev = fl_dev;
	return 0;
}

u32_t zb_sectorsize_get(u8_t sm_idx)
{
	u32_t sector_size;
	if (sm_idx >= slotmap_cnt) {
		return 0;
	}
	sector_size = slotmap[sm_idx].move_offset - slotmap[sm_idx].run_offset;
	return sector_size;
}

bool zb_range_in_slt(struct zb_slt_info *info, u32_t address, size_t len)
{
	return ((info->offset <= address) &
		((address + len) < (info->offset + info->size)));
}

bool zb_inplace_slt(u8_t sm_idx)
{
	return (slotmap[sm_idx].move_offset == slotmap[sm_idx].upgrade_offset);
}