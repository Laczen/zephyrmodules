/*
 * TLV (TYPE - LENGTH - VARIABLE) support for zepboot
 *
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef H_ZB_EIGHT_TLV_
#define H_ZB_EIGHT_TLV_

#include <zephyr.h>

#ifdef __cplusplus
extern "C" {
#endif

/* In a tlv area a entry has a type, a length and a value
 * A entry type of 0x0000 is reserved for internal usage (tlv end marker)
 */

struct tlv_entry {
    u16_t type;
    u16_t length;
    u8_t *value;
};

/**
 * @brief tlv API
 * @{
 */

/**
 * @brief zb_step_tlv
 *
 * step through the tlv.
 *
 * @param data: pointer to ram where to search for tlv
 * @param offset: offset in data to start read (updated by the routine)
 * @param entry: return buffer
 * @retval -ERRNO if error
 * @retval 0 if OK
 */
int zb_step_tlv(const void *data, u32_t *offset, struct tlv_entry *entry);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
