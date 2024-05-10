/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief Peripheral Heart Rate over LE Coded PHY sample
 */
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/sys/printk.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/services/hrs.h>
#include <zephyr/drivers/uart.h>

#include <dk_buttons_and_leds.h>

#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)

#define RUN_STATUS_LED          DK_LED1
#define CON_STATUS_LED          DK_LED2
#define RUN_LED_BLINK_INTERVAL  1000
#define NOTIFY_INTERVAL         1000

static void start_advertising_coded(struct k_work *work);
static void notify_work_handler(struct k_work *work);

static K_WORK_DEFINE(start_advertising_worker, start_advertising_coded);
static K_WORK_DELAYABLE_DEFINE(notify_work, notify_work_handler);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HRS_VAL),
					  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL),
					  BT_UUID_16_ENCODE(BT_UUID_DIS_VAL)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN)
};
static const struct bt_le_adv_param advertising_params =
BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_CONNECTABLE,
		BT_GAP_ADV_FAST_INT_MIN_2,
		BT_GAP_ADV_FAST_INT_MAX_2,
		NULL);
static const struct device *const uart_dev = DEVICE_DT_GET(DT_CHOSEN(app_uart));

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	int err;
	struct bt_conn_info info;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		printk("Connection failed (err %d)\n", conn_err);
		return;
	}

	err = bt_conn_get_info(conn, &info);
	if (err) {
		printk("Failed to get connection info (err %d)\n", err);
	} else {
		const struct bt_conn_le_phy_info *phy_info;
		phy_info = info.le.phy;

		printk("Connected: %s, tx_phy %u, rx_phy %u\n",
		       addr, phy_info->tx_phy, phy_info->rx_phy);
	}

	dk_set_led_on(CON_STATUS_LED);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason 0x%02x)\n", reason);

	k_work_submit(&start_advertising_worker);

	dk_set_led_off(CON_STATUS_LED);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void start_advertising_coded(struct k_work *work)
{
	int err;

	err = bt_le_adv_start(&advertising_params, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Failed to start advertising set (err %d)\n", err);
		return;
	}

	printk("Advertising started\n");
}

static void bas_notify(void)
{
	uint8_t battery_level = bt_bas_get_battery_level();

	__ASSERT_NO_MSG(battery_level > 0);

	battery_level--;

	if (!battery_level) {
		battery_level = 100;
	}

	bt_bas_set_battery_level(battery_level);
}

static void hrs_notify(void)
{
	static uint8_t heartrate = 100;

	heartrate++;
	if (heartrate == 160) {
		heartrate = 100;
	}

	bt_hrs_notify(heartrate);
}

static void notify_work_handler(struct k_work *work)
{
	/* Services data simulation. */
	hrs_notify();
	bas_notify();

	k_work_reschedule(k_work_delayable_from_work(work), K_MSEC(NOTIFY_INTERVAL));
}

static uint8_t my_uart_buffer[100];

static void uart_callback(const struct device *const uart_dev,
			  struct uart_event *const event,
			  void *const user_data) {
	switch (event->type) {
		case UART_RX_BUF_REQUEST:
			printk("uart cb type UART_RX_BUF_REQUEST\n");
			uart_rx_buf_rsp(uart_dev, my_uart_buffer, sizeof(my_uart_buffer));
			break;
		default:
			printk("uart cb type %d\n", event->type);
			break;
	}
}

int main(void)
{
	uint32_t led_status = 0;
	int err;

	printk("Starting Bluetooth Peripheral HR coded example\n");

	err = dk_leds_init();
	if (err) {
		printk("LEDs init failed (err %d)\n", err);
		return 0;
	}


	err = uart_callback_set(uart_dev, uart_callback, NULL);
	if (err) {
		printk("Failed to set UART driver callback: %d\n", err);
		return err;
	}

	static uint8_t memory[100];
	err = uart_rx_enable(uart_dev, memory, sizeof(memory), 100);
	if (err) {
		printk("Failed to enable UART RX: %d\n", err);
		return err;
	}

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	printk("Bluetooth initialized\n");

	k_work_submit(&start_advertising_worker);
	k_work_schedule(&notify_work, K_NO_WAIT);

	for (;;) {
		dk_set_led(RUN_STATUS_LED, (++led_status) % 2);
		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
	}
}
