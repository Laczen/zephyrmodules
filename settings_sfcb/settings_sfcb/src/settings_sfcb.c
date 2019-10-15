/*
 * Copyright (c) 2019 Laczen
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <settings_sfcb.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(settings_sfcb, CONFIG_SETTINGS_SFCB_LOG_LEVEL);

struct settings_sfcb_read_fn_arg {
	sfcb_loc loc;
};

static int settings_sfcb_load(struct settings_store *cs,
			      const struct settings_load_arg *arg);
static int settings_sfcb_save(struct settings_store *cs, const char *name,
			     const char *value, size_t val_len);

static struct settings_store_itf settings_sfcb_itf = {
	.csi_load = settings_sfcb_load,
	.csi_save = settings_sfcb_save,
};

static ssize_t settings_sfcb_read_fn(void *back_end, void *data, size_t len)
{
	struct settings_sfcb_read_fn_arg *rd_fn_arg;
	ssize_t rc;

	rd_fn_arg = (struct settings_sfcb_read_fn_arg *)back_end;

	rc = sfcb_read_loc(&rd_fn_arg->loc, data, len);
	return rc;
}

int settings_sfcb_src(struct settings_sfcb *cf)
{
	cf->cf_store.cs_itf = &settings_sfcb_itf;
	settings_src_register(&cf->cf_store);

	return 0;
}

static int settings_sfcb_compress(sfcb_fs *fs);

int settings_sfcb_dst(struct settings_sfcb *cf)
{
	cf->cf_store.cs_itf = &settings_sfcb_itf;
	settings_dst_register(&cf->cf_store);
	/* Destination requires a compress routine */
	cf->cf_sfcb->compress = &settings_sfcb_compress;

	return 0;
}

static int settings_sfcb_load(struct settings_store *cs,
			      const struct settings_load_arg *arg)
{
	struct settings_sfcb *cf = (struct settings_sfcb *)cs;
	struct settings_sfcb_read_fn_arg read_fn_arg;
	char name[SETTINGS_MAX_NAME_LEN + SETTINGS_EXTRA_LEN + 1];
	char wdata[CONFIG_SFCB_WBS];
	char *np, *wp;
	int name_end, rc, val_len, i;
	sfcb_ate *load_ate, *walk_ate;
	sfcb_loc walk_loc;
	bool load;

	rc = sfcb_start_loc(cf->cf_sfcb, &read_fn_arg.loc);
	if (rc) {
		return rc;
	}

	while (!sfcb_next_loc(&read_fn_arg.loc)) {

		load_ate = sfcb_get_ate(&read_fn_arg.loc);
		if (load_ate->id != SETTINGS_SFCB_ID) {
			continue;
		}

		/* In the sfcb backend each setting item is stored as:
		 * name=value
		 */
		rc = sfcb_read_loc(&read_fn_arg.loc, &name, sizeof(name));
		if (rc < 0) {
			continue;
		}
		name_end = 0;
		while ((name[name_end] != '=') && (name_end < rc)) {
			name_end++;
		}
		if (name_end == rc) {
			/* no '=' found */
			continue;
		}

		load = true;
		/* If a later match is found delay the loading */
		walk_loc = read_fn_arg.loc;
		while (!sfcb_next_loc(&walk_loc)) {
			walk_ate = sfcb_get_ate(&walk_loc);
			if (walk_ate->id != SETTINGS_SFCB_ID) {
				continue;
			}
			np = name;
			while (1) {
				rc = sfcb_read_loc(&walk_loc, &wdata,
					CONFIG_SFCB_WBS);
				if (!rc) {
					break;
				}
				wp = wdata;
				for (i = 0; i < rc; i++) {
					if (*wp != *np) {
						break;
					}
					if ((*wp == '=') || (*np == '=')) {
						break;
					}
					wp++;
					np++;
				}
				if (i == CONFIG_SFCB_WBS) {
					continue;
				}
				if (*wp != *np) {
					/* quit early when the walk location
					 * has no information about the load
					 */
					break;
				}
				if (*np == '=') {
					/* we have found the same variable */
					load = false;
					break;
				}
			}
			if (!load) {
				break;
			}
		}

		val_len = load_ate->len - name_end - 1;
		/* Only call set handler when last entry is found and it is not
		 * deleted.
		 */
		if (load && val_len) {
			name[name_end] = '\0';
			/* set the read position just after '=' */
			sfcb_setpos_loc(&read_fn_arg.loc, name_end + 1);
			rc = settings_call_set_handler(name, val_len,
				settings_sfcb_read_fn, &read_fn_arg,
				(void *)arg);
			if (rc) {
				break;
			}
		}

	}
	return 0;
}

