/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <sfcb.h>
#include <string.h>

// variables used by the filesystem
sfcb_fs sfcb;

// configuration of the filesystem is provided by this struct
const sfcb_fs_cfg cfg = {
	.offset = DT_FLASH_AREA_STORAGE_OFFSET,
	.sector_size = DT_FLASH_ERASE_BLOCK_SIZE,
	.sector_cnt = DT_FLASH_AREA_STORAGE_SIZE /
		      DT_FLASH_ERASE_BLOCK_SIZE,
	.dev_name = DT_FLASH_AREA_STORAGE_DEV,
};

// entry point
int main(void) {
    // mount the filesystem
    int rc;
    u32_t boot_count = 0;
    sfcb_loc loc;
    sfcb_ate *ate;

    sfcb.cfg = &cfg;
    rc = sfcb_mount(&sfcb);

    // reformat if we can't mount the filesystem
    if (rc) {
        sfcb_format(&sfcb);
        sfcb_mount(&sfcb);
    }

    // read current count
    if (!sfcb_start_loc(&sfcb, &loc)) {
        while (!sfcb_next_loc(&loc)) {
            ate = sfcb_get_ate(&loc);
            if (ate->id == 0) {
                sfcb_read_loc(&loc, &boot_count, sizeof(boot_count));
            }
        }
    }

    // update boot count
    boot_count += 1;
    if (!sfcb_open_loc(&sfcb, &loc, 0, sizeof(boot_count))) {
        (void)sfcb_write_loc(&loc, &boot_count, sizeof(boot_count));
        // remember the storage is not updated until the location is closed
        (void)sfcb_close_loc(&loc);
    }

    // release any resources we were using
    sfcb_unmount(&sfcb);

    // print the boot count
    printk("bootcount: %d\n", boot_count);
    return 0;
}