/*
 * Flash routines for zb8
 * a. general purpose flash read/write/erase routines
 * b. routines to use image command (cmd) info
 *
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 #ifndef H_ZB_EIGHT_FLASH_
 #define H_ZB_EIGHT_FLASH_

#include <zephyr.h>
#include <zb8/zb8_slot.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Support alignment write size of up to 32 bytes */
#define ALIGN_BUF_SIZE 32

#define EMPTY_U8 0xff
#define EMPTY_U32 0xffffffff

/**
 * @brief zb_flash API: general flash routines
 */

u32_t zb_align_up(struct zb_slt_info *info, size_t len);
u32_t zb_align_down(struct zb_slt_info *info, size_t len);
int zb_erase(struct zb_slt_info *info, u32_t offset, size_t len);
int zb_write(struct zb_slt_info *info, u32_t offset, const void *data,
             size_t len);
int zb_read(struct zb_slt_info *info, u32_t offset, void *data, size_t len);
bool zb_empty_slot(struct zb_slt_info *info);

/**
 * @}
 */

/**
 * @brief zb_cmd: command stored in flash
 * @{
 */

struct zb_cmd {
	/*@{*/
	u8_t cmd1;	/**< command */
        u8_t cmd2;      /**< extended command */
	u8_t cmd3;	/**< sector to process */
	u8_t crc8;	/**< crc8 calculated over cmd and sector */
	/*@}*/
} __packed;

/**
 * @}
 */

/**
 * @brief zb_cmd API
 * @{
 */

/**
 * @brief zb_cmd_read
 *
 * reads last valid cmd from a slot area
 *
 * @param info Pointer to zb_slt_info
 * @param cmd Pointer to command
 * @retval 0 Success
 * @retval -ERRNO errno code if error
 */
int zb_cmd_read(struct zb_slt_info *info, struct zb_cmd *cmd);

/**
 * @brief zb_cmd_write
 *
 * writes new cmd to a slot area
 *
 * @param fs Pointer to zb_slt_info
 * @param cmd Pointer to command
 * @retval 0 Success
 * @retval -ERRNO errno code if error
 */
int zb_cmd_write(struct zb_slt_info *info, struct zb_cmd *cmd);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
