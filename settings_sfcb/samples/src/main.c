/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <settings_sfcb.h>
#include <string.h>

// configuration of the filesystem is provided by this struct
const sfcb_fs_cfg cfg = {
	.offset = DT_FLASH_AREA_STORAGE_OFFSET,
	.sector_size = DT_FLASH_ERASE_BLOCK_SIZE,
	.sector_cnt = DT_FLASH_AREA_STORAGE_SIZE /
		      DT_FLASH_ERASE_BLOCK_SIZE,
	.dev_name = DT_FLASH_AREA_STORAGE_DEV,
};

// variables used by the filesystem
sfcb_fs sfcb = {
    .cfg = &cfg,
};

struct settings_sfcb setting_store = {
    .cf_sfcb = &sfcb,
};

u32_t boot_count=0;

static int set(const char *name, size_t len, settings_read_cb read_cb,
		  void *cb_arg)
{
	int rc;
	const char *next;

    if (!len) {
        printk("Ooh a deleted item\n");
        return 0;
    }
	if (settings_name_steq(name, "bc", &next) && !next) {
		rc = read_cb(cb_arg, &boot_count, sizeof(boot_count));
		return 0;
	}
	return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(ps, "ps", NULL, set, NULL, NULL);


// entry point
int main(void) {
    // mount the filesystem
    int rc, cnt;

    rc = settings_sfcb_backend_init(&setting_store);

    // reformat if we can't mount the filesystem
    if (rc) {
        sfcb_format(&sfcb);
        settings_sfcb_backend_init(&setting_store);
    }

    rc = settings_sfcb_src(&setting_store);
    rc = settings_sfcb_dst(&setting_store);

    for (cnt = 0; cnt < 8; cnt++) {
        boot_count = 0;
        settings_load();
        boot_count +=1;
        rc = settings_save_one("ps/bc", &boot_count, sizeof(boot_count));

        // print the boot count
        printk("bootcount: %d\n", boot_count);
    }

    while (sfcb.wr_sector != (sfcb.cfg->sector_cnt - 1)) {
        rc = settings_save_one("ps/bd", &boot_count, sizeof(boot_count));
    }

    boot_count = 0;
    settings_load();
    printk("bootcount: %d\n", boot_count);

    /* test delete */
    rc = settings_delete("ps/bc");
    boot_count = 0;
    settings_load();
    printk("bootcount: %d\n", boot_count);

    return 0;
}