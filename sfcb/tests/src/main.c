/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <sfcb.h>
#include <flash.h>
#include <ztest.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(main);

//#define DT_FLASH_SIM_ERASE_BLOCK_SIZE DT_FLASH_ERASE_BLOCK_SIZE

const sfcb_fs_cfg cfg = {
	.offset = DT_FLASH_AREA_STORAGE_OFFSET,
	.sector_size = DT_FLASH_ERASE_BLOCK_SIZE,
	.sector_cnt = DT_FLASH_AREA_STORAGE_SIZE /
		      DT_FLASH_ERASE_BLOCK_SIZE,
	.dev_name = DT_FLASH_AREA_STORAGE_DEV,
};

const sfcb_fs_cfg cfg2sector = {
	.offset = DT_FLASH_AREA_STORAGE_OFFSET,
	.sector_size = DT_FLASH_ERASE_BLOCK_SIZE,
	.sector_cnt = 2,
	.dev_name = DT_FLASH_AREA_STORAGE_DEV,
};

const sfcb_fs_cfg cfg1sector = {
	.offset = DT_FLASH_AREA_STORAGE_OFFSET,
	.sector_size = DT_FLASH_ERASE_BLOCK_SIZE,
	.sector_cnt = 1,
	.dev_name = DT_FLASH_AREA_STORAGE_DEV,
};

const sfcb_fs_cfg badcfg1 = { /* missing dev name */
	.offset = DT_FLASH_AREA_STORAGE_OFFSET,
	.sector_size = DT_FLASH_ERASE_BLOCK_SIZE,
	.sector_cnt = DT_FLASH_AREA_STORAGE_SIZE /
		      DT_FLASH_ERASE_BLOCK_SIZE,
};

const sfcb_fs_cfg badcfg2 = { /* unaligned offset */
	.offset = DT_FLASH_AREA_STORAGE_OFFSET - 1,
	.sector_size = DT_FLASH_ERASE_BLOCK_SIZE,
	.sector_cnt = DT_FLASH_AREA_STORAGE_SIZE /
		      DT_FLASH_ERASE_BLOCK_SIZE,
	.dev_name = DT_FLASH_AREA_STORAGE_DEV,
};

const sfcb_fs_cfg badcfg3 = { /* unaligned sector size */
	.offset = DT_FLASH_AREA_STORAGE_OFFSET,
	.sector_size = DT_FLASH_ERASE_BLOCK_SIZE - 1,
	.sector_cnt = DT_FLASH_AREA_STORAGE_SIZE /
		      DT_FLASH_ERASE_BLOCK_SIZE,
	.dev_name = DT_FLASH_AREA_STORAGE_DEV,
};

const sfcb_fs_cfg badcfg4 = { /* wrong sector size */
	.offset = DT_FLASH_AREA_STORAGE_OFFSET,
	.sector_size = 0,
	.sector_cnt = DT_FLASH_AREA_STORAGE_SIZE /
		      DT_FLASH_ERASE_BLOCK_SIZE,
	.dev_name = DT_FLASH_AREA_STORAGE_DEV,
};

sfcb_fs sfcb = {
	.cfg = &cfg,
};


void test_sfcb_mount(void)
{
	int rc;

#if IS_ENABLED(CONFIG_SFCB_ENABLE_CFG_CHECK)
	sfcb.cfg = &badcfg1;
	rc = sfcb_mount(&sfcb);
	zassert_false(rc == 0, "Mount succeeded for bad config");

	sfcb.cfg = &badcfg2;
	rc = sfcb_mount(&sfcb);
	zassert_false(rc == 0, "Mount succeeded for bad config");

	sfcb.cfg = &badcfg3;
	rc = sfcb_mount(&sfcb);
	zassert_false(rc == 0, "Mount succeeded for bad config");

	sfcb.cfg = &badcfg4;
	rc = sfcb_mount(&sfcb);
	zassert_false(rc == 0, "Mount succeeded for bad config");
#endif /* IS_ENABLED(CONFIG_SFCB_ENABLE_CFG_CHECK) */

	sfcb.cfg = &cfg;
	rc = sfcb_format(&sfcb);
	zassert_true(rc == 0, "Format failed [%d]", rc);
	rc = sfcb_mount(&sfcb);
	zassert_true(rc == 0, "Mount failed [%d]", rc);

	rc = sfcb_mount(&sfcb);
	zassert_false(rc == 0, "Remount succeeded [%d]", rc);

	rc = sfcb_format(&sfcb);
	zassert_false(rc == 0, "Format succeeded on mounted FS [%d]", rc);

	rc = sfcb_unmount(&sfcb);
	zassert_true(rc == 0, "Unmount failed");

	rc = sfcb_format(&sfcb);
	zassert_true(rc == 0, "Format failed [%d]", rc);

}

