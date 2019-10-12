/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/types.h>
#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

struct flash_img_ctx {
	u8_t buf[CONFIG_ZEPBOOT_DFU_BLOCK_BUF_SIZE];
	u16_t buf_offset;
	off_t wr_offset;
	struct device *fl_dev;
};

/** @brief Initialize flash_img_context.
 *
 *  return 0 on success, negative error code on fail
 */
int init_flash_img_ctx(struct flash_img_ctx *ctx);

/** @brief Flush flash_img_context.buf to flash.
 *
 *  return 0 on success, negative error code on fail
 */
int flush_flash_img_ctx(struct flash_img_ctx *ctx);



/** @brief Initialize zepboot and NRF DFU service.
 *
 *  Send an indication of attribute value change.
 *  Note: This function should only be called if CCC is declared with
 *  BT_GATT_CCC otherwise it cannot find a valid peer configuration.
 *
 *  Note: This procedure is asynchronous therefore the parameters need to
 *  remains valid while it is active.
 *
 */

void zepboot_dfu_init(void);

#ifdef __cplusplus
}
#endif