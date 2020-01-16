/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zb8/zb8_crypto.h>
#include <zb8/zb8_flash.h>

#include <tinycrypt/ecc_dh.h>
#include <tinycrypt/ecc_dsa.h>
#include <tinycrypt/sha256.h>
#include <tinycrypt/aes.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(zb8_crypto);

extern const u8_t ec256_boot_pri_key[];
extern const u16_t ec256_boot_pri_key_len;
extern const u8_t ec256_root_pub_key[];
extern const u16_t ec256_root_pub_key_len;

int zb_get_encr_key(u8_t *key, u8_t *nonce, const u8_t *pubkey, u16_t keysize)
{
	u8_t ext[4] = {0};
	u8_t secret[SHARED_SECRET_BYTES] = {0};
	u8_t digest[HASH_BYTES] = {0};

	const struct uECC_Curve_t * curve = uECC_secp256r1();
	struct tc_sha256_state_struct s;

	if (keysize > PRIVATE_KEY_BYTES) {
		return -EFAULT;
	}
	if (uECC_valid_public_key(pubkey, curve) != 0) {
		return -EFAULT;
	};

	if (!uECC_shared_secret(pubkey, ec256_boot_pri_key, secret, curve)) {
		return -EFAULT;
	}

	if (!tc_sha256_init(&s)) {
		return -EFAULT;
	}

	if (!tc_sha256_update(&s, secret, SHARED_SECRET_BYTES)) {
		return -EFAULT;
	}

	if (!tc_sha256_update(&s, ext, 4)) {
		return -EFAULT;
	}

	if (!tc_sha256_final(digest, &s)) {
		return -EFAULT;
	}
	memcpy(key, digest, keysize);
	memcpy(nonce, digest+keysize, keysize);

	memset(digest, 0, HASH_BYTES);
	memset(secret, 0, SHARED_SECRET_BYTES);

	return 0;
}

int zb_sign_verify(const u8_t *hash, const u8_t *signature)
{
	int cnt;
	u8_t pubk[PUBLIC_KEY_BYTES];
	const struct uECC_Curve_t * curve = uECC_secp256r1();

	/* validate the hash for each of the root pubkeys */
	cnt = 0;
	while (cnt < ec256_root_pub_key_len) {
		memcpy(pubk, &ec256_root_pub_key[cnt], PUBLIC_KEY_BYTES);
		cnt += PUBLIC_KEY_BYTES;
		if (uECC_valid_public_key(pubk, curve) != 0) {
			continue;
		}
		if (uECC_verify(pubk, hash, VERIFY_BYTES, signature, curve)) {
			return 0;
		}
	}
	return -EFAULT;
}

int zb_hash(u8_t *hash, struct zb_slt_info *info, u32_t off, size_t len)
{
	int rc;
	struct tc_sha256_state_struct s;
	u8_t buf[HASH_BYTES];

	if (!tc_sha256_init(&s)) {
		return -EFAULT;
	}

	while (len) {
		size_t buf_len = MIN(HASH_BYTES, len);
		rc = zb_read(info, off, buf, buf_len);
		if (rc) {
			return rc;
		}

		if (!tc_sha256_update(&s, buf, buf_len)) {
			return -EFAULT;
		}
		off += buf_len;
		len -= buf_len;
	}

	if (!tc_sha256_final(hash, &s)) {
		return -EFAULT;
	}

	return 0;
}

int zb_aes_ctr_mode(u8_t *buf, size_t len, u8_t *ctr, const u8_t *key)
{
	struct tc_aes_key_sched_struct sched;
	u8_t buffer[AES_BLOCK_SIZE];
	u8_t nonce[AES_BLOCK_SIZE];
	u32_t i;
	u8_t blk_off, j, u8;


	(void)memcpy(nonce, ctr, sizeof(nonce));
	(void)tc_aes128_set_encrypt_key(&sched, key);
	for (i = 0; i < len; i++) {
		blk_off = i & (AES_BLOCK_SIZE - 1);
		if (blk_off == 0) {
			if (tc_aes_encrypt(buffer, nonce, &sched)) {
				for (j = AES_BLOCK_SIZE; j > 0; --j) {
                			if (++nonce[j - 1] != 0) {
                    				break;
                			}
            			}
			} else {
				return -EFAULT;
			}
		}
		/* update output */
		u8 = *buf;
		*buf++ = u8 ^ buffer[blk_off];
	}
	(void)memcpy(ctr, nonce, sizeof(nonce));

	return 0;
}