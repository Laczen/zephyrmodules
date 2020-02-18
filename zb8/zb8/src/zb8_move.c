/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zb8/zb8_move.h>
#include <zb8/zb8_flash.h>
#include <zb8/zb8_image.h>
#include <zb8/zb8_fsl.h>

#include <errno.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(zb_move);

int zb_img_move(zb_move_cmd *mcmd, size_t len);

int zb_img_swap(uint8_t sm_idx) {

	int rc, dp_err = 0;
	struct zb_cmd cmd;
	zb_move_cmd mcmd;
	struct zb_slt_info run_slt, move_slt, upgr_slt, swpstat_slt;
	struct zb_img_info run_info, move_info, upgr_info;
	bool in_place, save_stat, upg2run_done, mov2upg_done, continue_swap;
	u32_t cmd_off, sectorsize;
	u32_t len;

	sectorsize = zb_sectorsize_get(sm_idx);
	if (!sectorsize) {
		return -EINVAL;
	}

	rc = zb_slt_open(&run_slt, sm_idx, RUN);
	rc = rc | zb_slt_open(&move_slt, sm_idx, MOVE);
	rc = rc | zb_slt_open(&upgr_slt, sm_idx, UPGRADE);
	rc = rc | zb_slt_open(&swpstat_slt, sm_idx, SWPSTAT);
	if (rc) {
		return rc;
	}

	/* reset the image info ..._ok = false */
	zb_res_img_info(&run_info);
	zb_res_img_info(&move_info);
	zb_res_img_info(&upgr_info);
	/* images in run or move slot will fail image hash verification if they
	 * use encryption, disable image hash verification, these images also
	 * don't need the dependencies to be verified.
	 */
	run_info.img_ok = true;
	run_info.dep_ok = true;
	move_info.img_ok = true;
	move_info.dep_ok = true;

	continue_swap = false;
	save_stat = (!swpstat_slt.size) ? false : true;
	in_place = zb_inplace_slt(sm_idx);

	run_info.hdr_ok = true;
	run_info.key_ok = true;
	if (!save_stat) {
		/* Swap status is not saved to flash */
		if (!in_place) {
			LOG_ERR("BAD Configuration");
			return -EINVAL;
		}
		if (zb_slt_has_img_hdr(&upgr_slt)) {
			/* upgrade slot contains no image */
			(void)zb_get_img_info(&run_info, &run_slt);
			if (run_info.is_bootloader) {
				LOG_INF("Erasing BL from run slot...");
				(void)zb_erase(&run_slt, 0, run_slt.size);
				return -EINVAL;
			}
			if (!run_info.confirmed) {
				LOG_INF("Unconfirmed image in run slot...");
				return -EINVAL;
			}
			return 0;
		}
		/* upgrade slot contains image */
		cmd.cmd1 = CMD_EMPTY;
		cmd.cmd2 = CMD_EMPTY;
		cmd.cmd3 = CMD_EMPTY;
	} else {
		/* status is saved to flash */
		if (zb_cmd_read(&swpstat_slt, &cmd) != -ENOENT) {
			(void)zb_get_img_info(&run_info, &run_slt);
			if (cmd.cmd2 == CMD2_SWP_END) {
				if ((!run_info.is_bootloader) &&
				    run_info.confirmed) {
					LOG_INF("Nothing to do...");
					return 0;
				}
				if ((in_place) && (run_info.is_bootloader)) {
					LOG_INF("Erasing BL from run slot...");
					(void)zb_erase(&run_slt, 0,
						       run_slt.size);
					/* Avoid booting this image */
					return -EINVAL;
				}
				LOG_INF("Restoring previous image...");
			} else {
				continue_swap = true;
				LOG_INF("Continuing swap...");
			}
		} else {
			cmd.cmd1 = CMD_EMPTY;
			cmd.cmd2 = CMD_EMPTY;
			cmd.cmd3 = CMD_EMPTY;
		}
	}
	run_info.hdr_ok = false;
	run_info.key_ok = false;

	if (!continue_swap) {
		if (zb_val_img_info(&upgr_info, &upgr_slt, &run_slt)) {
			LOG_ERR("BAD IMAGE in upgrade slot");
			return -EINVAL;
		}
		if (cmd.cmd2 == CMD2_SWP_END) {
			(void)zb_erase(&swpstat_slt, 0, swpstat_slt.size);
		}
		cmd.cmd1 = CMD1_SWAP;
		cmd.cmd2 = CMD_EMPTY;
		cmd.cmd3 = CMD_EMPTY;
	}

	LOG_INF("Doing %s upgrade", in_place ? "INPLACE" : "CLASSIC");

	upg2run_done = false;
	mov2upg_done = in_place;

	while (1) {
		if (cmd.cmd2 == CMD2_SWP_END) {
			break;
		}

		cmd_off = cmd.cmd3 * sectorsize;

		switch (cmd.cmd2) {
		case CMD_EMPTY: /* start move */
			LOG_INF("Start move");
			cmd.cmd3 = 0;
			if (in_place) {
				LOG_INF("No RUN2MOV required");
				cmd.cmd2 = CMD2_UPG2RUN;
				cmd.cmd3 = 0;
				break;
			}
			if (zb_get_img_info(&run_info, &run_slt)) {
				LOG_INF("No move required");
				cmd.cmd2 = CMD2_UPG2RUN;
				cmd.cmd3 = 0;
				break;
			}
			while ((cmd.cmd3 + 1) * sectorsize < run_info.end) {
				cmd.cmd3++;
			}
			cmd.cmd2 = CMD2_RUN2MOV;
			break;
		case CMD2_RUN2MOV: /* copy run to move slot */
			LOG_INF("RUN2MOV [sector:%d]", cmd.cmd3);
			(void)zb_get_img_info(&run_info, &run_slt);
			run_info.enc_start = run_info.end;
			mcmd.info = &run_info;
			mcmd.from = &run_slt;
			mcmd.to = &move_slt;
			mcmd.offset = cmd_off;

			/* erase sector in move slot */
			zb_erase(mcmd.to, mcmd.offset, sectorsize);
			/* do the move */
			len = MIN(sectorsize, run_info.end - cmd_off);
			zb_img_move(&mcmd, sectorsize);

			/* until cmd.sector = 0 */
			if (cmd.cmd3 == 0) {
				cmd.cmd2 = CMD2_UPG2RUN;
			} else {
				cmd.cmd3--;
			}
			break;
			/* Swap sectors */
		case CMD2_UPG2RUN: /* Move from upgrade to run */
			if (upg2run_done) {
				if (in_place) {
					cmd.cmd2 = CMD2_FINALISE;
				} else {
					cmd.cmd2 = CMD2_MOV2UPG;
				}
				break;
			}
			LOG_INF("UPG2RUN [sector: %d]", cmd.cmd3);
			/* erase sector in move slot */
			zb_erase(&run_slt, cmd_off, sectorsize);
			if (cmd.cmd3 == 0) {
				if (!in_place) {
					upgr_info.dep_ok = false;
				}
				dp_err = zb_get_img_info(&upgr_info, &upgr_slt);
				if (dp_err) {
				/* downgrade protection: if tampering is
				 * detected, drop back the old image
				 */
					LOG_INF("TAMPERING");
					(void)zb_slt_open(&upgr_slt, sm_idx,
							  MOVE);
					in_place = true;
					break;
				}
			} else {
				/* Headers have been swapped */
				(void)zb_get_img_info(&upgr_info, &run_slt);
			}
			if (dp_err) {
				/* do not decrypt dropping the old image */
				upgr_info.enc_start = upgr_info.end;
			}
			mcmd.info = &upgr_info;
			mcmd.from = &upgr_slt;
			mcmd.to = &run_slt;
			mcmd.offset = cmd_off;
			if (cmd_off < upgr_info.end) {
				/* do the move */
				len = MIN(sectorsize, upgr_info.end - cmd_off);
				zb_img_move(&mcmd, len);
			} else {
				upg2run_done = true;
			}
			if (in_place) {
				cmd.cmd3++;
			} else {
				cmd.cmd2 = CMD2_MOV2UPG;
			}
			break;
		case CMD2_MOV2UPG:
			if (mov2upg_done) {
				if (upg2run_done) {
					cmd.cmd2 = CMD2_FINALISE;
				} else {
					cmd.cmd3++;
					cmd.cmd2 = CMD2_UPG2RUN;
				}
				break;
			}
			LOG_INF("MOV2UPG [sector: %d]", cmd.cmd3);
			/* erase sector in upgrade slot */
			zb_erase(&upgr_slt, cmd_off, sectorsize);
			if (cmd.cmd3 == 0) {
				/* Image in the move slot has not been
				 * verified, there might be no image
				 */
				if (zb_get_img_info(&move_info, &move_slt)) {
					move_info.end = cmd_off;
				}
			} else {
				/* Headers have been swapped */
				/* Image in the move slot has not been
				 * verified, there might be no image
				 */
				if (zb_get_img_info(&move_info, &upgr_slt)) {
					move_info.end = cmd_off;
				}
			}
			mcmd.info = &move_info;
			mcmd.from = &move_slt;
			mcmd.to = &upgr_slt;
			mcmd.offset = cmd_off;
			if (cmd_off < move_info.end) {
				/* do the move */
				len = MIN(sectorsize, move_info.end - cmd_off);
				zb_img_move(&mcmd, len);
			} else {
				mov2upg_done = true;
			}
			cmd.cmd3++;
			cmd.cmd2 = CMD2_UPG2RUN;
			break;
		case CMD2_FINALISE:
			LOG_INF("Prepare image for booting");
			(void)zb_get_img_info(&run_info, &run_slt);
			if ((run_info.confirmed) || (in_place)) {
				(void)zb_img_confirm(&run_slt);
			}
			cmd.cmd2 = CMD2_SWP_END;
			break;
		}
		if (save_stat) {
			(void)zb_cmd_write(&swpstat_slt, &cmd);
		}
	}
	LOG_INF("Finished swap");
	if (dp_err) {
		LOG_INF("Detected tampering");
	}

	return 0;
}