void test_sfcb_loc(void)
{
	int rc;
	sfcb_loc loc;
	u16_t data_size = 16U;
	u16_t exp_offset, exp_sector = 1U;

	sfcb.cfg = &cfg;
	rc = sfcb_format(&sfcb);
	zassert_true(rc == 0, "Format failed [%d]", rc);
	rc = sfcb_mount(&sfcb);
	zassert_true(rc == 0, "Mount failed [%d]", rc);

	/* Opening and closing of first entry */
	rc = sfcb_open_loc(&sfcb, &loc, 0, data_size);
	zassert_true(rc == 0, "open loc failed [%d]", rc);
	rc = sfcb_close_loc(&loc);
	zassert_true(rc == 0, "close loc failed [%d]", rc);

	exp_offset = cfg.sector_size;
	exp_offset -= (2 * SFCB_ATE_SIZE);
	zassert_true(sfcb.wr_ate_offset == exp_offset, "Wrong ate offset");

	exp_offset = SFCB_SEC_START_SIZE + data_size;
	zassert_true(sfcb.wr_data_offset == exp_offset, "Wrong data offset");

	/* Unmount and remount to see if we get the same result */
	rc = sfcb_unmount(&sfcb);
	zassert_true(rc == 0, "Unmount failed [%d]", rc);

	rc = sfcb_mount(&sfcb);
	zassert_true(rc == 0, "Mount failed [%d]", rc);

	exp_offset = cfg.sector_size;
	exp_offset -= (2 * SFCB_ATE_SIZE);
	zassert_true(sfcb.wr_ate_offset == exp_offset, "Wrong ate offset");

	exp_offset = SFCB_SEC_START_SIZE + data_size;
	zassert_true(sfcb.wr_data_offset == exp_offset, "Wrong data offset");

	/* Opening and closing until we get to second sector */
	while (sfcb.wr_sector != exp_sector) {
		rc = sfcb_open_loc(&sfcb, &loc, 0, data_size);
		zassert_true(rc == 0, "open loc failed [%d]", rc);
		rc = sfcb_close_loc(&loc);
		zassert_true(rc == 0, "close loc failed [%d]", rc);
	}
	exp_offset = cfg.sector_size;
	exp_offset -= (2 * SFCB_ATE_SIZE);
	zassert_true(sfcb.wr_ate_offset == exp_offset, "Wrong ate offset");

	exp_offset = SFCB_SEC_START_SIZE + data_size;
	zassert_true(sfcb.wr_data_offset == exp_offset, "Wrong data offset");

	/* Unmount and remount to see if we get the same result */
	rc = sfcb_unmount(&sfcb);
	zassert_true(rc == 0, "Unmount failed [%d]", rc);

	rc = sfcb_mount(&sfcb);
	zassert_true(rc == 0, "Mount failed [%d]", rc);

	zassert_true(sfcb.wr_sector == exp_sector, "Wrong sector");

	exp_offset = cfg.sector_size;
	exp_offset -= (2 * SFCB_ATE_SIZE);
	zassert_true(sfcb.wr_ate_offset == exp_offset, "Wrong ate offset");

	exp_offset = SFCB_SEC_START_SIZE + data_size;
	zassert_true(sfcb.wr_data_offset == exp_offset, "Wrong data offset");

	rc = sfcb_unmount(&sfcb);
	zassert_true(rc == 0, "Unmount failed [%d]", rc);
}

