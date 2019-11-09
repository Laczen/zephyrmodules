/*  NVS2: non volatile storage in flash
 *
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sfcb.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(fs_sfcb, CONFIG_SFCB_LOG_LEVEL);

static inline u16_t sfcb_align_down(u16_t len)
{
	return len &= ~(CONFIG_SFCB_WBS - 1U);
}

static inline u16_t sfcb_align_up(u16_t len)
{
	return sfcb_align_down(len + CONFIG_SFCB_WBS - 1U);
}

static inline void sfcb_lock(sfcb_fs *fs)
{
	k_mutex_lock(&fs->mutex, K_FOREVER);
}

static inline void sfcb_unlock(sfcb_fs *fs)
{
	k_mutex_unlock(&fs->mutex);
}

/**
 * Sequence comparison of two u16_t
 *
 * if a > b returns positive sequence difference
 * if a < b returns negative sequence difference
 * if a == b returns 0
 */
static inline s16_t sfcb_scmp(u16_t a, u16_t b) {
    return (s16_t)(a - b);
}

/*
 * Unaligned write using a cache to read from at unaligned start and to store
 * remainder at unaligned end.
 */
static int sfcb_flash_write(sfcb_fs *fs, u16_t sec, u16_t sec_off,
	const void *data, size_t len, u8_t *cache)
{
	const u8_t *data8 = (const u8_t *)data;
	off_t off = fs->cfg->offset + sec * fs->sector_size + sec_off;
	u16_t cnt = 0, rem;
	bool fp_dis = false;
	int rc = 0;

	if (!len) {
		return 0;
	}

	if ((!fs) || (!fs->flash_device) || (!data && len)) {
		return -EINVAL;
	}

	rem = sec_off & (CONFIG_SFCB_WBS - 1U);

	if (rem + len >= CONFIG_SFCB_WBS) { /* flash write required */
		rc = flash_write_protection_set(fs->flash_device, 0);
		if (rc) {
			return rc;
		}
		fp_dis = true;
	}

	/* Unaligned start */
	if (rem) {
		cnt = CONFIG_SFCB_WBS - rem;
		if (cnt > len) {
			cnt = len;
		}
		if (cache) {
			memcpy(cache + rem, data8, cnt);
		}
		data8 += cnt;
		len -= cnt;
		off -= rem;
	}

	if (!len && !fp_dis) {
		return 0;
	}

	if (rem && cache) {
		rc = flash_write(fs->flash_device, off, cache, CONFIG_SFCB_WBS);
		if (rc) {
			goto END;
		}
		off += CONFIG_SFCB_WBS;
	}

	/* Aligned write */
	cnt = len & ~(CONFIG_SFCB_WBS - 1U);
	if (cnt) {
		rc = flash_write(fs->flash_device, off, data8, cnt);
		if (rc) {
			goto END;
		}
		len -= cnt;
		off += cnt;
		data8 += cnt;
	}

	/* Unaligned remainder */
	if (len && cache) {
		memset(cache, 0xff, CONFIG_SFCB_WBS);
		memcpy(cache, data8, len);
	}
END:
	if (fp_dis) {
		(void)flash_write_protection_set(fs->flash_device, 1);
	}

	return rc;
}

/* sfcb_flash_read assumes unaligned reads are possible */
static int sfcb_flash_read(sfcb_fs *fs, u16_t sec, u16_t sec_off, void *data,
			   size_t len)
{
	u8_t *data8 = (u8_t *)data;
	off_t off = fs->cfg->offset + sec * fs->sector_size + sec_off;

	if (!len) {
		return 0;
	}

	if ((!fs) || (!fs->flash_device) || (!data && len)) {
		return -EINVAL;
	}

	return flash_read(fs->flash_device, off, data8, len);
}

