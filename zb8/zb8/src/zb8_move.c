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

int zb_img_cmd_proc(uint8_t sm_idx) {

	int rc;
	struct zb_cmd cmd;
	zb_move_cmd mcmd;
	struct zb_slt_info run_slt, move_slt, upgr_slt, swpstat_slt;
	struct zb_img_info run_info, move_info, upgr_info;
	bool in_place, upg2run_done, mov2upg_done;
	u32_t cmd_off, sectorsize;
	u32_t len;

	sectorsize = zb_sectorsize_get(sm_idx);
	if (!sectorsize) {
		return -EINVAL;
	}

	rc = zb_slt_open(&run_slt, sm_idx, RUN);
	if (rc) {
		return rc;
	}

	rc = zb_slt_open(&move_slt, sm_idx, MOVE);
	if (rc) {
		return rc;
	}

	rc = zb_slt_open(&upgr_slt, sm_idx, UPGRADE);
	if (rc) {
		return rc;
	}

	rc = zb_slt_open(&swpstat_slt, sm_idx, SWPSTAT);
	if (rc) {
		return rc;
	}

	in_place = zb_inplace_slt(sm_idx);
	upg2run_done = false;
	mov2upg_done = false;
	run_info.key_ok = true;
	move_info.key_ok = false;
	upgr_info.key_ok = false;

	LOG_INF("Doing %s upgrade", in_place ? "INPLACE" : "CLASSIC");

	rc = zb_cmd_read(&swpstat_slt, &cmd);
	if (rc == -ENOENT) {
		cmd.cmd1 = CMD1_SWAP;
		cmd.cmd2 = CMD_EMPTY;
		cmd.cmd3 = CMD_EMPTY;
	}

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
			LOG_INF("UPG2RUN [sector: %d]", cmd.cmd3);
			if (cmd.cmd3 == 0) {
				(void)zb_get_img_info(&upgr_info, &upgr_slt);
			} else {
				/* Headers have been swapped */
				(void)zb_get_img_info(&upgr_info, &run_slt);
			}
			mcmd.info = &upgr_info;
			mcmd.from = &upgr_slt;
			mcmd.to = &run_slt;
			mcmd.offset = cmd_off;
			if (cmd_off < upgr_info.end) {
				/* erase sector in move slot */
				zb_erase(mcmd.to, mcmd.offset, sectorsize);
				/* do the move */
				len = MIN(sectorsize, upgr_info.end - cmd_off);
				zb_img_move(&mcmd, len);
			} else {
				upg2run_done = true;
			}
			if (in_place) {
				if (upg2run_done) {
					cmd.cmd2 = CMD2_FINALISE;
				} else {
					cmd.cmd3++;
				}
			} else {
				cmd.cmd2 = CMD2_MOV2UPG;
			}
			break;
		case CMD2_MOV2UPG:
			LOG_INF("MOV2UPG [sector: %d]", cmd.cmd3);
			if (cmd.cmd3 == 0) {
				/* Image in the move slot have not been
				 * verified, there might be no image
				 */
				if (zb_get_img_info(&move_info, &move_slt)) {
					move_info.end = cmd_off;
				}
			} else {
				/* Headers have been swapped */
				/* Image in the move slot have not been
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
				/* erase sector in move slot */
				zb_erase(mcmd.to, mcmd.offset, sectorsize);
				/* do the move */
				len = MIN(sectorsize, move_info.end - cmd_off);
				zb_img_move(&mcmd, len);
			} else {
				mov2upg_done = true;
			}
			if (upg2run_done && mov2upg_done) {
				cmd.cmd2 = CMD2_FINALISE;
			} else {
				cmd.cmd3++;
				cmd.cmd2 = CMD2_UPG2RUN;
			}
			break;
		case CMD2_FINALISE:
			LOG_INF("Prepare image for booting");
			(void)zb_get_img_info(&run_info, &run_slt);
			if (run_info.confirmed) {
				zb_img_confirm(&run_slt);
			}
			cmd.cmd2 = CMD2_SWP_END;
			break;
		}
		rc = zb_cmd_write(&swpstat_slt, &cmd);
	}

	LOG_INF("Finished swap");

	return 0;
}

int zb_img_swap(uint8_t sm_idx)
{
	int rc;
	struct zb_slt_info swpstat_slt, run_slt, upgrade_slt;
	struct zb_cmd cmd;
	struct zb_img_info run, upgrade;

	/* Get info from run image */
	rc = zb_slt_open(&run_slt, sm_idx, RUN);
	if (rc) {
		return rc;
	}

	(void)zb_get_img_info(&run, &run_slt);

	/* Get last swapstat command */
	rc = zb_slt_open(&swpstat_slt, sm_idx, SWPSTAT);
	if (rc) {
		return rc;
	}

	rc = zb_cmd_read(&swpstat_slt, &cmd);

	if (rc != -ENOENT) {
		if (cmd.cmd2 != CMD2_SWP_END) {
			LOG_INF("Continuing swap...");
			return zb_img_cmd_proc(sm_idx);
		}
		if (!zb_inplace_slt(sm_idx)) {
			if (run.is_bootloader) {
				LOG_INF("Restoring after bootloader swap...");
				(void)zb_erase(&swpstat_slt, 0,
					       swpstat_slt.size);
				return zb_img_cmd_proc(sm_idx);
			}
			if (run.confirmed == false) {
				LOG_INF("Restoring after temporary swap...");
				(void)zb_erase(&swpstat_slt, 0,
					       swpstat_slt.size);
				return zb_img_cmd_proc(sm_idx);
			}
		} else {
			if (run.is_bootloader) {
				LOG_INF("Erasing bootloader from run slot...");
				(void)zb_erase(&run_slt, 0, run_slt.size);
			}
		}
		LOG_INF("Nothing to do ...");
		return 0;
	}

	/* New swap */
	rc = zb_slt_open(&upgrade_slt, sm_idx, UPGRADE);
	if (rc) {
		return rc;
	}

	if (zb_val_img_info(&upgrade, &upgrade_slt, &run_slt)) {
		LOG_ERR("BAD IMAGE in upgrade slot");
		return -EINVAL;
	}

	return zb_img_cmd_proc(sm_idx);

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