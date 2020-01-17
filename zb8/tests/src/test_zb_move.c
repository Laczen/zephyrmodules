/*
 * Copyright (c) 2019 Laczen
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>

#include <zb8/zb8_move.h>
#include <zb8/zb8_flash.h>
#include <zb8/zb8_image.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(test_zb_move);

extern const unsigned char test_image_pln[1536];
extern const unsigned char test_image1_enc[1536];
extern const unsigned char test_image2_enc[1536];
extern const unsigned char test_imageboot_enc[1536];

#define HDR_SIZE 512
#define HDR_SKIP 32
#define IMG_SIZE 1024

void move_test(uint8_t sm_idx, const unsigned char *image)
{
	int err;
	struct zb_slt_info slt_info;
	u8_t imgheader[HDR_SIZE];
	u8_t img[IMG_SIZE];

	err = zb_slt_open(&slt_info, sm_idx, UPGRADE);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);

	err = zb_write(&slt_info, 0, image, HDR_SIZE + IMG_SIZE);
	zassert_true(err == 0,  "Unable to write the image: [err %d]", err);

	err = zb_slt_open(&slt_info, sm_idx, SWPSTAT);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);

	err = zb_erase(&slt_info, 0, slt_info.size);
	zassert_true(err == 0,  "Unable to erase SWPSTAT area: [err %d]", err);

	err = zb_img_swap(sm_idx);

	err = zb_slt_open(&slt_info, sm_idx, RUN);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);
	err = zb_read(&slt_info, 0, imgheader, HDR_SIZE - HDR_SKIP);
	zassert_true(err == 0, "Unable to read moved header");
	err = memcmp(&imgheader, image, HDR_SIZE - HDR_SKIP);
	zassert_true(err == 0, "Difference detected in moved header");

	err = zb_read(&slt_info, HDR_SIZE, img, IMG_SIZE);
	zassert_true(err == 0, "Unable to read moved image");
	err = memcmp(img, &test_image_pln[HDR_SIZE], IMG_SIZE);
	zassert_true(err == 0, "Difference detected in image");
}
/**
 * @brief Test the unencrypted move
 */
void test_zb_image_move_pln(void)
{
	int cnt, err;
	struct zb_slt_info slt_info;

	cnt = zb_slt_area_cnt();
	zassert_false(cnt == 0,  "Unable to get slotarea count: [cnt %d]", cnt);

	while ((cnt--) > 0) {
		LOG_INF("Testing Region %d", cnt);
		err = zb_slt_open(&slt_info, cnt, RUN);
		zassert_true(err == 0,  "Unable to get slotarea info: [err %d]",
			     err);
		err = zb_erase(&slt_info, 0U, slt_info.size);
		zassert_true(err == 0,  "Failed to erase RUN area");
		err = zb_slt_open(&slt_info, cnt, UPGRADE);
		zassert_true(err == 0,  "Unable to get slotarea info: [err %d]",
			     err);
		err = zb_erase(&slt_info, 0U, slt_info.size);
		zassert_true(err == 0,  "Failed to erase UPGRADE area");
		move_test(cnt, test_image_pln);
	}
}

/**
 * @brief Test the encrypted move
 */
void test_zb_image_move_enc(void)
{
	int cnt, err;
	struct zb_slt_info slt_info;

	cnt = zb_slt_area_cnt();
	zassert_false(cnt == 0,  "Unable to get slotarea count: [cnt %d]", cnt);

	while ((cnt--) > 0) {
		LOG_INF("Testing Region %d", cnt);
		err = zb_slt_open(&slt_info, cnt, RUN);
		zassert_true(err == 0,  "Unable to get slotarea info: [err %d]",
			     err);
		err = zb_erase(&slt_info, 0U, slt_info.size);
		zassert_true(err == 0,  "Failed to erase RUN area");
		err = zb_slt_open(&slt_info, cnt, UPGRADE);
		zassert_true(err == 0,  "Unable to get slotarea info: [err %d]",
			     err);
		err = zb_erase(&slt_info, 0U, slt_info.size);
		zassert_true(err == 0,  "Failed to erase UPGRADE area");
		move_test(cnt, test_image1_enc);
	}
}