static int sfcb_flash_sector_erase(sfcb_fs *fs, u16_t sector)
{
	off_t offset;

	if ((!fs) || (!fs->flash_device)) {
		return -EINVAL;
	}

	if (flash_write_protection_set(fs->flash_device, 0)) {
		return -ENXIO;
	}

	offset = fs->cfg->offset + sector * fs->sector_size;

	LOG_DBG("Erasing flash sector at %lx", (long int) offset);
	if (flash_erase(fs->flash_device, offset, fs->sector_size)) {
		return -ENXIO;
	}

	(void) flash_write_protection_set(fs->flash_device, 1);

	return 0;
}

static int sfcb_cmp_const(const void *data, u8_t value, size_t len)
{
	const u8_t *data8 = (const u8_t *)data;
	int i;

	if ((!data) || (!len)) {
		return -EINVAL;
	}

	for (i=0; i < len; i++) {
		if (data8[i] != value) {
			return data8[i] - value;
		}
	}

	return 0;
}

static void sfcb_crc8_update(void *data, size_t len)
{
	u8_t *data8 = (u8_t *)data;

	if ((!data) || (!len)) {
		return;
	}

	data8[len - 1] = crc8_ccitt(0xff, data8, len - 1);
}

static int sfcb_crc8_verify(const void *data, size_t len)
{
	const u8_t *data8 = (const u8_t *)data;

	if ((!data) || (!len)) {
		return -EINVAL;
	}

	return (int)(data8[len -1] - crc8_ccitt(0xff, data8, len - 1));
}

static int sfcb_flash_read_crc8_verify(sfcb_fs *fs, u16_t sec, u16_t sec_off,
				       void *data, size_t len)
{
	int rc;

	if ((!fs) || (!data) || (!len)) {
		return -EINVAL;
	}

	rc = sfcb_flash_read(fs, sec, sec_off, data, len);
	if (rc) {
		return rc;
	}

	rc = sfcb_crc8_verify(data, len);
	return rc;
}

void sfcb_next_sector(sfcb_fs *fs, u16_t *sector) {
	*sector += 1U;
	if (*sector == fs->sector_cnt) {
		*sector = 0U;
	}
}

void sfcb_prev_sector(sfcb_fs *fs, u16_t *sector) {
	if (*sector == 0) {
		*sector = fs->sector_cnt;
	}
	*sector -= 1U;
}

static int sfcb_next_in_sector(sfcb_loc *loc)
{
	sfcb_ate *ate;
	int rc = 0;

	if (!loc) {
		return -EINVAL;
	}

#if (CONFIG_SFCB_ATE_CACHE_SIZE !=1)
	if (!loc->ate_cache_offset) {
		/* cache exhausted */
		offset = loc->ate_offset - SFCB_ATE_CACHE_BYTES;
		loc->ate_cache_offset = SFCB_ATE_CACHE_BYTES;
		rc = sfcb_flash_read(loc->fs, loc->sector, offset,
			loc->ate_cache, SFCB_ATE_CACHE_BYTES);
		if (rc) {
			return rc;
		}
	}
	loc->ate_cache_offset -= SFCB_ATE_SIZE;
	loc->ate_offset -= SFCB_ATE_SIZE;
#else
	loc->ate_offset -= SFCB_ATE_SIZE;
	rc = sfcb_flash_read(loc->fs, loc->sector, loc->ate_offset,
		loc->ate_cache, SFCB_ATE_CACHE_BYTES);
	if (rc) {
		return rc;
	}
#endif /* (CONFIG_SFCB_ATE_CACHE_SIZE != 1) */

	ate = sfcb_get_ate(loc);
	if ((!sfcb_cmp_const(ate, 0xff, SFCB_ATE_SIZE)) || (!loc->ate_offset)) {
		return -ENOENT;
	}
	return 0;
}

