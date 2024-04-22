#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

static void start_scan(void);

static struct bt_conn *default_conn;
static const char *const scan_name = "buggy-device";

struct bt_data_parse_state {
  bool longname_ok;
};

static bool eir_found(struct bt_data *data, void *user_data) {
  struct bt_data_parse_state *state = user_data;
  const size_t scan_name_length = strlen(scan_name);

  switch (data->type) {
  case BT_DATA_NAME_COMPLETE:
    if (scan_name_length == 0) {
      break;
    }
    if (scan_name_length != data->data_len) {
      break;
    }
    if (memcmp(data->data, scan_name, data->data_len)) {
      break;
    }
    state->longname_ok = true;
    break;
  default:
    return true;
  }
  return true;
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                         struct net_buf_simple *ad) {
  char dev[BT_ADDR_LE_STR_LEN];
  struct bt_data_parse_state state = {};

  bt_addr_le_to_str(addr, dev, sizeof(dev));

  /* We're only interested in connectable events */
  if (type == BT_GAP_ADV_TYPE_ADV_IND ||
      type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
    bt_data_parse(ad, eir_found, (void *)&state);

    if (state.longname_ok) {
      LOG_INF("Found dev %s ... connecting", dev);
      int err = bt_le_scan_stop();
      if (err) {
        LOG_ERR("Stop LE scan failed (err %d)", err);
        return;
      }

      struct bt_le_conn_param *param = BT_LE_CONN_PARAM_DEFAULT;
      err =
          bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, param, &default_conn);
      if (err) {
        LOG_ERR("Create conn failed (err %d)", err);
        start_scan();
      }
    }
  }
}

static void start_scan(void) {
  int err;

  /* Use active scanning and disable duplicate filtering to handle any
   * devices that might update their advertising data at runtime.
   */
  struct bt_le_scan_param scan_param = {
      .type = BT_LE_SCAN_TYPE_ACTIVE,
      .options = BT_LE_SCAN_OPT_NONE,
      .interval = BT_GAP_SCAN_FAST_INTERVAL,
      .window = BT_GAP_SCAN_FAST_WINDOW,
  };

  err = bt_le_scan_start(&scan_param, device_found);
  if (err) {
    LOG_ERR("Scanning failed to start (err %d)", err);
    return;
  }

  LOG_INF("Scanning successfully started");
}

static void mtu_exchange_cb(struct bt_conn *conn, uint8_t mtu_err,
                            struct bt_gatt_exchange_params *params) {
  if (mtu_err != 0) {
    LOG_ERR("BT error while mtu exchange: %d", mtu_err);
    bt_conn_disconnect(conn, BT_HCI_ERR_UNACCEPT_CONN_PARAM);
    return;
  }

  LOG_INF("MTU exchange done.");
}

static struct bt_gatt_exchange_params mtu_exchange = {.func = mtu_exchange_cb};

static void connected(struct bt_conn *conn, uint8_t conn_err) {
  char addr[BT_ADDR_LE_STR_LEN];
  int err;

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  if (conn_err) {
    LOG_ERR("Failed to connect to %s (%u)", addr, conn_err);

    bt_conn_unref(default_conn);
    default_conn = NULL;

    start_scan();
    return;
  }

  LOG_INF("Connected: %s", addr);

  if (conn == default_conn) {
    err = bt_gatt_exchange_mtu(conn, &mtu_exchange);
    if (err) {
      LOG_ERR("MTU exchange failed (err %d)", err);
      bt_conn_disconnect(conn, BT_HCI_ERR_UNACCEPT_CONN_PARAM);
    }
  }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  LOG_INF("Disconnected: %s (reason 0x%02x)", addr, reason);

  if (default_conn != conn) {
    return;
  }

  bt_conn_unref(default_conn);
  default_conn = NULL;

  start_scan();
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
  if (conn != default_conn) {
    return;
  }

  if (err) {
    LOG_ERR("%s err: %d", __func__, err);
    return;
  }

  LOG_INF("Security level set to %u", level);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .security_changed = security_changed,
};

int main(void) {
  int ret;

  ret = bt_enable(NULL);
  if (ret) {
    LOG_ERR("Bluetooth init failed: %d", ret);
    return ret;
  }

  LOG_DBG("Bluetooth initialized");

  start_scan();

  return 0;
}
