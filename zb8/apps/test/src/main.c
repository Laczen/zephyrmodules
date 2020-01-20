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
}