int sfcb_next_loc(sfcb_loc *loc)
{
	int rc;
	sfcb_ate *ate;

	if (!loc) {
		return -EINVAL;
	}

	while (1) {
		rc = sfcb_next_in_sector(loc);
		if (rc == -ENOENT) {
			/* sector end or FS end */
			if ((loc->sector == loc->fs->wr_sector) &&
			    (loc->ate_offset == loc->fs->wr_ate_offset)) {
				/* FS end */
				return -ENOENT;
			}
			/* sector end */
			sfcb_next_sector(loc->fs, &loc->sector);
			loc->ate_offset = loc->fs->sector_size;
#if (CONFIG_SFCB_ATE_CACHE_SIZE !=1)
			loc->ate_cache_offset = 0;
#endif
			continue;
		}
		if (rc) {
			return rc;
		}
		ate = sfcb_get_ate(loc);
		if (!sfcb_crc8_verify(ate, SFCB_ATE_SIZE)) {
			break;
		}
	}
	loc->data_offset = 0;
	return 0;
}

int sfcb_start_loc(sfcb_fs *fs, sfcb_loc *loc)
{
	if ((!fs) || (!loc)) {
		return -EINVAL;
	}

	loc->fs = fs;
	loc->sector = fs->wr_sector;
	sfcb_next_sector(fs, &loc->sector);
	loc->ate_offset = fs->sector_size;
#if (CONFIG_SFCB_ATE_CACHE_SIZE !=1)
	loc->ate_cache_offset = 0;
#endif
	return 0;
}

int sfcb_compress_sector(sfcb_fs *fs, u16_t *sector)
{
	if (!fs) {
		return -EINVAL;
	}
	*sector = fs->wr_sector;
	sfcb_next_sector(fs, sector);
	return 0;
}

static int sfcb_init_sector(sfcb_fs *fs)
{
	int rc;
	sfcb_sec_start sec_start;

	if (!fs) {
		return -EINVAL;
	}

	sec_start.magic = SFCB_MAGIC;
	sec_start.sec_id = fs->wr_sector_id;
	sec_start.version = SFCB_VERSION;
	sfcb_crc8_update(&sec_start, SFCB_SEC_START_SIZE);

	rc = sfcb_flash_write(fs, fs->wr_sector, 0, &sec_start,
		SFCB_SEC_START_SIZE, NULL);

	if (rc) {
		return rc;
	}

	fs->wr_sector_id++;
	fs->wr_ate_offset = fs->sector_size - SFCB_ATE_SIZE;
	fs->wr_data_offset = SFCB_SEC_START_SIZE;

	return 0;
}

static int sfcb_new_sector(sfcb_fs *fs)
{
	int rc;

	if (!fs) {
		return -EINVAL;
	}

	sfcb_next_sector(fs, &fs->wr_sector);
	rc = sfcb_flash_sector_erase(fs, fs->wr_sector);
	if (rc) {
		return rc;
	}
	rc = sfcb_init_sector(fs);
	return rc;
}

static int sfcb_config_init(sfcb_fs *fs)
{
	u8_t wbs;
	struct flash_pages_info info;
	off_t page_off, end;

	if ((!fs) || (!fs->cfg->dev_name) || (!fs->cfg->size)) {
		LOG_ERR("Cfg error - missing configuration items");
		return -EINVAL;
	}

	fs->flash_device = device_get_binding(fs->cfg->dev_name);
	if (!fs->flash_device) {
		LOG_ERR("Cfg error - wrong flash device");
		return -EINVAL;
	}

	/* Check flash alignment */
	wbs = flash_get_write_block_size(fs->flash_device);
	if ((!wbs) || (CONFIG_SFCB_WBS % wbs)) {
		LOG_ERR("Cfg error - ALIGN_SIZE no multiple of WBS");
		goto ERR;
	}

	fs->sector_size = SFCB_MIN_SECTOR_SIZE;
	/* Check flash page size and offset */
	page_off = fs->cfg->offset;
	end = page_off + fs->cfg->size;

	while (page_off < end) {

		if (flash_get_page_info_by_offs(fs->flash_device, page_off,
			&info)) {
			LOG_ERR("Cfg error - unable to get page info");
			goto ERR;
		}

		if (page_off != info.start_offset) {
			LOG_ERR("Cfg error - offset not page aligned");
			goto ERR;
		}

		if (info.size > UINT16_MAX) {
			LOG_ERR("Cfg error - flash sector to large");
			goto ERR;
		}

		if (info.size > fs->sector_size) {
			fs->sector_size = info.size;
		}

		page_off += info.size;
	}

	fs->sector_cnt = fs->cfg->size / fs->sector_size;
	if (fs->compress && (fs->sector_cnt < 2)) {
		LOG_ERR("Cfg error - insufficient sectors for compress");
		goto ERR;
	}
	fs->flash_device = NULL;
	return 0;

ERR:
	fs->flash_device = NULL;
	return -EINVAL;
}

