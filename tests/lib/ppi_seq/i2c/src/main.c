/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <ppi_seq/ppi_seq.h>
#include <ppi_seq/ppi_seq_i2c_spi.h>
#include <zephyr/drivers/pinctrl.h>
#include <nrfx_twis.h>
#include <nrfx_timer.h>
#include <helpers/nrfx_gppi.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio/gpio_nrf.h>
LOG_MODULE_REGISTER(test);

/* Test is limited only to a basic scenario as ppi_seq functionality is verified by the SPI test. */
#define TWIM_NODE DT_NODELABEL(dut_twim)
#define TWIS_NODE DT_NODELABEL(dut_twis)
#define TIMER_NODE DT_NODELABEL(test_timer)

#define XFER_TX_LEN 4
#define XFER_RX_LEN 8
#define XFER_CNT 16
#define TWI_ADDR 0x3f

struct twis_test_data {
	nrfx_gppi_handle_t gppi_handle;
	nrfx_timer_t test_timer;
	uint32_t timestamp;
	uint32_t exp_time;
	uint32_t tx_cnt;
	uint32_t rx_cnt;
	uint32_t rx_len;
	uint32_t tx_len;
	nrfx_twis_t twis;
	bool err;
	uint8_t rx_buf[XFER_RX_LEN];
	uint8_t tx_buf[XFER_TX_LEN];
};

struct twim_test_data {
	nrfx_twim_t twim;
	struct ppi_seq_i2c_spi seq;
	uint8_t tx_buf[XFER_CNT][XFER_TX_LEN];
	uint8_t rx_buf[XFER_CNT][XFER_RX_LEN];
	uint32_t cb_cnt;
	uint32_t tx_cnt;
	uint32_t rx_cnt;
	uint32_t rpt;
	uint32_t t_cb;
	bool err;
	struct k_sem end_sem;
};

static struct twis_test_data twis_data;
static struct twim_test_data twim_data;

static void test_timer_init(void)
{
	nrfx_timer_config_t timer_config = {
		.frequency = MHZ(1),
		.mode = NRF_TIMER_MODE_TIMER,
		.bit_width = NRF_TIMER_BIT_WIDTH_32,
	};
	int rv;

	twis_data.test_timer.p_reg = (NRF_TIMER_Type *)DT_REG_ADDR(TIMER_NODE);

	rv = nrfx_timer_init(&twis_data.test_timer, &timer_config, NULL);
	zassert_ok(rv);

	nrfx_timer_enable(&twis_data.test_timer);

	uint32_t task = nrfx_timer_task_address_get(&twis_data.test_timer, NRF_TIMER_TASK_CAPTURE0);
	uint32_t event = nrf_twis_event_address_get(twis_data.twis.p_reg, NRF_TWIS_EVENT_WRITE);

	rv = nrfx_gppi_conn_alloc(event, task, &twis_data.gppi_handle);
	zassert_ok(rv);

	nrfx_gppi_conn_enable(twis_data.gppi_handle);
}

static void test_timer_uninit(void)
{
	uint32_t task = nrfx_timer_task_address_get(&twis_data.test_timer, NRF_TIMER_TASK_CAPTURE0);
	uint32_t event = nrf_twis_event_address_get(twis_data.twis.p_reg, NRF_TWIS_EVENT_WRITE);

	nrfx_gppi_conn_disable(twis_data.gppi_handle);
	nrfx_gppi_conn_free(event, task, twis_data.gppi_handle);
	nrfx_timer_uninit(&twis_data.test_timer);
}

static void prepare_buf(uint8_t *buf, size_t cnt, uint32_t init_val)
{
	for (int i = 0; i < cnt; i++) {
		buf[i] = (uint8_t)(init_val + i);
	}
}

