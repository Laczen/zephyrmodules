/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef H_ZB_EIGHT_DFU_
#define H_ZB_EIGHT_DFU_

#include <zephyr.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief zb_dfu_receive_start.
 *
 * receive start routine, called before receiving data
 * for future extensions, at the moment a NOP.
 *
 * @retval 0 on success, negative error code on fail
 */
int zb_dfu_receive_start(void);

/** @brief zb_dfu_receive.
 *
 * receive data from uploader
 *
 * @param offset data offset
 * @param data receive data
 * @param len size of received data
 * @retval 0 on success, negative error code on fail
 */
int zb_dfu_receive(u32_t offset, const u8_t *data, size_t len);

/** @brief zb_dfu_receive_flush
 *
 * receive flush, called at the end of upload to ensure all data is written to
 * flash
 *
 * @retval 0 on success, negative error code on fail
 */
void zb_dfu_receive_flush(void);

/** @brief zb_dfu_confirm.
 *
 * confirm image in run slot of slotarea sm_idx
 *
 * @retval 0 on success, negative error code on fail
 */
int zb_dfu_confirm(u8_t sm_idx);

/** @brief zb_dfu_restart.
 *
 * restart the system by starting the First Stage Loader
 *
 */
void zb_dfu_restart(void);

/** @brief zb_dfu_blstart.
 *
 * restart the system by starting the bootloader
 *
 */
void zb_dfu_blstart(void);

#ifdef __cplusplus
}
#endif

#endif