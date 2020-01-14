/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zb8/zb8_fsl.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

void main(void)
{
	struct zb_fsl_hdr hdr;
	struct device *fl_dev;

	hdr.run_offset = DT_FLASH_AREA_RUN_0_OFFSET,
	fl_dev = device_get_binding(DT_FLASH_AREA_RUN_0_DEV);

	switch(zb_fsl_get_type(fl_dev, &hdr)) {
	case RUN_IMG:
		zb_fsl_boot(RUN_SLT);
	case BOOT_IMG:
		zb_fsl_blupgrade(fl_dev, &hdr);
	case INVALID_IMG:
		zb_fsl_boot(BOOT_SLT);
	}
	/* You should not get here */
	LOG_ERR("Starting failed, try reset");
}