int zb_img_move(zb_move_cmd *mcmd, size_t len)
{
	u8_t buf[MOVE_BLOCK_SIZE];
	u8_t ctr[AES_KEY_SIZE] = {0U};
	u32_t ulen = 0; /* unencrypted length */
	u32_t off;
	int j;

	LOG_INF("Sector move: FR [off %x] TO [off %x]",
		mcmd->from->offset + mcmd->offset,
		mcmd->to->offset + mcmd->offset);

	if (mcmd->offset < mcmd->info->enc_start) {
		ulen = mcmd->info->enc_start - mcmd->offset;
	}

	memcpy(ctr, mcmd->info->enc_nonce, AES_KEY_SIZE);

	off = mcmd->info->enc_start;
	while ((off < mcmd->offset) && (!ulen)) {
		/* ctr increment is required */
		for (j = AES_KEY_SIZE; j > 0; --j) {
                	if (++ctr[j - 1] != 0) {
                    		break;
                	}
            	}
		off += AES_BLOCK_SIZE;
	}

	off = mcmd->offset;

	while (len) {
		size_t buf_len = MIN(len, MOVE_BLOCK_SIZE);

		if (ulen) {
			buf_len = MIN(buf_len, ulen);
		}

		(void)zb_read(mcmd->from, off, buf, buf_len);

		if (!ulen) {
			(void)zb_aes_ctr_mode(buf, buf_len, ctr,
					      mcmd->info->enc_key);
		}

		(void)zb_write(mcmd->to, off, buf, buf_len);

		if (ulen) {
			ulen -= buf_len;
		}
		len -= buf_len;
		off += buf_len;
	}

	return 0;
}