static int sfcb_fs_check(sfcb_fs *fs) {
	int rc = 0;
	u16_t i;
	sfcb_sec_start sec_start;

	if (!fs) {
		return -EINVAL;
	}

	fs->wr_sector = fs->sector_cnt;
	fs->wr_sector_id = 0U;

	for (i = 0; i < fs->sector_cnt; i++) {
		/* search for last sector */
		rc = sfcb_flash_read_crc8_verify(fs, i, 0, &sec_start,
			SFCB_SEC_START_SIZE);

		if (!rc) {
			if (fs->wr_sector == fs->sector_cnt) {
				fs->wr_sector_id = sec_start.sec_id;
				fs->wr_sector = i;
			}
			if (sfcb_scmp(sec_start.sec_id, fs->wr_sector_id) > 0) {
				fs->wr_sector_id = sec_start.sec_id;
				fs->wr_sector = i;
			}
			continue;
		}

		if (rc < 0) {
			return rc;
		}
	}

	if (fs->wr_sector == fs->sector_cnt) {
		/* This is not a sfcb FS or it is empty */
		return -EACCES;
	}

	return 0;
}

int sfcb_format(sfcb_fs *fs)
{
	int rc;
	sfcb_ate ate;
	u16_t i;

	if (!fs) {
		return -EINVAL;
	}

	if (fs->flash_device) {
		return -EBUSY;
	}

	rc = sfcb_config_init(fs);
	if (rc) {
		return rc;
	}

	fs->flash_device = device_get_binding(fs->cfg->dev_name);

	for (i = 0; i < fs->sector_cnt; i++) {
		/* erase sector 0 and all sectors that do not have empty first
		 * ATE
		 */
		rc = sfcb_flash_read(fs, i, fs->sector_size -
			SFCB_ATE_SIZE, &ate, SFCB_ATE_SIZE);
		if (rc) {
			goto END;
		}

		if ((!i) || (sfcb_cmp_const(&ate, 0xff, SFCB_ATE_SIZE))) {
			rc = sfcb_flash_sector_erase(fs, i);
			if (rc) {
				goto END;
			}
		}
	}

	fs->wr_sector = 0;
	fs->wr_sector_id = 0U;
	rc = sfcb_init_sector(fs);
	if (rc) {
		goto END;
	}

END:
	fs->flash_device = NULL;
	return rc;
}