/**
 * @brief Test a pattern of uploads
 */
void test_zb_image_pattern_0(void)
{
	int cnt, err;
	struct zb_slt_info slt_info;
	u8_t imgheader[HDR_SIZE];

	cnt = 0;
	LOG_INF("Testing Region %d", cnt);
	err = zb_slt_open(&slt_info, cnt, RUN);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);
	err = zb_erase(&slt_info, 0U, slt_info.size);
	zassert_true(err == 0,  "Failed to erase RUN area");

	/* Put test_image1_enc */
	LOG_INF("Starting placement of test_image1_enc");
	err = zb_slt_open(&slt_info, cnt, UPGRADE);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);
	err = zb_erase(&slt_info, 0U, slt_info.size);
	zassert_true(err == 0,  "Failed to erase UPGRADE area");
	err = zb_write(&slt_info, 0, test_image1_enc, HDR_SIZE + IMG_SIZE);
	zassert_true(err == 0,  "Unable to write image: [err %d]", err);

	err = zb_slt_open(&slt_info, cnt, SWPSTAT);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);
	err = zb_erase(&slt_info, 0, slt_info.size);
	zassert_true(err == 0,  "Unable to erase SWPSTAT area: [err %d]", err);

	err = zb_img_swap(cnt);
	zassert_true(err == 0,  "SWAP returned error: [err %d]", err);

	err = zb_slt_open(&slt_info, cnt, RUN);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);
	err = zb_read(&slt_info, 0, imgheader, HDR_SIZE);
	zassert_true(err == 0, "Unable to read moved header");
	err = memcmp(&imgheader[HDR_SKIP], &test_image1_enc[HDR_SKIP],
		     HDR_SIZE-HDR_SKIP);
	zassert_true(err == 0, "Difference detected in moved header");

	/* Put test_image2_enc */
	LOG_INF("Starting upgrade to test_image2_enc");
	err = zb_slt_open(&slt_info, cnt, UPGRADE);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);
	err = zb_erase(&slt_info, 0U, slt_info.size);
	zassert_true(err == 0,  "Failed to erase UPGRADE area");
	err = zb_write(&slt_info, 0, test_image2_enc, HDR_SIZE + IMG_SIZE);
	zassert_true(err == 0,  "Unable to write image: [err %d]", err);

	err = zb_slt_open(&slt_info, cnt, SWPSTAT);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);
	err = zb_erase(&slt_info, 0, slt_info.size);
	zassert_true(err == 0,  "Unable to erase SWPSTAT area: [err %d]", err);

	err = zb_img_swap(cnt);
	zassert_true(err == 0,  "SWAP returned error: [err %d]", err);

	err = zb_slt_open(&slt_info, cnt, RUN);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);
	err = zb_read(&slt_info, 0, imgheader, HDR_SIZE);
	zassert_true(err == 0, "Unable to read moved header");
	err = memcmp(&imgheader[HDR_SKIP], &test_image2_enc[HDR_SKIP],
		     HDR_SIZE-HDR_SKIP);
	zassert_true(err == 0, "Difference detected in moved header");

	/* call swap again, this should restore test_image1_enc */
	LOG_INF("Starting restore of test_image1_enc");
	err = zb_img_swap(cnt);
	zassert_true(err == 0,  "SWAP returned error: [err %d]", err);

	err = zb_slt_open(&slt_info, cnt, RUN);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);
	err = zb_read(&slt_info, 0, imgheader, HDR_SIZE);
	zassert_true(err == 0, "Unable to read moved header");
	err = memcmp(&imgheader[HDR_SKIP], &test_image1_enc[HDR_SKIP],
		     HDR_SIZE-HDR_SKIP);
	zassert_true(err == 0, "Difference detected in moved header");

	/* Put test_imageboot_enc a bootloader - do not erase the swpstat */
	LOG_INF("Starting upgrade of bootloader");
	err = zb_slt_open(&slt_info, cnt, UPGRADE);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);
	err = zb_erase(&slt_info, 0U, slt_info.size);
	zassert_true(err == 0,  "Failed to erase UPGRADE area");
	err = zb_write(&slt_info, 0, test_imageboot_enc, HDR_SIZE + IMG_SIZE);
	zassert_true(err == 0,  "Unable to write image: [err %d]", err);

	err = zb_img_swap(cnt);
	zassert_true(err == 0,  "SWAP returned error: [err %d]", err);

	err = zb_slt_open(&slt_info, cnt, RUN);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);
	err = zb_read(&slt_info, 0, imgheader, HDR_SIZE);
	zassert_true(err == 0, "Unable to read moved header");
	err = memcmp(&imgheader[HDR_SKIP], &test_imageboot_enc[HDR_SKIP],
		     HDR_SIZE-HDR_SKIP);
	zassert_true(err == 0, "Difference detected in moved header");

	/* Call swap again, should restore the previous image */
	LOG_INF("Restore test_image1_enc after bootloader");
	err = zb_img_swap(cnt);
	zassert_true(err == 0,  "SWAP returned error: [err %d]", err);

	err = zb_slt_open(&slt_info, cnt, RUN);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);
	err = zb_read(&slt_info, 0, imgheader, HDR_SIZE);
	zassert_true(err == 0, "Unable to read moved header");
	err = memcmp(&imgheader[HDR_SKIP], &test_image1_enc[HDR_SKIP],
		     HDR_SIZE-HDR_SKIP);
	zassert_true(err == 0, "Difference detected in moved header");

	/* Upgrade to test_image2_enc */
	LOG_INF("Upgrade to test_image2_enc");
	err = zb_slt_open(&slt_info, cnt, UPGRADE);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);
	err = zb_erase(&slt_info, 0U, slt_info.size);
	zassert_true(err == 0,  "Failed to erase UPGRADE area");
	err = zb_write(&slt_info, 0, test_image2_enc, HDR_SIZE + IMG_SIZE);
	zassert_true(err == 0,  "Unable to write image: [err %d]", err);
	err = zb_slt_open(&slt_info, cnt, SWPSTAT);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);
	err = zb_erase(&slt_info, 0, slt_info.size);
	zassert_true(err == 0,  "Unable to erase SWPSTAT area: [err %d]", err);

	err = zb_img_swap(cnt);
	zassert_true(err == 0,  "SWAP returned error: [err %d]", err);

	err = zb_slt_open(&slt_info, cnt, RUN);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);
	err = zb_read(&slt_info, 0, imgheader, HDR_SIZE);
	zassert_true(err == 0, "Unable to read moved header");
	err = memcmp(&imgheader[HDR_SKIP], &test_image2_enc[HDR_SKIP],
		     HDR_SIZE-HDR_SKIP);
	zassert_true(err == 0, "Difference detected in moved header");

	/* Confirm the image */
	//LOG_INF("Confirming test_image2_enc");
	//err = zb_fsl_hdr_confirm(slt_info.fl_dev, slt_info.offset);

	/* Try to restore the previous image */
	LOG_INF("Try to restore previous image, should do nothing");
	err = zb_slt_open(&slt_info, cnt, SWPSTAT);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);
	err = zb_erase(&slt_info, 0, slt_info.size);
	zassert_true(err == 0,  "Unable to erase SWPSTAT area: [err %d]", err);

	err = zb_img_swap(cnt);
	zassert_true(err == 0,  "SWAP returned error: [err %d]", err);

	err = zb_slt_open(&slt_info, cnt, RUN);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);
	err = zb_read(&slt_info, 0, imgheader, HDR_SIZE);
	zassert_true(err == 0, "Unable to read moved header");
	// err = memcmp(&imgheader[HDR_SKIP], &test_image1_enc[HDR_SKIP],
	// 	     HDR_SIZE-HDR_SKIP);
	// zassert_false(err == 0, "Header is equal to test_image1_enc header");

}

void test_zb_move(void)
{
	ztest_test_suite(test_zb_move,
			 ztest_unit_test(test_zb_image_move_pln),
			 ztest_unit_test(test_zb_image_move_enc),
			 ztest_unit_test(test_zb_image_pattern_0)
			);

	ztest_run_test_suite(test_zb_move);
}
