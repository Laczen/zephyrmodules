/*
 * Copyright (c) 2019 LaczenJMS.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <zb8/zb8_move.h>
#include <zb8/zb8_image.h>
#include <zb8/zb8_fsl.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(main);

void main(void)
{
	u8_t cnt;
	struct zb_slt_info run_slt;
	struct zb_img_info run;

	LOG_INF("Welcome to ZB-8 swapper");

	cnt = zb_slt_area_cnt();

	/* Start or continue swap */
	while ((cnt--) > 0) {
		(void)zb_img_swap(cnt);
	}

	/* Get info from run_0 image */
	(void)zb_slt_open(&run_slt, 0, RUN);
	(void)zb_get_img_info(&run, &run_slt);

	/* If image in slot 0 is a bootloader call FSL */
	if (run.is_bootloader) {
		LOG_INF("Restarting FSL to upgrade bootloader");
		zb_fsl_jump_fsl();
	}

	/* TODO Add support for RAM images */

	/* Otherwise start the image */
	LOG_INF("Starting the image in run 0 slot");
	zb_fsl_jump_run();
}

