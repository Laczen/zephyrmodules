/*
 *
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef H_ZB_EIGHT_MOVE_
#define H_ZB_EIGHT_MOVE_

#include <zephyr.h>
#include <zb8/zb8_slot.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOVE_BLOCK_SIZE 512

/*
 * Commands are written to flash as a set of 3 bytes (cmd1, cmd2, cmd3)
 * followed by a crc8. cmd1 is used to track general properties: type of swap,
 * error state, ... cmd2 is used to track the steps during the swap process and
 * cmd3 is used to track the sector being processed.
 */

#define CMD_EMPTY		0b11111111

/* cmd1 definitions */
#define CMD1_SWAP		0b01111111
#define CMD1_ERROR		0b00000000


/* cmd2 definitions */
#define CMD2_RUN2MOV		0b01111111 /* Copy run to move (top to bottom)*/
#define CMD2_UPG2RUN		0b00111110 /* Decrypt upgrade to run:
				    	    * a. erase run sect x,
					    * b. decrypt upgr sect x to run x
					    */
#define CMD2_MOV2UPG		0b00011111 /* Encrypt move to upgrade:
					    * a. erase upg sect x,
					    * b. encrypt mov sect x to upg x
					    */
#define CMD2_FINALISE		0b00001111 /* Finalise run: update image crc32
					    * so that fsl can start it
					    */
#define CMD2_SWP_END		0b00000000

/**
 * @brief zb_move_cmd: necessary info to do a move of a sector or a move from
 * flash to ram
 * @{
 */

typedef struct {
	struct zb_img_info *info; /* info about moved image */
	struct zb_slt_info *from; /* from slot */
	struct zb_slt_info *to;   /* to slot */
	u32_t offset; /* offset of sector that is moved */
} zb_move_cmd;

/**
 * @}
 */

/**
 * @brief zb_move API
 * @{
 */

/**
 * @brief zb_img_swap
 *
 * Continues or starts swapping of images in specified area
 *
 * @param[in] area Pointer to zb_slt_area that contains the images to be swapped
 * @retval 0 Success
 * @retval -ERRNO errno code if error
 */
int zb_img_swap(uint8_t sm_idx);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
