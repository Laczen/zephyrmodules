/*
 * Copyright (c) 2019 LaczenJMS.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <zb8/zb8_move.h>
#include <zb8/zb8_flash.h>
#include <zb8/zb8_fsl.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(main);

void main(void)
{
	u8_t cnt;
	int rc;
	struct zb_slt_info run;
	struct zb_fsl_hdr hdr;

	LOG_INF("Welcome to ZB-8 swapper");

	cnt = zb_slt_area_cnt();

	/* Start or continue swap */
	while ((cnt--) > 1) {
		rc = zb_img_swap(cnt);
		if (rc) {
			LOG_INF("Swap failed for slot %u [%d]", cnt, rc);
		}
	}

	rc = zb_img_swap(0);
	if (!rc) {
		/* Get info from run_0 image */
		(void)zb_slt_open(&run, 0, RUN);
		(void)zb_read(&run, 0U, &hdr, sizeof(struct zb_fsl_hdr));

		if (hdr.magic == FSL_MAGIC) {

			hdr.run_offset -= hdr.hdr_info.size;
			if ((hdr.run_offset == DT_FLASH_AREA_SWPR_OFFSET) ||
			    (hdr.run_offset == DT_FLASH_AREA_LDR_OFFSET)) {
				LOG_INF("Restarting for swapper/loader update");
				zb_fsl_jump_fsl();
			}
		}

	} else {
		LOG_INF("Swap failed for slot 0 [%d]", rc);
	}

	if (rc && zb_inplace_slt(0)) {
		LOG_INF("Falling back to loader image");
		zb_fsl_jump_ldr();
	}

	/* TODO Add support for RAM images */
	zb_fsl_jump_run();

	LOG_ERR("Starting failed, try reset");

}

