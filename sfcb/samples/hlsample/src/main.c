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
	.size = DT_FLASH_AREA_STORAGE_SIZE,
	.dev_name = DT_FLASH_AREA_STORAGE_DEV,
};

// entry point
int main(void) {
    // mount the filesystem
    int rc;
    u32_t boot_count = 0;

    sfcb.cfg = &cfg;
    rc = sfcb_mount(&sfcb);

    // reformat if we can't mount the filesystem
    if (rc) {
        sfcb_format(&sfcb);
        sfcb_mount(&sfcb);
    }

    // read current count
    (void)sfcb_read(&sfcb, 0, &boot_count, sizeof(boot_count));

    // update boot count
    boot_count += 1;
    (void)sfcb_write(&sfcb, 0, &boot_count, sizeof(boot_count));

    // release any resources we were using
    sfcb_unmount(&sfcb);

    // print the boot count
    printk("bootcount: %d\n", boot_count);
    return 0;
}