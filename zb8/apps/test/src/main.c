/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <zb8/zb8_fsl.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(main);

void main(void)
{
	LOG_INF("Welcome to the test application");

	/* In an application you can jump to the first stage loader,
	 * the swapper or the image loader by calling the routines:
	 *   zb_fsl_jump_fsl(),
	 *   zb_fsl_jump_swpr(),
	 *   zb_fsl_jump_ldr,
	 */
	LOG_INF("Let's jump back to the loader (if any)");
	zb_fsl_jump_ldr();
}