void test_sfcb_loc_first(void) {
	int rc;
	sfcb_loc loc;
	u16_t data_size = 16U, sector;

	/*
	 * Test if we can find first loc for different config options.
	 */

	LOG_INF("1 Sector Configuration");
	/* 1 sector config */
	sfcb.cfg = &cfg1sector;
	rc = sfcb_mount(&sfcb);
	zassert_true(rc == 0, "Mount failed [%d]", rc);

	rc = sfcb_first_loc(&sfcb, &loc);
	zassert_true(rc == 0, "first loc failed");
	rc = loc.sector - 0U;
	zassert_true(rc == 0, "first loc wrong sector");
	rc = loc.ate_offset - (cfg.sector_size - SFCB_ATE_SIZE);
	zassert_true(rc == 0, "first loc wrong offset");

	rc = sfcb_unmount(&sfcb);
	zassert_true(rc == 0, "Unmount failed [%d]", rc);

	LOG_INF("2 Sector Configuration");
	/* 2 sector config */
	sfcb.cfg = &cfg2sector;
	rc = sfcb_mount(&sfcb);
	zassert_true(rc == 0, "Mount failed [%d]", rc);

	rc = sfcb_first_loc(&sfcb, &loc);
	zassert_true(rc == 0, "first loc failed");
	rc = loc.sector - 0U;
	zassert_true(rc == 0, "first loc wrong sector");
	rc = loc.ate_offset - (cfg.sector_size - SFCB_ATE_SIZE);
	zassert_true(rc == 0, "first loc wrong offset");

	rc = sfcb_unmount(&sfcb);
	zassert_true(rc == 0, "Unmount failed [%d]", rc);

	LOG_INF("Multi Sector Configuration");
	/* multi sector config */
	sfcb.cfg = &cfg;
	rc = sfcb_mount(&sfcb);
	zassert_true(rc == 0, "Mount failed [%d]", rc);

	rc = sfcb_first_loc(&sfcb, &loc);
	zassert_true(rc == 0, "first loc failed");
	rc = loc.sector - 0U;
	zassert_true(rc == 0, "first loc wrong sector");
	rc = loc.ate_offset - (cfg.sector_size - SFCB_ATE_SIZE);
	zassert_true(rc == 0, "first loc wrong offset");

	rc = sfcb_unmount(&sfcb);
	zassert_true(rc == 0, "Unmount failed [%d]", rc);

	/*
	 * Test if we can find the correct compression sector.
	 */
	LOG_INF("1 Sector Configuration");
	sfcb.cfg = &cfg1sector;
	rc = sfcb_mount(&sfcb);
	zassert_true(rc == 0, "Mount failed [%d]", rc);

	rc = sfcb_compress_sector(&sfcb, &sector);
	zassert_true(rc == 0, "Failed to get compress sector");
	rc = sfcb_first_loc(&sfcb, &loc);
	zassert_true(rc == 0, "Failed to get first loc");
	rc = sector - loc.sector;
	zassert_true(rc == 0, "Wrong compress sector");

	rc = sfcb_unmount(&sfcb);
	zassert_true(rc == 0, "Unmount failed [%d]", rc);

	LOG_INF("2 Sector Configuration");
	sfcb.cfg = &cfg2sector;
	rc = sfcb_mount(&sfcb);
	zassert_true(rc == 0, "Mount failed [%d]", rc);

	rc = sfcb_compress_sector(&sfcb, &sector);
	zassert_true(rc == 0, "Failed to get compress sector");
	rc = sfcb_first_loc(&sfcb, &loc);
	zassert_true(rc == 0, "Failed to get first loc");
	rc = sector - loc.sector;
	zassert_true(rc == 0, "Wrong compress sector");

	rc = sfcb_unmount(&sfcb);
	zassert_true(rc == 0, "Unmount failed [%d]", rc);

	/* Since we have only one sector written, the compress sector will be
	 * different from the first sector.
	 */
	LOG_INF("Multi Sector Configuration");
	sfcb.cfg = &cfg;
	rc = sfcb_mount(&sfcb);
	zassert_true(rc == 0, "Mount failed [%d]", rc);

	rc = sfcb_compress_sector(&sfcb, &sector);
	zassert_true(rc == 0, "Failed to get compress sector");
	rc = sfcb_first_loc(&sfcb, &loc);
	zassert_true(rc == 0, "Failed to get first loc");
	rc = sector - loc.sector;
	zassert_false(rc == 0, "Wrong compress sector");

	/* Write it full */
	while (1) {
		rc = sfcb_open_loc(&sfcb, &loc, 0, data_size);
		zassert_true(rc == 0, "open loc failed [%d]", rc);
		rc = sfcb_close_loc(&loc);
		zassert_true(rc == 0, "close loc failed [%d]", rc);
		if (loc.sector == 0U) {
			break;
		}
	}

	rc = sfcb_compress_sector(&sfcb, &sector);
	zassert_true(rc == 0, "Failed to get compress sector");
	rc = sfcb_first_loc(&sfcb, &loc);
	zassert_true(rc == 0, "Failed to get first loc");
	rc = sector - loc.sector;
	zassert_true(rc == 0, "Wrong compress sector");

	rc = sfcb_unmount(&sfcb);
	zassert_true(rc == 0, "Unmount failed [%d]", rc);
}