static bool check_buf(uint8_t *buf, size_t cnt, uint8_t exp_val, int line)
{
	for (size_t i = 0; i < cnt; i++) {
		if (buf[i] != exp_val) {
			zassert_equal(buf[i], exp_val,
				"%d: Unexpected buffer content at %d, exp:%02x got:%02x",
				line, i, exp_val, buf[i]);
			return false;
		}
		exp_val++;
	}

	return true;
}

static void i2c_slave_handler(nrfx_twis_event_t const *event)
{
	switch (event->type) {
	case NRFX_TWIS_EVT_READ_REQ:
		LOG_DBG("TWIS event: read request");
		prepare_buf(twis_data.tx_buf, XFER_RX_LEN, twis_data.tx_cnt);
		nrfx_twis_tx_prepare(&twis_data.twis, twis_data.tx_buf, XFER_RX_LEN);
		break;
	case NRFX_TWIS_EVT_READ_DONE:
		LOG_DBG("TWIS event: read done: %d", event->data.tx_amount);
		twis_data.tx_cnt += event->data.tx_amount;
		zassert_equal(XFER_RX_LEN, event->data.tx_amount);
		break;
	case NRFX_TWIS_EVT_WRITE_REQ:
	{
		nrfx_twis_rx_prepare(&twis_data.twis, twis_data.rx_buf, XFER_TX_LEN);

		uint32_t capt = nrfx_timer_capture_get(&twis_data.test_timer,
							NRF_TIMER_CC_CHANNEL0);

		if (twis_data.timestamp != 0) {
			uint32_t diff = capt - twis_data.timestamp;

			zassert_within(diff, twis_data.exp_time, twis_data.exp_time / 10,
			"Time between TWIS ops %d us not within:%d with delta:%d",
			diff, twis_data.exp_time, twis_data.exp_time / 10);
		}
		twis_data.timestamp = capt;

		LOG_DBG("TWIS event: write request");
		break;
	}
	case NRFX_TWIS_EVT_WRITE_DONE:
		if (twis_data.err == false) {
			twis_data.err = check_buf(twis_data.rx_buf, event->data.rx_amount,
					(uint8_t)twis_data.rx_cnt, __LINE__);
		}
		LOG_DBG("TWIS event: write done: %d", event->data.rx_amount);
		twis_data.rx_cnt += event->data.rx_amount;
		zassert_equal(XFER_TX_LEN, event->data.rx_amount);
		break;
	default:
		LOG_ERR("TWIS unexpected event: %d", event->type);
		break;
	}
}

static void twim_event_handler(nrfx_twim_event_t const *event, void *context)
{
	TC_PRINT("Unexpected TWIM interrupt handler call. %d", event->type);
	zassert_false(true);
}

static void twim_init(void)
{
	int rv;
	nrfx_twim_config_t config = {
		.frequency = NRF_TWIM_FREQ_400K,
		.skip_gpio_cfg = true,
		.skip_psel_cfg = true
	};

	IRQ_CONNECT(DT_IRQN(TWIM_NODE), DT_IRQ_BY_IDX(TWIM_NODE, 0, priority),
			nrfx_twim_irq_handler, &twim_data.twim, 0);

	twim_data.twim.p_twim = (NRF_TWIM_Type *)DT_REG_ADDR(TWIM_NODE);

	PINCTRL_DT_DEFINE(TWIM_NODE);
	rv = pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(TWIM_NODE), PINCTRL_STATE_DEFAULT);
	zassert_ok(rv, "Unexpected error:%d", rv);

	rv = nrfx_twim_init(&twim_data.twim, &config, twim_event_handler, NULL);
	zassert_ok(rv, "Unexpected error:%d", rv);

	nrfx_twim_enable(&twim_data.twim);
}

