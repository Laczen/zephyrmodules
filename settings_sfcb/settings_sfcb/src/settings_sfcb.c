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

/**
 * @brief settings_sfcb_read_name
 *
 * Reads the name from a location and returns the name length
 *
 * @param loc: Pointer to location
 * @param name: buffer to store name in
 * @param name buffer length
 * @retval >=0: OK
 * @retval < 0: -ERRCODE
 */
static int settings_sfcb_read_name(sfcb_loc *loc, char *name, size_t len)
{
	ssize_t rc, i;

	rc = sfcb_read_loc(loc, name, len);
	if (rc < 0) {
		return rc;
	}

	for (i = 0; i < rc; i++) {
		if (name[i] == '=') {
			name[i] = '\0';
			break;
		}
	}

	if (i == rc) {
		return -ENODATA;
	}
	return i;
}

/**
 * @brief settings_sfcb_check_duplicate
 *
 * Searches for entries that use the same name, starting from loc
 *
 * @param loc: Pointer to location
 * @retval true: a later entry with the same name exists
 * @retval false: a later entry with the same name does not exist
 */
static bool settings_sfcb_check_duplicate(const sfcb_loc *loc,
	const char * const name)
{
	sfcb_loc loc1;

	loc1 = *loc;
	while (!sfcb_next_loc(&loc1)) {
		sfcb_ate *ate1;
		u8_t name1[SETTINGS_MAX_NAME_LEN + SETTINGS_EXTRA_LEN + 1];

		ate1 = sfcb_get_ate(&loc1);
		if (ate1->id != SETTINGS_SFCB_ID) {
			continue;
		}

		if (settings_sfcb_read_name(&loc1, name1, sizeof(name1)) <= 0) {
			continue;
		}

		if (!strcmp(name, name1)) {
			return true;
		}
	}
	return false;
}

static int settings_sfcb_load(struct settings_store *cs,
			      const struct settings_load_arg *arg)
{
	int rc;
	struct settings_sfcb *cf = (struct settings_sfcb *)cs;
	struct settings_sfcb_read_fn_arg read_fn_arg;
	sfcb_ate *load_ate;
	char name[SETTINGS_MAX_NAME_LEN + SETTINGS_EXTRA_LEN + 1];
	int name_len, val_len;

	rc = sfcb_start_loc(cf->cf_sfcb, &read_fn_arg.loc);
	if (rc) {
		return rc;
	}

	while (!sfcb_next_loc(&read_fn_arg.loc)) {

		load_ate = sfcb_get_ate(&read_fn_arg.loc);
		if (load_ate->id != SETTINGS_SFCB_ID) {
			continue;
		}

		name_len = settings_sfcb_read_name(&read_fn_arg.loc, name,
			sizeof(name));
		if (name_len < 0) {
			continue;
		}

		val_len = load_ate->len - name_len - 1;
		if (!val_len) {
			continue;
		}

		if (settings_sfcb_check_duplicate(&read_fn_arg.loc, name)) {
			continue;
		}

		/* set the read position just after '=' */
		sfcb_setpos_loc(&read_fn_arg.loc, name_len + 1);
		rc = settings_call_set_handler(name, val_len,
			settings_sfcb_read_fn, &read_fn_arg, (void *)arg);
		if (rc) {
			break;
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
	int rc;
	u16_t compress_sector;
	sfcb_loc loc_compress;
	sfcb_ate *ate_compress;
	char name[SETTINGS_MAX_NAME_LEN + SETTINGS_EXTRA_LEN + 1];
	int name_len, val_len;

	rc = sfcb_compress_sector(fs, &compress_sector);
	if (rc) {
		return rc;
	}

	rc = sfcb_start_loc(fs, &loc_compress);
	if (rc) {
		return rc;
	}

	while (sfcb_next_loc(&loc_compress)) {

		if (loc_compress.sector != compress_sector) {
			break;
		}

		ate_compress = sfcb_get_ate(&loc_compress);
		if (ate_compress->id != SETTINGS_SFCB_ID) {
			continue;
		}

		name_len = settings_sfcb_read_name(&loc_compress, name,
			sizeof(name));
		if (name_len < 0) {
			continue;
		}

		val_len = ate_compress->len - name_len - 1;
		if (!val_len) {
			continue;
		}

		if (settings_sfcb_check_duplicate(&loc_compress, name)) {
			continue;
		}

		rc = sfcb_copy_loc(&loc_compress);
		if (rc) {
			return rc;
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
	.size = DT_FLASH_AREA_STORAGE_SIZE,
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