void test_sfcb_loc_walk(void)
{
	int rc;
	sfcb_loc loc;

	sfcb.cfg = &cfg;
	rc = sfcb_mount(&sfcb);
	zassert_true(rc == 0, "Mount failed [%d]", rc);

	/* walking forward */
	rc = sfcb_first_loc(&sfcb, &loc);
	zassert_true(rc == 0, "first loc failed");

	while (1) {
		if (sfcb_next_loc(&loc)) {
			break;
		}
	}

	rc = (loc.sector - sfcb.wr_sector) ||
	     (loc.ate_offset - sfcb.wr_ate_offset);
	zassert_true(rc == 0, "Wrong sector or offset");

	rc = sfcb_unmount(&sfcb);
	zassert_true(rc == 0, "Unmount failed [%d]", rc);

}

void test_sfcb_readwritelowlevel(void)
{
	int rc, id = 0, i;
	sfcb_loc loc;
	sfcb_ate *ate;
	char data[6]="test=0", buf[10];

	sfcb.cfg = &cfg;
	rc = sfcb_format(&sfcb);
	zassert_true(rc == 0, "Format failed [%d]", rc);
	rc = sfcb_mount(&sfcb);
	zassert_true(rc == 0, "Mount failed [%d]", rc);

	/* write in one step */
	while (sfcb.wr_sector == 0) {
		rc = sfcb_open_loc(&sfcb, &loc, id, sizeof(data));
		zassert_true(rc == 0, "open loc failed [%d]", rc);
		rc = sfcb_write_loc(&loc, data, sizeof(data));
		zassert_true(rc == sizeof(data), "write loc failed [%d]", rc);
		rc = sfcb_close_loc(&loc);
		zassert_true(rc == 0, "close loc failed [%d]", rc);
		id++;
	}

	/* write in chunks */
	while (sfcb.wr_sector == 1) {
		rc = sfcb_open_loc(&sfcb, &loc, id, sizeof(data));
		zassert_true(rc == 0, "open loc failed [%d]", rc);
		for (i = 0; i < sizeof(data); i++) {
			rc = sfcb_write_loc(&loc, &data[i], 1);
			zassert_true(rc == 1, "write loc failed [%d]", rc);
		}
		rc = sfcb_close_loc(&loc);
		zassert_true(rc == 0, "close loc failed [%d]", rc);
		id++;
	}

	/* read entry with id = 0 in one step */
	rc = sfcb_first_loc(&sfcb, &loc);
	zassert_true(rc == 0, "first loc failed [%d]", rc);
	while (1) {
		ate = sfcb_get_ate(&loc);
		if (ate->id == 0) {
			rc = ate->len - sizeof(data);
			zassert_true(rc == 0, "wrong found data size");
			rc = sfcb_read_loc(&loc, buf, ate->len);
			zassert_true(rc == sizeof(data), "wrong rd data size");
			rc = memcmp(data, buf, sizeof(data));
			zassert_true(rc == 0, "wrong rd data");
		}
		if (sfcb_next_loc(&loc)) {
			break;
		}
	}

	/* read entry with id = 0 in chunks */
	rc = sfcb_first_loc(&sfcb, &loc);
	zassert_true(rc == 0, "first loc failed [%d]", rc);
	while (1) {
		ate = sfcb_get_ate(&loc);
		if (ate->id == 0) {
			rc = ate->len - sizeof(data);
			zassert_true(rc == 0, "wrong found data size");
			for (i = 0; i < ate->len; i++) {
				rc = sfcb_read_loc(&loc, &buf[i], 1);
				zassert_true(rc == 1, "wrong rd data size");
			}
			rc = memcmp(data, buf, sizeof(data));
			zassert_true(rc == 0, "wrong rd data");
		}
		if (sfcb_next_loc(&loc)) {
			break;
		}
	}

	/* read entry with id = 10 in one step */
	rc = sfcb_first_loc(&sfcb, &loc);
	zassert_true(rc == 0, "first loc failed [%d]", rc);
	while (1) {
		ate = sfcb_get_ate(&loc);
		if (ate->id == 10) {
			rc = ate->len - sizeof(data);
			zassert_true(rc == 0, "wrong found data size");
			rc = sfcb_read_loc(&loc, buf, ate->len);
			zassert_true(rc == sizeof(data), "wrong rd data size");
			rc = memcmp(data, buf, sizeof(data));
			zassert_true(rc == 0, "wrong rd data");
		}
		if (sfcb_next_loc(&loc)) {
			break;
		}
	}

	/* read entry with id = 10 in chunks */
	rc = sfcb_first_loc(&sfcb, &loc);
	zassert_true(rc == 0, "first loc failed [%d]", rc);
	while (1) {
		ate = sfcb_get_ate(&loc);
		if (ate->id == 10) {
			rc = ate->len - sizeof(data);
			zassert_true(rc == 0, "wrong found data size");
			for (i = 0; i < ate->len; i++) {
				rc = sfcb_read_loc(&loc, &buf[i], 1);
				zassert_true(rc == 1, "wrong rd data size");
			}
			rc = memcmp(data, buf, sizeof(data));
			zassert_true(rc == 0, "wrong rd data");
		}
		if (sfcb_next_loc(&loc)) {
			break;
		}
	}

	/* read entry with id = 10 in chunks */
	rc = sfcb_first_loc(&sfcb, &loc);
	zassert_true(rc == 0, "first loc failed [%d]", rc);
	while (1) {
		ate = sfcb_get_ate(&loc);
		if (ate->id == 10) {
			rc = ate->len - sizeof(data);
			zassert_true(rc == 0, "wrong found data size");
			for (i = 0; i < ate->len; i++) {
				rc = sfcb_read_loc(&loc, &buf[0], 1);
				if (buf[0] == '=') {
					break;
				}
			}
			rc = sfcb_rewind_loc(&loc);
			zassert_true(rc == 0, "rewind failed");
			rc = sfcb_read_loc(&loc, &buf, i);
			buf[i]='\0';
			rc = memcmp(data, buf, i - 1);
			zassert_true(rc == 0, "wrong rd data");
			i++; /* jump over = */
			rc = sfcb_setpos_loc(&loc, i);
			rc = sfcb_read_loc(&loc, &buf, ate->len - i);
			zassert_true(rc == 1, "wrong rd size");
			zassert_true(buf[0] == '0', "wrong rd data");
		}
		if (sfcb_next_loc(&loc)) {
			break;
		}
	}

	rc = sfcb_unmount(&sfcb);
	zassert_true(rc == 0, "Unmount failed [%d]", rc);
}