static void twis_init(void)
{
	int rv;
	const nrfx_twis_config_t config = {
		.addr = {TWI_ADDR, 0},
		.skip_gpio_cfg = true,
		.skip_psel_cfg = true,
	};

	twis_data.twis.p_reg = (NRF_TWIS_Type *)DT_REG_ADDR(TWIS_NODE);

	IRQ_CONNECT(DT_IRQN(TWIS_NODE), DT_IRQ_BY_IDX(TWIS_NODE, 0, priority),
			nrfx_twis_irq_handler, &twis_data.twis, 0);

	PINCTRL_DT_DEFINE(TWIS_NODE);
	rv = pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(TWIS_NODE), PINCTRL_STATE_DEFAULT);
	zassert_ok(rv, "Unexpected error:%d", rv);

	rv = nrfx_twis_init(&twis_data.twis, &config, i2c_slave_handler);
	zassert_ok(rv, "Unexpected error:%d", rv);

	nrfx_twis_enable(&twis_data.twis);
}

static void ppi_seq_twim_cb(struct ppi_seq_i2c_spi *seq, struct ppi_seq_i2c_spi_data *data,
		bool last)
{
	uint32_t xfers_cnt = data->batch_cnt * (data->extra_xfers + 1);
	uint32_t tx_bytes = xfers_cnt * data->desc.twim.primary_length;
	uint32_t rx_bytes = xfers_cnt * data->desc.twim.secondary_length;

	twim_data.t_cb = k_cycle_get_32();
	twim_data.cb_cnt++;

	if (last) {
		k_sem_give((struct k_sem *)&twim_data.end_sem);
	}

	if (twim_data.rpt > 0) {
		twim_data.rpt--;
		if (twim_data.rpt == 0) {
			ppi_seq_i2c_spi_stop(seq, true);
		}
	}

	if (twim_data.err == false) {
		twim_data.err = check_buf(data->desc.twim.p_secondary_buf, rx_bytes,
					(uint8_t)twim_data.rx_cnt, __LINE__);
		twim_data.rx_cnt += rx_bytes;
	}

	/* Fill data for the next buffer. */
	prepare_buf(data->desc.twim.p_primary_buf, tx_bytes, twim_data.tx_cnt);
	twim_data.tx_cnt += tx_bytes;
}

static void twim_seq_init(enum ppi_seq_notifier_type notifier_type)
{
	static struct ppi_seq_config config;
	static struct ppi_seq_notifier notifier;
	union ppi_seq_i2c_spi_dev dev;
	uint32_t task = nrfx_twim_start_task_address_get(&twim_data.twim, NRFX_TWIM_XFER_TXRX);
	int rv;

	IRQ_CONNECT(DT_IRQN(DT_NODELABEL(dut_cnt_timer)),
		    DT_IRQ_BY_IDX(DT_NODELABEL(dut_cnt_timer), 0, priority),
			nrfx_timer_irq_handler, &notifier.nrfx_timer.timer, 0);

	dev.twim = &twim_data.twim;

	twim_data.cb_cnt = 0;
	twim_data.tx_cnt = 0;
	twim_data.rx_cnt = 0;

	if (notifier_type == PPI_SEQ_NOTIFIER_SYS_TIMER) {
		notifier.type = PPI_SEQ_NOTIFIER_SYS_TIMER;
		notifier.sys_timer.offset =
			(1000000 * (XFER_TX_LEN + XFER_RX_LEN)) / (KHZ(400) / 8) + 10;
	} else {
		notifier.type = PPI_SEQ_NOTIFIER_NRFX_TIMER;
		notifier.nrfx_timer.timer.p_reg =
			(NRF_TIMER_Type *)DT_REG_ADDR(DT_NODELABEL(dut_cnt_timer));
		notifier.nrfx_timer.end_seq_event =
				nrfx_twim_stopped_event_address_get(&twim_data.twim);
		notifier.nrfx_timer.extra_main_ops = 0;
	}

	k_sem_init(&twim_data.end_sem, 0, 1);

	config.extra_ops = NULL;
	config.extra_ops_count = 0;
	config.use_grtc = true;
	config.timer_reg = NULL;
	config.rtc = NULL;
	config.notifier = &notifier;
	config.callback = ppi_seq_i2c_spi_internal_cb;
	config.task = task;

	rv = ppi_seq_i2c_spi_init(&twim_data.seq, &config, dev, false);
	zassert_ok(rv, "Unexpected error:%d", rv);
}

