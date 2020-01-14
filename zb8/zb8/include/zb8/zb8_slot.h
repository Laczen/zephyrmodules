/*
 * Slot definitions for zepboot
 *
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 #ifndef H_ZB_EIGHT_SLOT_
 #define H_ZB_EIGHT_SLOT_

#include <zephyr.h>
#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief slot_area: flash is separated in multiple slot areas, each of these
 * slt_area is divided into a run, move, upgrade and a swap status slot.
 * Images are placed in the upgrade slot and then swapped/decrypted into the
 * run slot.
 *
 * The swap status area is used to keep track of the swap and/or decryption
 * status by writing commands to it.
 *
 * The move slot is defined as the run slot offset by one sector. The sector
 * used during decryption is defined as the difference between the offset
 * of the move slot and the run slot.
 *
 * A swap operation is done as:
 *
 * a. Moving the run slot to the move slot (copying all sectors from the run
 *    slot up by one sector).
 * b. Sector by sector:
 *    1. Erasing the run sector, decrypt the upgrade sector to the run sector
 *    2. Erasing the upgrade sector, encrypt the move sector to the upgrade
 *       sector
 *
 * REMARK: The run, move and upgrade slots should be the same size.
 */

enum slot {RUN, MOVE, UPGRADE, SWPSTAT, RAM};

struct zb_slt_info {
    u32_t offset;
    u32_t size;
    struct device *fl_dev;
};

struct zb_slt_area {
    u32_t run_offset;
    u32_t move_offset;
    u32_t upgrade_offset;
    u32_t swpstat_offset;
    u32_t run_size;
    u32_t move_size;
    u32_t upgrade_size;
    u32_t swpstat_size;
    const char *run_devname;
    const char *move_devname;
    const char *upgrade_devname;
    const char *swpstat_devname;
};

/**
 * @}
 */

/**
 * @brief zb_slot API: routines for using slots
 */

/**
 * @brief zb_slt_area_cnt
 *
 * Returns the number of slt_areas in slot_map.
 *
 * @retval cnt number of slot areas
 */
u8_t zb_slt_area_cnt(void);

/**
 * @brief zb_slt_open
 *
 * Opens the slot slotmap[sm_idx] slot xxx, fills in required info to use.
 *
 * @param fs Pointer to zb_slt_info
 * @param sm_idx Index to slot_area: slotmap[sm_idx].
 * @param slot RUN, MOVE, UPGRADE or SWPSTAT
 * @retval 0 Success
 * @retval -ERRNO errno code if error
 */
int zb_slt_open(struct zb_slt_info *info, u8_t sm_idx, enum slot slot);

/**
 * @brief zb_sectorsize_get
 *
 * Returns the sector size for slotmap[sm_idx].
 *
 * @param sm_idx Index to slot_area: slotmap[sm_idx].
 * @retval positive sector size if successful
 * @retval 0 if error
 */
u32_t zb_sectorsize_get(u8_t sm_idx);

/**
 * @brief zb_range_in_slt
 *
 * Check if a range belongs to a slot.
 *
 * @param sm_idx Index to slot_area: slotmap[sm_idx]
 * @param address range start address
 * @param len range length
 * @retval true if range belongs to slot, false if not
 */
bool zb_range_in_slt(struct zb_slt_info *info, u32_t address, size_t len);

/**
 * @brief zb_inplace_slt
 *
 * Check if a slot area is of type inplace (move == upgrade).
 *
 * @param sm_idx Index to slot_area: slotmap[sm_idx]
 * @retval true if slot area is of type inplace
 */
bool zb_inplace_slt(u8_t sm_idx);
/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