void test_sfcb_readwritehighlevel(void)
{
	int rc, id = 0;
	char data[6]="test=0", buf[10];

	sfcb.cfg = &cfg;
	rc = sfcb_format(&sfcb);
	zassert_true(rc == 0, "Format failed [%d]", rc);
	rc = sfcb_mount(&sfcb);
	zassert_true(rc == 0, "Mount failed [%d]", rc);
	rc = sfcb_write(&sfcb, id, data, sizeof(data));
	zassert_true(rc == sizeof(data), "Write failed [%d]", rc);
	rc = sfcb_read(&sfcb, id, buf, sizeof(buf));
	zassert_true(rc == sizeof(data), "Read wrong size [%d]", rc);
	rc = sfcb_unmount(&sfcb);
	zassert_true(rc == 0, "Unmount failed [%d]", rc);
}

int compress(sfcb_fs *fs)
{
	int rc;
	sfcb_loc loc_compress, loc_walk;
	sfcb_ate *ate_compress, *ate_walk;
	bool copy;
	u16_t compress_sector;

	if (sfcb_first_loc(fs, &loc_compress)) {
		return 0;
	}
	rc = sfcb_compress_sector(fs, &compress_sector);
	zassert_true(rc == 0, "Compress sector failed [%d]", rc);

	while (loc_compress.sector == compress_sector) {
		copy = true;
		ate_compress = sfcb_get_ate(&loc_compress);
		loc_walk = loc_compress;
		while (!sfcb_next_loc(&loc_walk)) {
			ate_walk = sfcb_get_ate(&loc_walk);
			if (ate_compress->id == ate_walk->id) {
				copy = false;
				break;
			}
		}

		if ((copy) && (ate_compress->id == 0)) {
			rc = sfcb_copy_loc(&loc_compress);
			if (rc) {
				LOG_INF("Copy failed [%d]", rc);
				return rc;
			}
		}

		if (sfcb_next_loc(&loc_compress)) {
			break;
		}
	}
	return 0;
}

