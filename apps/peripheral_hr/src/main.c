/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/services/hrs.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/drivers/uart.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};
static const struct bt_le_adv_param advertising_params = BT_LE_ADV_PARAM_INIT(
	BT_LE_ADV_OPT_USE_NAME | BT_LE_ADV_OPT_FORCE_NAME_IN_AD | BT_LE_ADV_OPT_CONNECTABLE, 1600,
	1920, NULL);
static const struct device *const uart_dev = DEVICE_DT_GET(DT_CHOSEN(app_uart));

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection failed (err 0x%02x)", err);
	} else {
		LOG_INF("Connected");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason 0x%02x)", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void bt_ready(void)
{
	int err;

	LOG_INF("Bluetooth initialized");

	err = bt_le_adv_start(&advertising_params, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return;
	}

	LOG_INF("Advertising successfully started");
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_ERR("Pairing cancelled: %s", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
	.cancel = auth_cancel,
};

static void bas_notify(void)
{
	uint8_t battery_level = bt_bas_get_battery_level();

	battery_level--;

	if (!battery_level) {
		battery_level = 100U;
	}

	bt_bas_set_battery_level(battery_level);
}

static void hrs_notify(void)
{
	static uint8_t heartrate = 90U;

	/* Heartrate measurements simulation */
	heartrate++;
	if (heartrate == 160U) {
		heartrate = 90U;
	}

	bt_hrs_notify(heartrate);
}

static void uart_callback(const struct device *const uart_dev, struct uart_event *const event,
			  void *const user_data)
{
}

static void work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	int ret;

	ret = uart_callback_set(uart_dev, uart_callback, NULL);
	if (ret) {
		LOG_ERR("Failed to set UART driver callback: %d", ret);
		return;
	}

	static uint8_t memory[100];
	ret = uart_rx_enable(uart_dev, memory, sizeof(memory), 100);
	if (ret) {
		LOG_ERR("Failed to enable UART RX: %d", ret);
		return;
	}

	LOG_INF("UART initialized");
}
static K_WORK_DELAYABLE_DEFINE(work, work_handler);

int main(void)
{
	int ret;

	ret = k_work_schedule(&work, K_SECONDS(1));
	if (ret < 0) {
		LOG_ERR("Can't schedule work: %d", ret);
	}

	ret = bt_enable(NULL);
	if (ret) {
		LOG_ERR("Bluetooth init failed (ret %d)", ret);
		return ret;
	}

	bt_ready();

	bt_conn_auth_cb_register(&auth_cb_display);

	LOG_INF("BT ready");

	/* Implement notification. At the moment there is no suitable way
	 * of starting delayed work so we do it here
	 */
	while (1) {
		k_sleep(K_SECONDS(1));

		/* Heartrate measurements simulation */
		hrs_notify();

		/* Battery level simulation */
		bas_notify();
	}
	return 0;
}
