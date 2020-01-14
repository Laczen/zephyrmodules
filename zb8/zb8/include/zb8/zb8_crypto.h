/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef H_ZB_EIGHT_CRYPTO_
#define H_ZB_EIGHT_CRYPTO_

#include <zephyr.h>
#include <tinycrypt/ecc.h>
#include <tinycrypt/aes.h>
#include <zb8/zb8_slot.h>

#define AES_BLOCK_SIZE		TC_AES_BLOCK_SIZE
#define AES_KEY_SIZE		TC_AES_KEY_SIZE
#define SIGNATURE_BYTES		2 * NUM_ECC_BYTES
#define PUBLIC_KEY_BYTES	2 * NUM_ECC_BYTES
#define PRIVATE_KEY_BYTES	NUM_ECC_BYTES
#define SHARED_SECRET_BYTES	NUM_ECC_BYTES
#define VERIFY_BYTES		NUM_ECC_BYTES
#define HASH_BYTES		NUM_ECC_BYTES

#ifdef __cplusplus
extern "C" {
#endif

/** @brief crypto API
 * @{
 */

/**
 * @brief zb_aes_ctr_mode
 *
 * perform aes ctr calculation
 *
 * @param buf pointer to buffer to encrypt / encrypted buffer
 * @param len bytes to encrypt
 * @param ctr counter (as byte array)
 * @param key encryption key
 * @retval 0 Success
 * @retval -ERRNO errno code if error
 */
int zb_aes_ctr_mode(u8_t *buf, size_t len, u8_t *ctr, const u8_t *key);

/**
 * @brief zb_get_encr_key_nonce
 *
 * Get the key and nonce used for encryption using a ec_dh 256 key exchange.
 * The key and nonce are derived from the shared secret using a key derivation
 * function (i.e. KDF1).
 * This routine uses the bootloader private key.
 *
 * @param key: returned encryption key
 * @param nonce: returned nonce
 * @param pubkey: public key used to generate the shared secret
 * @param keysize: expected size of returned key and nonce (i.e AES block size)
 * @retval -ERRNO errno code if error
 * @retval 0 if succesfull
 */
int zb_get_encr_key(u8_t *key, u8_t *nonce, const u8_t *pubkey, u16_t keysize);

/**
 * @brief zb_sign_verify
 *
 * Verifies the signature given the hash for a ec_dsa 256 signing.
 * This routine uses the public root keys that are stored in the bootloader.
 *
 * @param hash: calculated message hash
 * @param signature: message hash signature
 * @retval -ERRNO errno code if error
 * @retval 0 if succesfull
 */
int zb_sign_verify(const u8_t *hash, const u8_t *signature);

/**
 * @brief zb_hash
 *
 * Calculates the hash (SHA256) over a region.
 *
 * @param hash: calculated message hash
 * @param info: region info in zb_slot_info format
 * @param off: offset of region
 * @param len: region length
 * @retval -ERRNO errno code if error
 * @retval 0 if succesfull
 */
int zb_hash(u8_t *hash, struct zb_slt_info *info, u32_t off, size_t len);

/**
 * @}
 */


#ifdef __cplusplus
}
#endif

#endif