int sfcb_fs_init(sfcb_fs *fs)
{
	int rc;
	u16_t data_offset;
	sfcb_ate ate;
	u8_t buf[CONFIG_SFCB_WBS];

	if (!fs) {
		return -EINVAL;
	}

	fs->wr_sector_id++;
	fs->wr_ate_offset = fs->sector_size;
	fs->wr_data_offset = SFCB_SEC_START_SIZE;

	/* update wr_ate_offset */
	while (fs->wr_ate_offset > SFCB_SEC_START_SIZE) {
		/* search for first empty ate */
		fs->wr_ate_offset -= SFCB_ATE_SIZE;
		rc = sfcb_flash_read_crc8_verify(fs, fs->wr_sector,
			fs->wr_ate_offset, &ate, SFCB_ATE_SIZE);

		if (rc < 0) {
			return rc;
		}

		if (!rc) {
			fs->wr_data_offset = ate.offset;
			fs->wr_data_offset += sfcb_align_up(ate.len);
			continue;
		}

		if (!sfcb_cmp_const(&ate, 0xff, SFCB_SEC_START_SIZE)) {
			/* empty ate */
			break;
		}
	}

	/* update fs->wr_data_offset */
	data_offset = fs->wr_ate_offset;
	while (fs->wr_data_offset < data_offset) {
		/* search for first last non empty item in flash */
		data_offset -= CONFIG_SFCB_WBS;
		rc = sfcb_flash_read(fs, fs->wr_sector, data_offset, &buf,
				 CONFIG_SFCB_WBS);
		if (rc) {
			return rc;
		}
		if (sfcb_cmp_const(&buf, 0xff, CONFIG_SFCB_WBS)) {
			/* found non-empty area */
			data_offset += CONFIG_SFCB_WBS;
			break;
		}
	}

	if (data_offset > fs->wr_data_offset) {
		fs->wr_data_offset = data_offset;
	}

	if (fs->compress && (fs->sector_cnt > 1)) {
		/* compress might have been interrupted call it again, if it
		 * fails (this will be due to insufficient space) erase the
		 * current write sector and restart compress */
		rc = fs->compress(fs);
		if (rc) {
			sfcb_prev_sector(fs, &fs->wr_sector);
			fs->wr_sector_id--;
			rc = sfcb_new_sector(fs);
			if (rc) {
				return rc;
			}

			rc = fs->compress(fs);
			if (rc) {
				return rc;
			}
		}
	}

	LOG_INF("SFCB initialized: WR_SECTOR %x, WR_ATE %x, WR_DATA %x",
		fs->wr_sector, fs->wr_ate_offset, fs->wr_data_offset);
	return 0;
}

int sfcb_mount(sfcb_fs *fs)
{
	int rc;
	if (!fs) {
		return -EINVAL;
	}

	if (fs->flash_device) {
		return -EBUSY;
	}

	k_mutex_init(&fs->mutex);

	sfcb_lock(fs);

	rc = sfcb_config_init(fs);
	if (rc) {
		return rc;
	}

	fs->flash_device = device_get_binding(fs->cfg->dev_name);
	rc = sfcb_fs_check(fs);
	if (rc) {
		goto END;
	}

	rc = sfcb_fs_init(fs);
	if (rc) {
		goto END;
	}

END:
	sfcb_unlock(fs);
	if (rc) {
		fs->flash_device = NULL;
	}
	return rc;
}

int sfcb_unmount(sfcb_fs *fs)
{
	if (!fs) {
		return -EINVAL;
	}
	fs->flash_device = NULL;
	return 0;
}

static int sfcb_init_loc(sfcb_fs *fs, sfcb_loc *loc, u16_t id, u16_t len)
{
	u16_t req_space;
	sfcb_ate *ate;

	if ((!fs) || (!loc)) {
		return -EACCES;
	}
	/* Always leave space for empty ATE */
	req_space = sfcb_align_up(len) + SFCB_ATE_SIZE;
	if ((fs->wr_ate_offset - fs->wr_data_offset) < req_space) {
		return -ENOMEM;
	}

	loc->fs = fs;
	loc->sector = fs->wr_sector;
	loc->data_offset = 0;
	loc->ate_offset = fs->wr_ate_offset;

#if (CONFIG_SFCB_ATE_CACHE_SIZE !=1)
	loc->ate_cache_offset = 0;
#endif /* (CONFIG_SFCB_ATE_CACHE_SIZE !=1) */

#if (!IS_ENABLED(CONFIG_SFCB_FLASH_SUPPORTS_UNALIGNED_WRITE))
	memset(&loc->dcache, 0xff, CONFIG_SFCB_WBS);
#endif /* (!IS_ENABLED(CONFIG_SFCB_FLASH_SUPPORTS_UNALIGNED_WRITE)) */

	ate = sfcb_get_ate(loc);
	ate->id = id;
	ate->len = len;
	ate->offset = fs->wr_data_offset;
	return 0;
}

