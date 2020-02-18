/*
 * Copyright (c) 2019 Laczen
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>

#include <zb8/zb8_flash.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(test_zb_flash);

/**
 * @brief Test zb_slt_open()
 */
void test_zb_slt_open(void)
{
	int err, cnt;
	struct zb_slt_info slt_info;

	cnt = zb_slt_area_cnt();
	zassert_false(cnt == 0,  "Unable to get slotarea count: [cnt %d]", cnt);

	while (cnt > 0) {
		err = zb_slt_open(&slt_info, cnt - 1, RUN);
		zassert_true(err == 0, "Unable to open RUN: [err %d]", err);
		LOG_INF("RUN offset [%x], size [%d]", slt_info.offset,
			slt_info.size);

		err = zb_slt_open(&slt_info, cnt - 1, MOVE);
		zassert_true(err == 0, "Unable to open MOVE: [err %d]", err);
		LOG_INF("MOVE offset [%x], size [%d]", slt_info.offset,
			slt_info.size);

		err = zb_slt_open(&slt_info, cnt - 1, UPGRADE);
		zassert_true(err == 0, "Unable to open UPGRADE: [err %d]", err);
		LOG_INF("UPGRADE offset [%x], size [%d]", slt_info.offset,
			slt_info.size);

		err = zb_slt_open(&slt_info, cnt - 1, SWPSTAT);
		zassert_true(err == 0, "Unable to open SWPSTAT: [err %d]", err);
		LOG_INF("SWPSTAT offset [%x], size [%d]", slt_info.offset,
			slt_info.size);

		cnt--;
	}
}

/**
 * @brief Test zb_sectorsize_get()
 */
void test_zb_sectorsize_get(void)
{
	int cnt;
	uint32_t sectorsize;

	cnt = zb_slt_area_cnt();
	zassert_false(cnt == 0,  "Unable to get slotarea count: [cnt %d]", cnt);

	while (cnt > 0) {
		sectorsize = zb_sectorsize_get(cnt - 1);
		zassert_false(sectorsize == 0, "Sectorsize error");
		LOG_INF("Sectorsize [%d]", sectorsize);

		cnt--;
	}
}

/**
 * Helper routine for flash testing
 */
static void fltest(struct zb_slt_info *slt_info)
{
	int err;
	uint8_t buf[8] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
	uint8_t rdbuf[8];

	err = zb_erase(slt_info, 0, slt_info->size);
	zassert_true(err == 0, "Flash erase error [err %d]", err);
	err = zb_write(slt_info, 0, buf, sizeof(buf));
	zassert_true(err == 0, "Flash write error [err %d]", err);
	err = zb_read(slt_info, 0, rdbuf, sizeof(rdbuf));
	zassert_true(err == 0, "Flash read error [err %d]", err);
	err = memcmp(buf, rdbuf, sizeof(buf));
	zassert_true(err == 0, "Flash read error value differs");
	err = zb_erase(slt_info, 0, slt_info->size);
	zassert_true(err == 0, "Flash erase error [err %d]", err);
}

/**
 * @brief Test zb_erase/write/read
 */
void test_zb_ewr(void)
{
	int err, cnt;
	struct zb_slt_info slt_info;

	cnt = zb_slt_area_cnt();
	zassert_false(cnt == 0,  "Unable to get slotarea count: [cnt %d]", cnt);

	while (cnt > 0) {
		err = zb_slt_open(&slt_info, cnt - 1, RUN);
		zassert_true(err == 0, "Unable to open RUN: [err %d]", err);
		fltest(&slt_info);

		err = zb_slt_open(&slt_info, cnt - 1, MOVE);
		zassert_true(err == 0, "Unable to open MOVE: [err %d]", err);
		fltest(&slt_info);

		err = zb_slt_open(&slt_info, cnt - 1, UPGRADE);
		zassert_true(err == 0, "Unable to open UPGRADE: [err %d]", err);
		fltest(&slt_info);

		err = zb_slt_open(&slt_info, cnt - 1, SWPSTAT);
		zassert_true(err == 0, "Unable to open SWPSTAT: [err %d]", err);
		if (slt_info.size) {
			fltest(&slt_info);
		}

		cnt--;
	}
}

/**
 * @brief Test read and write of zb commands
 */
void test_zb_cmd(void)
{
	int err, cnt;
	struct zb_slt_info slt_info;
	struct zb_cmd cmd, cmd_rd;

	cnt = zb_slt_area_cnt();
	zassert_false(cnt == 0,  "Unable to get slotarea count: [cnt %d]", cnt);

	err = zb_slt_open(&slt_info, 0, SWPSTAT);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);

	err = zb_erase(&slt_info, 0, slt_info.size);
	zassert_true(err == 0,  "Unable to erase stat area: [err %d]", err);

	err = zb_cmd_read(&slt_info, &cmd_rd);
	zassert_true(err == -ENOENT, "Found cmd entry in empty flash area");

	cmd.cmd1 = 0x0;
	cmd.cmd2 = 0x0;
	cmd.cmd3 = 0x0;

	err = zb_cmd_write(&slt_info, &cmd);
	zassert_true(err == 0, "Failed to write command");

	err = zb_cmd_read(&slt_info, &cmd_rd);
	zassert_true(((cmd.cmd1 == cmd_rd.cmd1) && (cmd.cmd2 == cmd_rd.cmd2) &&
		     (cmd.cmd3 == cmd_rd.cmd3)),
		      "Found different cmd entry in flash area");

	/* write commands until stat area is full */
	while (err == 0) {
		cmd.cmd3++;
		err = zb_cmd_write(&slt_info, &cmd);
		if (cmd.cmd3 > 1024) {
			break;
		}
	}
	zassert_true(err == -ENOSPC, "To many cmd writes possible");
}

void test_zb_flash(void)
{
	ztest_test_suite(test_zb_flash,
			 ztest_unit_test(test_zb_slt_open),
			 ztest_unit_test(test_zb_sectorsize_get),
			 ztest_unit_test(test_zb_ewr),
			 ztest_unit_test(test_zb_cmd)
			);

	ztest_run_test_suite(test_zb_flash);

}
