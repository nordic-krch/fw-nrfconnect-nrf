/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <shell/shell_bt_nus.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(shell_bt_nus, CONFIG_SHELL_BT_NUS_LOG_LEVEL);

SHELL_BT_NUS_DEFINE(shell_transport_bt_nus, 32, 32);
SHELL_DEFINE(shell_bt_nus, "bt_nus:~$ ", &shell_transport_bt_nus,
	     CONFIG_SHELL_BACKEND_SERIAL_LOG_MESSAGE_QUEUE_SIZE,
	     CONFIG_SHELL_BACKEND_SERIAL_LOG_MESSAGE_QUEUE_TIMEOUT,
	     SHELL_FLAG_OLF_CRLF);

static bool is_init;

static void rx_callback(struct bt_conn *conn, const u8_t *const data, u16_t len)
{
	const struct shell_bt_nus *bt_nus =
		(const struct shell_bt_nus *)shell_transport_bt_nus.ctx;
	u32_t done = ring_buf_put(bt_nus->rx_ringbuf, data, len);

	LOG_DBG("Received %d bytes.", len);
	if (done < len) {
		LOG_WRN("RX ring buffer full. Dropping %d bytes", len - done);
	}

	bt_nus->ctrl_blk->handler(SHELL_TRANSPORT_EVT_RX_RDY,
				  bt_nus->ctrl_blk->context);
}

static void tx_try(const struct shell_bt_nus *bt_nus)
{
	u8_t *buf;
	u32_t size;

	size = ring_buf_get_claim(bt_nus->tx_ringbuf, &buf, 20 /*todo*/);

	if (size) {
		int err = bt_gatt_nus_send(bt_nus->ctrl_blk->conn, buf, size);

		if (err == 0) {
			ring_buf_get_finish(bt_nus->tx_ringbuf, size);
			LOG_DBG("Sent %d bytes", size);
		} else {
			LOG_WRN("Failed to send %d bytes (%d error)",
								size, err);
			ring_buf_get_finish(bt_nus->tx_ringbuf, size);
			bt_nus->ctrl_blk->tx_busy = 0;
		}
	} else {
		bt_nus->ctrl_blk->tx_busy = 0;
	}
}

static void tx_callback(struct bt_conn *conn)
{
	const struct shell_bt_nus *bt_nus =
		(const struct shell_bt_nus *)shell_transport_bt_nus.ctx;


	LOG_DBG("Sent operation completed");
	tx_try(bt_nus);
	bt_nus->ctrl_blk->handler(SHELL_TRANSPORT_EVT_TX_RDY,
				  bt_nus->ctrl_blk->context);
}

static int init(const struct shell_transport *transport,
		const void *config,
		shell_transport_handler_t evt_handler,
		void *context)
{
	const struct shell_bt_nus *bt_nus =
			(struct shell_bt_nus *)transport->ctx;

	LOG_DBG("Initialized");
	bt_nus->ctrl_blk->handler = evt_handler;
	bt_nus->ctrl_blk->context = context;

	return 0;
}

static int uninit(const struct shell_transport *transport)
{
	return 0;
}

static int enable(const struct shell_transport *transport, bool blocking_tx)
{
	const struct shell_bt_nus *bt_nus =
			(struct shell_bt_nus *)transport->ctx;

	if (blocking_tx) {
		/* transport cannot work in blocking mode, shut down */
		bt_nus->ctrl_blk->conn = NULL;
	}

	return 0;
}

static int read(const struct shell_transport *transport,
		void *data, size_t length, size_t *cnt)
{
	const struct shell_bt_nus *bt_nus =
			(struct shell_bt_nus *)transport->ctx;

	*cnt = ring_buf_get(bt_nus->rx_ringbuf, data, length);

	return 0;
}

static int write(const struct shell_transport *transport,
		 const void *data, size_t length, size_t *cnt)
{
	const struct shell_bt_nus *bt_nus =
			(struct shell_bt_nus *)transport->ctx;

	if (bt_nus->ctrl_blk->conn == NULL) {
		*cnt = length;
		return 0;
	}

	*cnt = ring_buf_put(bt_nus->tx_ringbuf, data, length);
	LOG_DBG("Write req:%d accept:%d", length, *cnt);

	if (atomic_set(&bt_nus->ctrl_blk->tx_busy, 1) == 0) {
		tx_try(bt_nus);
	}

	return 0;
}

void shell_bt_nus_disable(void)
{
	const struct shell_bt_nus *bt_nus =
			(const struct shell_bt_nus *)shell_transport_bt_nus.ctx;

	bt_nus->ctrl_blk->conn = NULL;
}

void shell_bt_nus_enable(struct bt_conn *conn)
{
	const struct shell_bt_nus *bt_nus =
			(const struct shell_bt_nus *)shell_transport_bt_nus.ctx;

	bt_nus->ctrl_blk->conn = conn;

	if (!is_init) {
		(void)shell_init(&shell_bt_nus, NULL,
				true, true, LOG_LEVEL_ERR);
		is_init = true;
	}
}

const struct shell_transport_api shell_bt_nus_transport_api = {
	.init = init,
	.uninit = uninit,
	.enable = enable,
	.write = write,
	.read = read,
};

int shell_bt_nus_init(void)
{
	struct bt_gatt_nus_cb callbacks = {
		.received_cb = rx_callback,
		.sent_cb = tx_callback
	};

	return bt_gatt_nus_init(&callbacks);
}