int sfcb_open_loc(sfcb_fs *fs, sfcb_loc *loc, u16_t id, u16_t len)
{
	int rc, nscnt = 0;

	if ((!fs) || (!loc)) {
		return -EINVAL;
	}

	while (1) {
		rc = sfcb_init_loc(fs, loc, id, len);
		if (rc != -ENOMEM) {
			break;
		}
		/* open new sector */
		rc = sfcb_new_sector(fs);
		if (rc) {
			return rc;
		}
		/* call gc */
		if (fs->compress && (fs->sector_cnt > 1)) {
			sfcb_lock(fs);
			rc = fs->compress(fs);
			sfcb_unlock(fs);
		}
		nscnt++;
		if (nscnt == fs->sector_cnt) {
			return -ENOMEM;
		}
	}

	if (!rc) {
		sfcb_lock(loc->fs);
	}
	return rc;
}

static int sfcb_close_loc_no_unlock(sfcb_loc *loc)
{
	int rc;
	sfcb_ate *ate;

#if (!IS_ENABLED(CONFIG_SFCB_FLASH_SUPPORTS_UNALIGNED_WRITE))
	u16_t data_offset;
	data_offset = sfcb_align_down(loc->data_offset);
	if (loc->data_offset != data_offset) {
		data_offset += loc->fs->wr_data_offset;
		rc = sfcb_flash_write(loc->fs, loc->fs->wr_sector, data_offset,
			loc->dcache, CONFIG_SFCB_WBS, NULL);
		if (rc) {
			return rc;
		}
	}
#endif /* (!IS_ENABLED(CONFIG_SFCB_FLASH_SUPPORTS_UNALIGNED_WRITE)) */

	ate = sfcb_get_ate(loc);
	sfcb_crc8_update(ate, SFCB_ATE_SIZE);

	rc = sfcb_flash_write(loc->fs, loc->fs->wr_sector,
		loc->fs->wr_ate_offset, ate, SFCB_ATE_SIZE, NULL);
	if (rc) {
		return rc;
	}

	loc->fs->wr_data_offset += sfcb_align_up(ate->len);
	loc->fs->wr_ate_offset -= SFCB_ATE_SIZE;

	return 0;
}

int sfcb_close_loc(sfcb_loc *loc) {
	int rc;

	if (!loc) {
		return -EINVAL;
	}
	if ((!loc->fs) || (loc->ate_offset != loc->fs->wr_ate_offset) ||
	    (loc->sector != loc->fs->wr_sector)) {
		return -EACCES;
	}

	rc = sfcb_close_loc_no_unlock(loc);
	sfcb_unlock(loc->fs);
	return rc;
}

int sfcb_rewind_loc(sfcb_loc *loc)
{
	if (!loc) {
		return -EINVAL;
	}

	if ((loc->ate_offset == loc->fs->wr_ate_offset) &&
	    (loc->sector == loc->fs->wr_sector)) {
		return -EACCES;
	}

	loc->data_offset = 0;
	return 0;
}

int sfcb_setpos_loc(sfcb_loc *loc, u16_t pos)
{
	sfcb_ate *ate;

	if (!loc) {
		return -EINVAL;
	}

	if ((loc->ate_offset == loc->fs->wr_ate_offset) &&
	    (loc->sector == loc->fs->wr_sector)) {
		return -EACCES;
	}

	ate = sfcb_get_ate(loc);
	if (pos > ate->len) {
		return -EINVAL;
	}

	loc->data_offset = pos;
	return 0;
}

