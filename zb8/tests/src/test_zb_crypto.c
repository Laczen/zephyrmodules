/*
 * Copyright (c) 2019 Laczen
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>

#include <zb8/zb8_flash.h>
#include <zb8/zb8_crypto.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(test_zb_crypto);

extern u8_t test_msg[];
extern u8_t test_msg_hash[];

/**
 * @brief Test hash calculation for data in flash
 */
void test_zb_hash(void)
{
	int err, cnt;
	struct zb_slt_info info;
	u8_t hash[HASH_BYTES];

	cnt = zb_slt_area_cnt();
	zassert_false(cnt == 0,  "Unable to get slotarea count: [cnt %d]", cnt);

	err = zb_slt_open(&info, cnt - 1, RUN);
	zassert_true(err == 0,  "Unable to get info: [err %d]", err);

	err = zb_erase(&info, 0, info.size);
	zassert_true(err == 0,  "Unable to erase image 0 area: [err %d]", err);

	err = zb_write(&info, 0, test_msg, HASH_BYTES);
	zassert_true(err == 0,  "Unable to write test message: [err %d]", err);


	err = zb_hash(hash, &info, 0, HASH_BYTES);
	zassert_true(err == 0, "Hash calculation failed: [err %d]", err);

	err = memcmp(hash, test_msg_hash, HASH_BYTES);
	zassert_true(err == 0, "Hash differs");
}

extern u8_t test_signature[];

/**
 * @brief Test signature verification
 */
void test_zb_sign_verify(void)
{
	int err;
	u8_t tmp;

	err = zb_sign_verify(test_msg_hash, test_signature);
	zassert_true(err == 0, "Signature validation failed: [err %d]", err);

	/* modify the key */
	tmp = test_msg_hash[0];
	test_msg_hash[0] >>=1;
	err = zb_sign_verify(test_msg_hash, test_signature);
	zassert_false(err == 0, "Invalid hash generates valid signature");

	/* reset the key */
	test_msg_hash[0] = tmp;

}

extern u8_t test_enc_pub_key[];
extern u8_t test_enc_key[];

/**
 * @brief Test the generation of an encryption key from public key
 */
void test_zb_get_encr_key(void)
{
	int err;
	u8_t enc_key[16], nonce[16], tmp;

	err = zb_get_encr_key(enc_key, nonce, test_enc_pub_key, 16);
	zassert_true(err == 0,  "Failed to get encryption key: [err %d]", err);
	err = memcmp(enc_key, test_enc_key, 16);
	zassert_true(err == 0,  "Encryption keys differ: [err %d]", err);

	/* modify the key */
	tmp = test_enc_pub_key[0];
	test_enc_pub_key[0] >>= 1;
	err = zb_get_encr_key(enc_key, nonce, test_enc_pub_key, 16);
	zassert_false(err == 0,  "Invalid pubkey generates encryption key");

	/* reset the key */
	test_enc_pub_key[0] = tmp;

}

extern u8_t test_msg[];
extern u8_t ec256_boot_pri_key[];
u8_t enc_test_msg[HASH_BYTES];
u8_t dec_test_msg[HASH_BYTES];

/**
 * @brief Test the aes enc routine
 */
void test_zb_aes_enc(void)
{
	int err;

	u8_t ctr[AES_BLOCK_SIZE]={0};

	memcpy(enc_test_msg, test_msg, HASH_BYTES);
	err = zb_aes_ctr_mode(enc_test_msg, HASH_BYTES, ctr,
			      ec256_boot_pri_key);
	zassert_true(err == 0,  "AES CTR returned [err %d]", err);
	err = ctr[15] - HASH_BYTES/AES_BLOCK_SIZE;
	zassert_true(err == 0,  "AES CTR wrong CTR value");
	err = memcmp(enc_test_msg, test_msg, HASH_BYTES);
	zassert_false(err == 0,  "AES wrong encrypt data");

}
void test_zb_aes_dec(void)
{
	int err;

	u8_t ctr[AES_BLOCK_SIZE]={0};

	memcpy(dec_test_msg, enc_test_msg, HASH_BYTES);
	err = zb_aes_ctr_mode(dec_test_msg, HASH_BYTES, ctr,
			      ec256_boot_pri_key);
	zassert_true(err == 0,  "AES CTR returned [err %d]", err);
	err = ctr[15] - HASH_BYTES/AES_BLOCK_SIZE;
	zassert_true(err == 0,  "AES CTR wrong CTR value");
	err = memcmp(dec_test_msg, test_msg, HASH_BYTES);
	zassert_true(err == 0,  "AES wrong decrypt data");

}

void test_zb_crypto(void)
{
	ztest_test_suite(test_zb_crypto,
			 ztest_unit_test(test_zb_aes_enc),
			 ztest_unit_test(test_zb_aes_dec),
			 ztest_unit_test(test_zb_hash),
			 ztest_unit_test(test_zb_sign_verify),
			 ztest_unit_test(test_zb_get_encr_key)
	);

	ztest_run_test_suite(test_zb_crypto);
}
