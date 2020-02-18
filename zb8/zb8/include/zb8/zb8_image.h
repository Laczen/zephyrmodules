/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef H_ZB_EIGHT_IMAGE_
#define H_ZB_EIGHT_IMAGE_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>
#include <zb8/zb8_fsl.h>
#include <zb8/zb8_crypto.h>

/** @brief image API structures
 * @{
 */

/**
 * @brief zb_img_dep: dependency specifier
 */

struct zb_img_dep {
    	u32_t offset;
    	__packed struct zb_fsl_ver ver_min;
   	__packed struct zb_fsl_ver ver_max;
};

/**
 * @brief img_info: contains all the info required during a move
 */
struct zb_img_info {
	u32_t start;
    	u32_t enc_start;
    	u32_t end;
    	u32_t load_address;
	u32_t version;
    	u32_t build;
    	u8_t enc_key[AES_KEY_SIZE];
    	u8_t enc_nonce[AES_KEY_SIZE];
    	bool hdr_ok;
    	bool img_ok;
    	bool dep_ok;
	bool key_ok;
	bool is_bootloader;
    	bool confirmed;
};

/**
 * @}
 */

#define TLVE_IMAGE_HASH 0x0100
#define TLVE_IMAGE_HASH_BYTES HASH_BYTES

#define TLVE_IMAGE_EPUBKEY 0x0200
#define TLVE_IMAGE_EPUBKEY_BYTES PUBLIC_KEY_BYTES

#define TLVE_IMAGE_DEPS 0x0300
#define TLVE_IMAGE_DEPS_BYTES sizeof(struct zb_img_dep)

/**
 * @brief image API
 * @{
 */

/**
 * @brief zb_val_img_info
 *
 * Get information for the image in slt_info, validate header, image and
 * dependencies.
 *
 * @param img_info pointer to store info in.
 * @param slt_info
 * @param dst_slt_info destination slot info
 * @retval 0 Success
 * @retval -ERRNO errno code if error
 */
int zb_val_img_info(struct zb_img_info *info, struct zb_slt_info *slt_info,
		    struct zb_slt_info *dst_slt_info);

/**
 * @brief zb_get_img_info
 *
 * Get information for the image in slt_info, no validation of header, image
 * or dependencies.
 *
 * @param img_info pointer to store info in.
 * @param slt_info
 * @retval 0 Success
 * @retval -ERRNO errno code if error
 */
int zb_get_img_info(struct zb_img_info *info, struct zb_slt_info *slt_info);

/**
 * @brief zb_slt_has_img_hdr
 *
 * Check if the slt in slt_info contains a image hdr.
 *
 * @param slt_info
 * @retval 0 Success
 * @retval -ERRNO errno code if error
 */
int zb_slt_has_img_hdr(struct zb_slt_info *slt_info);

/**
 * @brief zb_res_img_info
 *
 * Reset boolean values in img_info.
 *
 * @param img_info pointer.
 */
void zb_res_img_info(struct zb_img_info *info);

/**
 * @brief zb_img_info_valid
 *
 * checks if image info is valid
 *
 * @param img_info pointer
 * @retval true if image valid, false if not
 */
bool zb_img_info_valid(struct zb_img_info *info);

/**
 * @brief zb_img_confirm
 *
 * confirms the image by writing a crc32 just before the start of the image
 *
 * @param slt_info
 * @retval 0 Success
 * @retval -ERRNO errno code if error
 *
 */
int zb_img_confirm(struct zb_slt_info *slt_info);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
