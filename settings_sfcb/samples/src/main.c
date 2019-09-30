/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <settings_sfcb.h>
#include <string.h>

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

extern sfcb_fs settings_sfcb;
// entry point
int main(void) {
    int rc, cnt;

    rc = settings_subsys_init();

    for (cnt = 0; cnt < 8; cnt++) {
        boot_count = 0;
        settings_load();
        boot_count +=1;
        rc = settings_save_one("ps/bc", &boot_count, sizeof(boot_count));

        // print the boot count
        printk("bootcount: %d\n", boot_count);
    }

    while (settings_sfcb.wr_sector != (settings_sfcb.cfg->sector_cnt - 1)) {
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