ssize_t sfcb_write_loc(sfcb_loc *loc, const void *data, size_t len)
{
	int rc;
	sfcb_ate *ate;
	u16_t data_offset;

	if (!loc) {
		return -EINVAL;
	}

	if ((loc->ate_offset != loc->fs->wr_ate_offset) ||
	    (loc->sector != loc->fs->wr_sector)) {
		return -EACCES;
	}

	ate = sfcb_get_ate(loc);
	if (loc->data_offset + len > ate->offset + ate->len) {
		return -ENOSPC;
	}

	sfcb_unlock(loc->fs);
	data_offset = loc->fs->wr_data_offset + loc->data_offset;

#if IS_ENABLED(CONFIG_SFCB_FLASH_SUPPORTS_UNALIGNED_WRITE)
	rc = sfcb_flash_write(loc->fs, loc->sector, data_offset, data, len,
			      NULL);
#else
	rc = sfcb_flash_write(loc->fs, loc->sector, data_offset, data, len,
			      loc->dcache);
#endif /* IS_ENABLED(CONFIG_SFCB_FLASH_SUPPORTS_UNALIGNED_WRITE) */

	sfcb_lock(loc->fs);
	if (rc) {
		return rc;
	}
	loc->data_offset += len;

	return len;
}

ssize_t sfcb_read_loc(sfcb_loc *loc, void *data, size_t len)
{
	int rc;
	sfcb_ate *ate;
	u16_t data_offset;

	if ((!loc) || (!loc->fs)) {
		return -EACCES;
	}

	ate = sfcb_get_ate(loc);

	if (loc->data_offset + len > ate->len) {
		len = ate->len - loc->data_offset;
	}

	data_offset = ate->offset + loc->data_offset;
	rc = sfcb_flash_read(loc->fs, loc->sector, data_offset, data, len);
	if (rc) {
		return rc;
	}
	loc->data_offset += len;

	return len;
}

int sfcb_copy_loc(sfcb_loc *loc) {
	int rc;
	sfcb_loc newloc;
	sfcb_ate *ate;
	u16_t len, rd_len;
	u8_t buf[CONFIG_SFCB_WBS];

	if ((!loc) || (!loc->fs)) {
		return -EINVAL;
	}

	ate = sfcb_get_ate(loc);

	if (sfcb_crc8_verify(ate, SFCB_ATE_SIZE)) {
		return -EINVAL;
	}

	if ((loc->ate_offset == loc->fs->wr_ate_offset) &&
	    (loc->sector == loc->fs->wr_sector)) {
		return -EACCES;
	}

	rc = sfcb_init_loc(loc->fs, &newloc, ate->id, ate->len);
	if (rc) {
		return rc;
	}

	/* Rewind the loc */
	(void)sfcb_rewind_loc(loc);
	len = ate->len;
	while (len) {
		rd_len = sfcb_read_loc(loc, &buf, sizeof(buf));
		if (rd_len < 0) {
			return rd_len;
		}
		rc = sfcb_write_loc(&newloc, &buf, rd_len);
		if (rc < 0) {
			return rc;
		}
		len -= rd_len;
	}
	rc = sfcb_close_loc_no_unlock(&newloc);
	return rc;
}

ssize_t sfcb_write(sfcb_fs *fs, u16_t id, const void *data, size_t len)
{
	int rc;
	sfcb_loc loc;

	rc = sfcb_open_loc(fs, &loc, id, len);
	if (rc) {
		return rc;
	}
	rc = sfcb_write_loc(&loc, data, len);
	if (rc < 0) {
		return rc;
	}
	rc = sfcb_close_loc(&loc);
	if (rc) {
		return rc;
	}
	return len;
}

ssize_t sfcb_read(sfcb_fs *fs, u16_t id, void *data, size_t len)
{
	int rc;
	sfcb_loc loc, loc_walk;
	sfcb_ate *ate;
	bool read = false;

	rc = sfcb_start_loc(fs, &loc_walk);
	if (rc) {
		return rc;
	}

	while (!sfcb_next_loc(&loc_walk)) {
		ate = sfcb_get_ate(&loc_walk);
		if (ate->id == id) {
			loc = loc_walk;
			read = true;
		}
	}

	if (!read) {
		return -ENOENT;
	}

	return sfcb_read_loc(&loc, data, len);

}