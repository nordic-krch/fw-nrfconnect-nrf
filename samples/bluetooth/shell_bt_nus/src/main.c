/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file
 *  @brief Nordic UART Bridge Service (NUS) sample
 */

#include <zephyr.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci.h>
#include <bluetooth/services/nus.h>
#include <logging/log.h>
#include <shell/shell_bt_nus.h>
#include <stdio.h>

LOG_MODULE_REGISTER(app);

#define STACKSIZE               CONFIG_BT_GATT_NUS_THREAD_STACK_SIZE
#define PRIORITY                7

#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN	        (sizeof(DEVICE_NAME) - 1)


static struct bt_conn *current_conn;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, NUS_UUID_SERVICE),
};

static void connected(struct bt_conn *conn, u8_t err)
{
	if (err) {
		LOG_ERR("Connection failed (err %u)", err);
		return;
	}

	LOG_INF("Connected");
	current_conn = bt_conn_ref(conn);
	shell_bt_nus_enable(conn);
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	LOG_INF("Disconnected (reason %u)", reason);

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}
}

static void /*__attribute__((unused))*/ security_changed(struct bt_conn *conn,
						     bt_security_t level)
{
	LOG_INF("Security level was raised to %d", level);
}

static struct bt_conn_cb conn_callbacks = {
	.connected    = connected,
	.disconnected = disconnected,
	COND_CODE_1(CONFIG_BT_SMP,
			(.security_changed = security_changed), ())
};

static char *addr_to_str(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	return log_strdup(addr);
}

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	LOG_INF("Passkey for %s: %06u", addr_to_str(conn), passkey);
}

static void auth_cancel(struct bt_conn *conn)
{
	LOG_INF("Pairing cancelled: %s", addr_to_str(conn));
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
	.passkey_display = IS_ENABLED(CONFIG_BT_SMP) ?
			auth_passkey_display : NULL,
	.passkey_entry = NULL,
	.cancel = IS_ENABLED(CONFIG_BT_SMP) ?
			auth_cancel : NULL,
};

static void bt_ready(int err)
{
	if (err) {
		LOG_ERR("BLE init failed with error code %d", err);
		return;
	}

	err = shell_bt_nus_init();
	if (err) {
		LOG_ERR("Failed to initialize BT NUS shell (err: %d)", err);
		return;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd,
			      ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
	}
	LOG_INF("Bluetooth ready");
}

void main(void)
{
	int err;

	err = bt_enable(bt_ready);
	if (err) {
		return;
	}

	bt_conn_cb_register(&conn_callbacks);

	if (IS_ENABLED(CONFIG_BT_SMP)) {
		bt_conn_auth_cb_register(&conn_auth_callbacks);
	}
}

