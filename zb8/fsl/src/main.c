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
	zb_fsl_boot();
	/* You should not get here */
	LOG_ERR("Starting failed, try reset");
}
