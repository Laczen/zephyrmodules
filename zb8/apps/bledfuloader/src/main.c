/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>

#include <bluetooth/gatt.h>

#include <zepboot/zb_fsl.h>
#include <zepboot/zb_dfu.h>
#include "nrf_dfu.h"


#include <logging/log.h>
LOG_MODULE_REGISTER(main);

struct bt_conn *default_conn;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x0d, 0x18, 0x0f, 0x18, 0x05, 0x18),
};

static void connected(struct bt_conn *conn, u8_t err)
{
	if (err) {
		LOG_DBG("Connection failed (err 0x%02x)", err);
	} else {
		default_conn = bt_conn_ref(conn);
		LOG_INF("Connected");
	}
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	LOG_INF("Disconnected (reason 0x%02x)", reason);

	if (default_conn) {
		bt_conn_unref(default_conn);
		default_conn = NULL;
	}
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static void bt_ready(int err)
{
	if (err) {
		LOG_DBG("Bluetooth init failed (err %d)", err);
		return;
	}

	LOG_INF("Bluetooth initialized");

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_DBG("Advertising failed to start (err %d)", err);
		return;
	}

	LOG_INF("Advertising successfully started");
}

static void reset_to_app(void)
{
	u8_t sm_idx;
	struct zb_fsl_ctx ctxb;

	zb_dfu_receive_flush(&sm_idx);
	LOG_INF("Receive finished, restarting to bootloader");
	/* Restart the bootloader */
	ctxb.addr = DT_FLASH_AREA_BOOT_OFFSET;
	ctxb.fl_dev = device_get_binding(DT_FLASH_AREA_BOOT_DEV);
	zb_fsl_verify(&ctxb);
	zb_fsl_boot(&ctxb);


}

struct nrf_dfu_cb dfu_callbacks = {
	.receive = &zb_dfu_receive,
	.reset_to_app = &reset_to_app,

};

static void dfu_init(void)
{
	nrf_dfu_init(&dfu_callbacks, 0U, 0U);
}

void main(void)
{
	int err;

	err = bt_enable(bt_ready);
	if (err) {
		LOG_DBG("Bluetooth init failed (err %d)", err);
		return;
	}

	bt_conn_cb_register(&conn_callbacks);
	dfu_init();
	LOG_INF("Welcome to ZEPBOOT bledfu uploader\n");

	while (1) {
		k_sleep(MSEC_PER_SEC);
	}
}
