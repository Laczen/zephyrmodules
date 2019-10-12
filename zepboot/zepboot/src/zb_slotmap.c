/*
 * Copyright (c) 2018 Laczen
 *
 * Slot map definition: all slot areas are described in this file. A slot area
 * is a region in flash that contains 3 seperate regions: a slot 0 region from
 * where a image is normally executed, a slot 1 region that is used to store
 * a image upgrade and a status region that is used to track the move status.
 * When a image upgrade is placed in slot 1, the bootloader will detect if it
 * is new, validate the upgrade and if ok start the move to either slot 0 or
 * remain in slot 1.
 * When there are no image upgrades in any of the slot areas the bootloader
 * will boot the image in slot_map[0], slot 0 or slot 1. Images in slot 1
 * are only booted if the load_address is equal to the location in slot 1.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "../include/zb_flash.h"

const struct slt_area slot_map[] = {
	{
	 .slt1_offset = DT_FLASH_AREA_IMAGE_1_OFFSET,
	 .slt1_size = DT_FLASH_AREA_IMAGE_1_SIZE,
	 .slt1_devname = DT_FLASH_AREA_0_DEV,
	 .slt0_offset = DT_FLASH_AREA_IMAGE_0_OFFSET,
	 .slt0_size = DT_FLASH_AREA_IMAGE_0_SIZE,
	 .slt0_devname = DT_FLASH_AREA_0_DEV,
#if IS_ENABLED(CONFIG_ZEPBOOT_IS_BOOTLOADER)
	 /* Only the bootloader needs access to swapstat */
	 .swpstat_offset = DT_FLASH_AREA_IMAGE_SCRATCH_OFFSET,
	 .swpstat_size = DT_FLASH_AREA_IMAGE_SCRATCH_SIZE,
	 .swpstat_devname = DT_FLASH_AREA_0_DEV,
#endif
	},
};

const unsigned int slot_map_cnt = sizeof(slot_map)/sizeof(slot_map[0]);