static int settings_sfcb_save(struct settings_store *cs, const char *name,
			     const char *value, size_t val_len)
{
	struct settings_sfcb *cf = (struct settings_sfcb *)cs;
	sfcb_loc wr_loc;
	int rc, nm_len;

	if (!name) {
		return -EINVAL;
	}

	nm_len = strlen(name);
	rc = sfcb_open_loc(cf->cf_sfcb, &wr_loc, SETTINGS_SFCB_ID,
			   nm_len + 1 + val_len);
	if (rc) {
		return rc;
	}
	(void)sfcb_write_loc(&wr_loc, name, nm_len);
	(void)sfcb_write_loc(&wr_loc, "=", 1);
	(void)sfcb_write_loc(&wr_loc, value, val_len);
	rc = sfcb_close_loc(&wr_loc);
	return rc;
}

int settings_sfcb_compress(sfcb_fs *fs)
{
	int rc, rc1, rc2, i;
	sfcb_loc loc_compress, loc_walk;
	sfcb_ate *ate_compress, *ate_walk;
	bool copy;
	u16_t compress_sector;
	u8_t cdata[CONFIG_SFCB_WBS], wdata[CONFIG_SFCB_WBS];
	u8_t *cdp, *wdp;

	rc = sfcb_start_loc(fs, &loc_compress);
	if (rc) {
		return rc;
	}
	rc = sfcb_next_loc(&loc_compress);
	if (rc) {
		return rc;
	}
	rc = sfcb_compress_sector(fs, &compress_sector);
	if (rc) {
		return rc;
	}

	while (loc_compress.sector == compress_sector) {

		copy = true;
		ate_compress = sfcb_get_ate(&loc_compress);
		if (ate_compress->id != SETTINGS_SFCB_ID) {
			continue;
		}

		loc_walk = loc_compress;
		while (!sfcb_next_loc(&loc_walk)) {
			ate_walk = sfcb_get_ate(&loc_walk);
			if (ate_walk->id != SETTINGS_SFCB_ID) {
				continue;
			}
			while (1) {
				rc1 = sfcb_read_loc(&loc_compress, &cdata,
					CONFIG_SFCB_WBS);
				rc2 = sfcb_read_loc(&loc_walk, &wdata,
					CONFIG_SFCB_WBS);
				rc = MIN(rc1, rc2);
				if (!rc) {
					break;
				}
				cdp = cdata;
				wdp = wdata;
				for (i = 0; i < rc; i++) {
					if (*wdp != *cdp) {
						break;
					}
					if ((*cdp == '=') || (*wdp == '=')) {
						break;
					}
					cdp++;
					wdp++;
				}
				if (i == CONFIG_SFCB_WBS) {
					continue;
				}
				if (*wdp != *cdp) {
					/* quit early when the walk location
					 * has no information about the compress
					 */
					break;
				}
				if (*cdp == '=') {
					/* we have found the same variable */
					copy = false;
					break;
				}
			}
			if (!copy) {
				break;
			}
			(void)sfcb_rewind_loc(&loc_compress);
		}

		if ((copy) && (ate_compress->len)) {
			rc = sfcb_copy_loc(&loc_compress);
			if (rc) {
				return rc;
			}
		}

		if (sfcb_next_loc(&loc_compress)) {
			break;
		}
	}
	return 0;
}

/* Initialize the sfcb backend. */
int settings_sfcb_backend_init(struct settings_sfcb *cf)
{
	int rc;

	rc = sfcb_mount(cf->cf_sfcb);
	if (rc) {
		return rc;
	}
	return 0;
}

#if defined(CONFIG_SETTINGS_SFCB_DEFAULT)

const sfcb_fs_cfg settings_sfcb_cfg = {
	.offset = DT_FLASH_AREA_STORAGE_OFFSET,
	.sector_size = DT_FLASH_ERASE_BLOCK_SIZE,
	.sector_cnt = DT_FLASH_AREA_STORAGE_SIZE /
		      DT_FLASH_ERASE_BLOCK_SIZE,
	.dev_name = DT_FLASH_AREA_STORAGE_DEV,
};

// variables used by the filesystem
sfcb_fs settings_sfcb = {
    .cfg = &settings_sfcb_cfg,
};

struct settings_sfcb setting_store = {
    .cf_sfcb = &settings_sfcb,
};

int settings_backend_init(void)
{
	int rc;

	rc = settings_sfcb_backend_init(&setting_store);
    	// reformat if we can't mount the filesystem
    	if (rc) {
        	sfcb_format(&settings_sfcb);
        	settings_sfcb_backend_init(&setting_store);
    	}
    	rc = settings_sfcb_src(&setting_store);
    	rc = settings_sfcb_dst(&setting_store);
	return rc;
}
#endif /* defined(CONFIG_SETTINGS_SFCB_DEFAULT) */
