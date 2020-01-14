/*
 * Copyright (c) 2019 Laczen
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>

#include <zb8/zb8_flash.h>
#include <zb8/zb8_image.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(test_zb_image);

extern const unsigned char test_image_pln[1536];
extern const unsigned char test_image1_enc[1536];

#define HDR_SIZE 512
#define IMG_SIZE 1024

/**
 * @brief Test the opening of a image in slot RUN
 */
void test_zb_open_image(void)
{
	int err;
	struct zb_slt_info slt_info;
	struct zb_img_info info;

	err = zb_slt_open(&slt_info, 0, RUN);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);

	err = zb_erase(&slt_info, 0, slt_info.size);
	zassert_true(err == 0,  "Unable to erase image 0 area: [err %d]", err);

	err = zb_write(&slt_info, 0, test_image_pln, sizeof(test_image_pln));
	zassert_true(err == 0,  "Unable to write the image: [err %d]", err);

	/* without any verification */
	err = zb_get_img_info(&info, &slt_info);
	zassert_true(err == 0,  "Error loading image info: [err %d]", err);
	err = info.start - HDR_SIZE;
	zassert_true(err == 0,  "Wrong img start");
	err = info.end - IMG_SIZE - HDR_SIZE;
	zassert_true(err == 0,  "Wrong img size");
	err = info.end - info.enc_start;
	zassert_true(err == 0,  "Wrong enc start");

	/* with all verification */
	err = zb_val_img_info(&info, &slt_info, &slt_info);
	zassert_true(err == 0,  "Error loading image info: [err %d]", err);
	err = info.start - HDR_SIZE;
	zassert_true(err == 0,  "Wrong img start");
	err = info.end - IMG_SIZE - HDR_SIZE;
	zassert_true(err == 0,  "Wrong img size");
	err = info.end - info.enc_start;
	zassert_true(err == 0,  "Wrong enc start");
}

void test_zb_open_image_enc(void)
{
	int err;
	struct zb_slt_info slt_info;
	struct zb_img_info info;

	err = zb_slt_open(&slt_info, 0, RUN);
	zassert_true(err == 0,  "Unable to get slotarea info: [err %d]", err);

	err = zb_erase(&slt_info, 0, slt_info.size);
	zassert_true(err == 0,  "Unable to erase image 0 area: [err %d]", err);

	err = zb_write(&slt_info, 0, test_image1_enc, sizeof(test_image1_enc));
	zassert_true(err == 0,  "Unable to write the image: [err %d]", err);

	/* without any verification */
	err = zb_get_img_info(&info, &slt_info);
	zassert_true(err == 0,  "Error loading image info: [err %d]", err);
	err = info.start - HDR_SIZE;
	zassert_true(err == 0,  "Wrong img start");
	err = info.end - IMG_SIZE - HDR_SIZE;
	zassert_true(err == 0,  "Wrong img size");
	err = info.start - info.enc_start;
	zassert_true(err == 0,  "Wrong enc start");

	/* with all verification */
	err = zb_val_img_info(&info, &slt_info, &slt_info);
	zassert_true(err == 0,  "Error loading image info: [err %d]", err);
	err = info.start - HDR_SIZE;
	zassert_true(err == 0,  "Wrong img start");
	err = info.end - IMG_SIZE - HDR_SIZE;
	zassert_true(err == 0,  "Wrong img size");
	err = info.start - info.enc_start;
	zassert_true(err == 0,  "Wrong enc start");
}

void test_zb_image(void)
{
	ztest_test_suite(test_zb_image,
			 ztest_unit_test(test_zb_open_image),
			 ztest_unit_test(test_zb_open_image_enc)
			);

	ztest_run_test_suite(test_zb_image);
}