void test_sfcb_compress(void)
{

	int rc;
	sfcb_loc loc;
	sfcb_ate *ate;
	u16_t data_id, sector, scnt;
	char data[6]="123456";
	char buf[10];
	bool found;


	sfcb.cfg = &cfg;
	rc = sfcb_format(&sfcb);
	zassert_true(rc == 0, "Format failed [%d]", rc);

	sfcb.compress = &compress;
	rc = sfcb_mount(&sfcb);
	zassert_true(rc == 0, "Mount failed [%d]", rc);

	sector = 0U;
	scnt = 0U;
	data_id = 0;

	/* writing data until we trigger compress routine */
	while (scnt < (cfg.sector_cnt - 1)) {
		rc = sfcb_open_loc(&sfcb, &loc, data_id, sizeof(data));
		zassert_true(rc == 0, "open loc failed [%d]", rc);
		rc = sfcb_write_loc(&loc, data, sizeof(data));
		zassert_true(rc == sizeof(data), "write failed [%d]", rc);
		rc = sfcb_close_loc(&loc);
		data_id++;
		zassert_true(rc == 0, "close loc failed [%d]", rc);
		if (sfcb.wr_sector != sector) {
			sector = sfcb.wr_sector;
			scnt++;
			/* reset data_id, so id = 0,1,2 are only in sector 0 */
			data_id = 3;
		}
	}

	LOG_INF("Continuing...");
	/* data with id = 0 is copied by compress routine */
	/* write until a new sector is taken */
	while (sfcb.wr_sector == sector) {
		rc = sfcb_open_loc(&sfcb, &loc, data_id, sizeof(data));
		zassert_true(rc == 0, "open loc failed [%d]", rc);
		rc = sfcb_write_loc(&loc, data, sizeof(data));
		zassert_true(rc == sizeof(data), "write failed [%d]", rc);
		rc = sfcb_close_loc(&loc);
		zassert_true(rc == 0, "close loc failed [%d]", rc);
		data_id++;
	}

	/* now all data in the old sector has been removed, try reading entries
	 * with id = 1, this should fail, while reading entry with id = 0 should
	 * succeed.
	 */
	found = false;
	rc = sfcb_first_loc(&sfcb, &loc);
	zassert_true(rc == 0, "first loc failed [%d]", rc);

	do {
		ate = sfcb_get_ate(&loc);
		if (ate->id == 1) {
			zassert_true(0, "Found entry [id %u, len %u]", ate->id,
				ate->len);
		}
		if (ate->id == 0) {
			LOG_INF("Found entry with id 0");
			found = true;
			zassert_true(ate->len == sizeof(data),
				"Found entry with wrong size [id %u, len %u]",
				ate->id, ate->len);
			rc = sfcb_read_loc(&loc, &buf, ate->len);
			zassert_true(rc == sizeof(data), "read failed");
			rc = memcmp(data, buf, sizeof(data));
			zassert_true(rc == 0, "wrong data");
		}
	} while (!sfcb_next_loc(&loc));

	zassert_true(found, "Entry with id 0 not found");


}
void test_main(void)
{
	ztest_test_suite(test_sfcb,
			 ztest_unit_test(test_sfcb_mount),
			 ztest_unit_test(test_sfcb_loc),
			 ztest_unit_test(test_sfcb_loc_first),
			 ztest_unit_test(test_sfcb_loc_walk),
			 ztest_unit_test(test_sfcb_readwritelowlevel),
			 ztest_unit_test(test_sfcb_readwritehighlevel),
			 ztest_unit_test(test_sfcb_compress)
			);

	ztest_run_test_suite(test_sfcb);
}
