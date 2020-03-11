/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef H_ZB_EIGHT_CONFIRM_
#define H_ZB_EIGHT_CONFIRM_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>

/**
 * @brief confirm API
 * @{
 */

/**
 * @brief zb_slt_validate
 *
 * Validates the run image in a slot by checking the image crc32 vs the crc32
 * written in the verify slot
 *
 * @param sm_idx slot number
 * @retval 0 valid image
 * @retval -ERRNO errno code if error
 *
 */
int zb_slt_validate(u8_t sm_idx);

/**
 * @brief zb_slt_confirm
 *
 * Confirms the run image in a slot by writing the crc32 to the verify slot
 *
 * @param sm_idx slot number
 * @retval 0 Success
 * @retval -ERRNO errno code if error
 *
 */
int zb_slt_confirm(u8_t sm_idx);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