static uint32_t single_run(struct ppi_seq_i2c_spi_job *job, uint32_t period, uint32_t timeout)
{
	int rv;
	uint32_t t;
	uint32_t prev_rx_cnt = twim_data.rx_cnt;
	uint32_t prev_cb_cnt = twim_data.cb_cnt;
	uint32_t prev_twis_tx_cnt = twis_data.tx_cnt;
	uint32_t exp_rx_bytes = XFER_CNT * XFER_RX_LEN;

	t = k_cycle_get_32();
	rv = ppi_seq_i2c_spi_start(&twim_data.seq, period, job);
	zassert_ok(rv, "Unexpected error:%d", rv);

	rv = k_sem_take(&twim_data.end_sem, K_USEC(timeout));
	zassert_ok(rv, "Unexpected error:%d", rv);

	zassert_equal(twim_data.rx_cnt - prev_rx_cnt, exp_rx_bytes,
		"Unexpected amount of bytes received: %d, exp:%d",
		twim_data.rx_cnt - prev_rx_cnt, exp_rx_bytes);
	zassert_equal(twim_data.cb_cnt - prev_cb_cnt, 1);
	zassert_equal(twis_data.tx_cnt - prev_twis_tx_cnt, exp_rx_bytes);

	return twim_data.t_cb - t;
}

static void test_single_run(enum ppi_seq_notifier_type notifier)
{
	struct ppi_seq_i2c_spi_job job = {
		.desc = {
			.twim = {
				.type = NRFX_TWIM_XFER_TXRX,
				.address = TWI_ADDR,
				.primary_length = XFER_TX_LEN,
				.secondary_length = XFER_RX_LEN,
				.p_primary_buf = twim_data.tx_buf[0],
				.p_secondary_buf = twim_data.rx_buf[0],
			}
		},
		.batch_cnt = XFER_CNT,
		.repeat = 1,
		.extra_xfers = 0,
		.tx_postinc = true,
		.cb = ppi_seq_twim_cb
	};
	uint32_t rpt = XFER_CNT;
	uint32_t period = 2000;
	uint32_t t;
	uint32_t exp_time = (rpt - 1) * period + period / 2;

	twis_data.exp_time = period;
	twim_seq_init(notifier);

	prepare_buf(twim_data.tx_buf[0], XFER_TX_LEN * XFER_CNT, twim_data.tx_cnt);
	twim_data.tx_cnt = XFER_TX_LEN * XFER_CNT;

	t = single_run(&job, period, exp_time + 1000);

	if (notifier == PPI_SEQ_NOTIFIER_SYS_TIMER) {
		/* Due to 1 tick accuracy and processing of k_timer delay is increased. */
		exp_time += 50;
	}

	zassert_within(t, exp_time, period / 2,
			"Time %d us not within:%d with delta:%d",
			t, exp_time, period / 2);
}

ZTEST(ppi_seq_twim, test_single_run_with_grtc_interval_and_sys_timer)
{
	test_single_run(PPI_SEQ_NOTIFIER_SYS_TIMER);
}

static void before(void *not_used)
{
	ARG_UNUSED(not_used);

	memset(&twis_data, 0, sizeof(twis_data));
	memset(&twim_data, 0, sizeof(twim_data));

	twim_init();
	twis_init();
	test_timer_init();
}

static void after(void *not_used)
{
	ARG_UNUSED(not_used);

	test_timer_uninit();
	nrfx_twis_uninit(&twis_data.twis);
	nrfx_twim_uninit(&twim_data.twim);
	ppi_seq_i2c_spi_uninit(&twim_data.seq);
}

ZTEST_SUITE(ppi_seq_twim, NULL, NULL, before, after, NULL);
