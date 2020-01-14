/*
 * Copyright (c) 2019 LaczenJMS.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <zb8/zb8_move.h>
#include <zb8/zb8_fsl.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(main);

void main(void)
{
	int cnt;

	cnt = zb_slt_area_cnt();

	/* Start or continue swap */
	while ((cnt--) > 0) {
		(void)zb_img_swap(cnt);
	}

	/* Boot image in run-0 slot or restart fsl */
	zb_fsl_boot(RUN_SLT);
	zb_fsl_boot(FSL_SLT);
}

