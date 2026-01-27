/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <debug/cpu_load.h>
#include <ppi_seq/ppi_seq.h>
#include <ppi_seq/ppi_seq_i2c_spi.h>
#include <zephyr/drivers/pinctrl.h>
#include <nrfx_spim.h>
#include <nrfx_spis.h>
#include <nrfx_timer.h>
#include <nrfx_grtc.h>
#include <nrfx_gpiote.h>
#include <hal/nrf_gpio.h>
#include <debug/ppi_trace.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio/gpio_nrf.h>
LOG_MODULE_REGISTER(test);

#define SPIM_NODE DT_NODELABEL(dut_spi)
#define SPIS_NODE DT_NODELABEL(dut_spis)
#define TIMER_NODE DT_NODELABEL(test_timer)

#define XFER_LEN 8
#define XFER_CNT 16
#define SEQ_PERIOD 250
#define SEQ_PERIOD_RTC 306
#define SEQ_OFFSET 62

enum test_ppi_seq_mode {
	TEST_MODE_RTC,
	TEST_MODE_TIMER,
	TEST_MODE_GRTC_TIMER,
	TEST_MODE_GRTC,
};

enum test_ppi_seq_op {
	TEST_OP_SINGLE_HW_SS,
	TEST_OP_SINGLE_GPIOTE_SS,
	TEST_OP_DOUBLE,
};

struct spis_test_data {
	nrfx_spis_t spis;
	nrfx_timer_t test_timer;
	nrfx_gppi_handle_t gppi_handle;
	uint32_t cb_cnt;
	uint32_t rx_cnt;
	uint32_t tx_cnt;
	uint32_t exp_len;
	volatile bool stop;
	bool err;
	uint8_t rx_buf[XFER_LEN];
	uint8_t tx_buf[XFER_LEN];
	uint32_t last_timestamp;
	uint32_t timestamp_idx;
	uint32_t timestamp_max;
	uint32_t delta;
	uint32_t max_delta;
	uint32_t exp_diff[2];
};

struct spim_test_data {
	nrfx_spim_t spim;
	struct ppi_seq_i2c_spi seq;
	nrfx_gpiote_t *gpiote;
	uint8_t gpiote_task_ch;
	uint32_t ss_pin;
	uint32_t ss_task;

	uint8_t tx_buf0[XFER_CNT][XFER_LEN];
	uint8_t rx_buf0[XFER_CNT][XFER_LEN];

	uint8_t tx_buf1[XFER_CNT][XFER_LEN];
	uint8_t rx_buf1[XFER_CNT][XFER_LEN];

	uint32_t exp_len;
	uint32_t cb_cnt;
	uint32_t tx_cnt;
	uint32_t rx_cnt;
	uint32_t rpt;
	bool stop_immediate;
	volatile bool stop;
	uint32_t t_cb;
	bool err;
	struct k_sem end_sem;
};

static struct spis_test_data spis_data;
static struct spim_test_data spim_data;

static NRF_TIMER_Type *timer = (NRF_TIMER_Type *)DT_REG_ADDR(DT_NODELABEL(dut_trig_timer));

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
			if (buf[i] != exp_val) {
				NRF_P2->OUTSET=BIT(10);
				k_busy_wait(1);
				NRF_P2->OUTCLR=BIT(10);
			}

			zassert_equal(buf[i], exp_val,
				"%d: Unexpected buffer content at %d, exp:%02x got:%02x",
				line, i, exp_val, buf[i]);
			return false;
		}
		exp_val++;
	}

	return true;
}

static void next_spis(void)
{
	int rv;

	prepare_buf(spis_data.tx_buf, spis_data.exp_len, spis_data.tx_cnt);
	spis_data.tx_cnt += spis_data.exp_len;

	rv = nrfx_spis_buffers_set(&spis_data.spis, spis_data.tx_buf, spis_data.exp_len,
				   spis_data.rx_buf, spis_data.exp_len);
	zassert_ok(rv, "Unexpected error:%d", rv);
}

static void spis_check_time(void)
{
	uint32_t capt = nrfx_timer_capture_get(&spis_data.test_timer, NRF_TIMER_CC_CHANNEL0);

	if (spis_data.cb_cnt > 4) {
		uint32_t distance = capt - spis_data.last_timestamp;
		int delta = spis_data.exp_diff[spis_data.timestamp_idx] - distance;

		delta = delta < 0 ? -delta : delta;
		if (delta > spis_data.delta) {
			NRF_P2->OUTSET=BIT(10);
			k_busy_wait(1);
			NRF_P2->OUTCLR=BIT(10);
			if (spis_data.cb_cnt == 9) printk("A\n");
		}

		spis_data.max_delta = MAX(delta, spis_data.max_delta);
		zassert_true(delta <= spis_data.delta,
				"Unexpected timing for the SPIS event: "
				"%d us after last (expected:%d, delta:%d) rx_cnt:%d last_t:%d",
				distance, spis_data.exp_diff[spis_data.timestamp_idx],
				spis_data.delta, spis_data.rx_cnt, spis_data.last_timestamp);
	}

	spis_data.timestamp_idx++;
	if (spis_data.timestamp_idx == spis_data.timestamp_max) {
		spis_data.timestamp_idx = 0;
	}
	spis_data.last_timestamp = capt;
}

static void spis_event_handler(nrfx_spis_event_t const *event, void *context)
{
	if ((event->evt_type == NRFX_SPIS_XFER_DONE) && (spis_data.stop == false)) {
		zassert_equal(spis_data.exp_len, event->rx_amount);
		zassert_equal(spis_data.exp_len, event->tx_amount);

		spis_check_time();

		NRF_P2->OUTSET=BIT(9);
		if (spis_data.err == false) {
			spis_data.err = check_buf(spis_data.rx_buf, spis_data.exp_len,
					spis_data.rx_cnt, __LINE__);
		}
		NRF_P2->OUTCLR=BIT(9);
		spis_data.rx_cnt += spis_data.exp_len;
		spis_data.cb_cnt++;
		next_spis();
	}
}

static void spim_event_handler(nrfx_spim_event_t const *event, void *context)
{
	TC_PRINT("Unexpected SPIM interrupt handler call.");
	zassert_false(true);
}

static void spim_init(bool use_hw_ss)
{
	int rv;
	nrfx_spim_config_t config;

	IRQ_CONNECT(DT_IRQN(SPIM_NODE), DT_IRQ_BY_IDX(SPIM_NODE, 0, priority),
			nrfx_spim_irq_handler, &spim_data.spim, 0);

	spim_data.spim.p_reg = (NRF_SPIM_Type *)DT_REG_ADDR(SPIM_NODE);

	PINCTRL_DT_DEFINE(SPIM_NODE);
	rv = pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(SPIM_NODE), PINCTRL_STATE_DEFAULT);
	zassert_ok(rv, "Unexpected error:%d", rv);

	uint32_t pin = DT_GPIO_PIN(SPIM_NODE, cs_gpios);
	uint32_t port = DT_PROP(DT_GPIO_CTLR(SPIM_NODE, cs_gpios), port);
	uint32_t ss_pin = 32 * port + pin;

	/* CS configure. */
	if (use_hw_ss) {
		nrf_gpio_pin_set(ss_pin);
		nrf_gpio_cfg_output(ss_pin);
		nrf_spim_csn_configure(spim_data.spim.p_reg, ss_pin, NRF_SPIM_CSN_POL_LOW, 32);
		spim_data.ss_task = 0;
	} else {
		const struct device *cs_port = DEVICE_DT_GET(DT_GPIO_CTLR(SPIM_NODE, cs_gpios));
		nrfx_gpiote_t *gpiote = gpio_nrf_gpiote_by_port_get(cs_port);
		nrfx_gpiote_output_config_t out_config = {
			.drive = NRF_GPIO_PIN_S0S1,
			.input_connect = NRF_GPIO_PIN_INPUT_DISCONNECT,
			.pull = NRF_GPIO_PIN_NOPULL
		};
		nrfx_gpiote_task_config_t task_config = {
			.polarity = NRF_GPIOTE_POLARITY_TOGGLE,
			.init_val = NRF_GPIOTE_INITIAL_VALUE_HIGH
		};

		rv = nrfx_gpiote_channel_alloc(gpiote, &spim_data.gpiote_task_ch);
		zassert_ok(rv, "Unexpected error:%d", rv);

		task_config.task_ch = spim_data.gpiote_task_ch;

		rv = nrfx_gpiote_output_configure(gpiote, ss_pin, &out_config, &task_config);
		zassert_ok(rv, "Unexpected error:%d", rv);
		nrfx_gpiote_out_task_enable(gpiote, ss_pin);

		spim_data.ss_task = nrfx_gpiote_out_task_address_get(gpiote, ss_pin);
		spim_data.ss_pin = ss_pin;
		spim_data.gpiote = gpiote;
	}

	config.ss_pin = NRF_SPIM_PIN_NOT_CONNECTED;
	config.frequency = MHZ(8);
	config.mode = NRF_SPIM_MODE_0;
	config.use_hw_ss = use_hw_ss;
	config.rx_delay = 1;
	config.bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST;
	config.skip_gpio_cfg = true;
	config.skip_psel_cfg = true;

	rv = nrfx_spim_init(&spim_data.spim, &config, spim_event_handler, NULL);
	zassert_ok(rv, "Unexpected error:%d", rv);
}

static void test_timer_init(uint32_t event)
{
	nrfx_timer_config_t timer_config = {
		.frequency = MHZ(1),
		.mode = NRF_TIMER_MODE_TIMER,
		.bit_width = NRF_TIMER_BIT_WIDTH_32,
	};
	int rv;
	uint32_t task;

	spis_data.test_timer.p_reg = (NRF_TIMER_Type *)DT_REG_ADDR(TIMER_NODE);

	rv = nrfx_timer_init(&spis_data.test_timer, &timer_config, NULL);
	zassert_ok(rv);

	nrfx_timer_enable(&spis_data.test_timer);

	task = nrfx_timer_task_address_get(&spis_data.test_timer, NRF_TIMER_TASK_CAPTURE0);
	rv = nrfx_gppi_conn_alloc(event, task, &spis_data.gppi_handle);
	zassert_ok(rv);

	nrfx_gppi_conn_enable(spis_data.gppi_handle);
}

static void test_timer_uninit(uint32_t event)
{
	uint32_t task = nrfx_timer_task_address_get(&spis_data.test_timer, NRF_TIMER_TASK_CAPTURE0);

	nrfx_gppi_conn_disable(spis_data.gppi_handle);
	nrfx_gppi_conn_free(event, task, spis_data.gppi_handle);
	nrfx_timer_uninit(&spis_data.test_timer);
}

static void spis_init(void)
{
	int rv;
	nrfx_spis_config_t config = {
		.mode = NRF_SPIS_MODE_0,
		.bit_order = NRF_SPIS_BIT_ORDER_MSB_FIRST,
		.skip_gpio_cfg = true,
		.skip_psel_cfg = true,
	};
	uint32_t event;

	spis_data.spis.p_reg = (NRF_SPIS_Type *)DT_REG_ADDR(SPIS_NODE);

	PINCTRL_DT_DEFINE(SPIS_NODE);
	rv = pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(SPIS_NODE), PINCTRL_STATE_DEFAULT);
	zassert_ok(rv, "Unexpected error:%d", rv);

	rv = nrfx_spis_init(&spis_data.spis, &config, spis_event_handler, NULL);
	zassert_ok(rv, "Unexpected error:%d", rv);

	IRQ_CONNECT(DT_IRQN(SPIS_NODE), DT_IRQ_BY_IDX(SPIS_NODE, 0, priority),
			nrfx_spis_irq_handler, &spis_data.spis, 0);

	event = nrf_spis_event_address_get(spis_data.spis.p_reg, NRF_SPIS_EVENT_RXSTARTED);
	test_timer_init(event);
}

static void spis_uninit(void)
{
	uint32_t event;

	nrfx_spis_uninit(&spis_data.spis);

	event = nrf_spis_event_address_get(spis_data.spis.p_reg, NRF_SPIS_EVENT_RXSTARTED);
	test_timer_uninit(event);
}

static void ppi_seq_spim_cb(struct ppi_seq_i2c_spi *seq, struct ppi_seq_i2c_spi_data *data,
		bool last)
{
	NRF_P2->OUTSET=BIT(8);
	uint32_t xfers_cnt = data->batch_cnt * (data->extra_xfers + 1);
	uint32_t batch_bytes = xfers_cnt * data->desc.spim.tx_length;

	spim_data.t_cb = k_cycle_get_32();
	spim_data.cb_cnt++;

	if (last) {
		k_sem_give(&spim_data.end_sem);
	}

	if (spim_data.rpt > 0) {
		spim_data.rpt--;
		if (spim_data.rpt == 0) {
			ppi_seq_i2c_spi_stop(seq, spim_data.stop_immediate);
		}
	}

	if (spim_data.stop == false) {
		if (spim_data.err == false) {
			spim_data.err = check_buf(data->desc.spim.p_rx_buffer, batch_bytes,
						spim_data.rx_cnt, __LINE__);
		}
		spim_data.rx_cnt += batch_bytes;
	}

	/* Fill data for the next buffer. */
	prepare_buf((uint8_t *)data->desc.spim.p_tx_buffer, batch_bytes, spim_data.tx_cnt);
	spim_data.tx_cnt += batch_bytes;
	NRF_P2->OUTCLR=BIT(8);
}

static void seq_spim_init(enum test_ppi_seq_mode mode, enum test_ppi_seq_op op,
			enum ppi_seq_notifier_type notifier_type, uint32_t xfer_len)
{
	static struct ppi_seq_extra_op ops[2];
	static struct ppi_seq_config config;
	static struct ppi_seq_notifier notifier;
	union ppi_seq_i2c_spi_dev dev;
	uint32_t task;
	int rv;

	spim_init(op != TEST_OP_SINGLE_GPIOTE_SS);
	IRQ_CONNECT(DT_IRQN(DT_NODELABEL(dut_cnt_timer)),
		    DT_IRQ_BY_IDX(DT_NODELABEL(dut_cnt_timer), 0, priority),
			nrfx_timer_irq_handler, &notifier.nrfx_timer.timer, 0);

	dev.spim = &spim_data.spim;
	task = nrfx_spim_start_task_address_get(&spim_data.spim);
	spis_data.exp_len = xfer_len;
	spis_data.last_timestamp = 0;

	spim_data.cb_cnt = 0;
	spim_data.tx_cnt = 0;
	spim_data.rx_cnt = 0;

	memset(&config, 0, sizeof(config));
	if (op == TEST_OP_SINGLE_GPIOTE_SS) {
		config.task = spim_data.ss_task;
		ops[0].task = task,
		ops[0].offset = 2;
		ops[1].task = spim_data.ss_task;
		ops[1].offset = 14;
		config.extra_ops = ops;
		config.extra_ops_count = 2;
	} else if (op == TEST_OP_DOUBLE) {
		ops[0].task = task;
		ops[0].offset = SEQ_OFFSET;
		config.task = task;
		config.extra_ops = ops;
		config.extra_ops_count = 1;
	} else {
		config.task = task;
		config.extra_ops = NULL;
		config.extra_ops_count = 0;
	}

	switch (mode) {
#ifdef RTC_PRESENT
	case TEST_MODE_RTC:
		config.rtc = NRF_RTC10;
		break;
#endif
	case TEST_MODE_TIMER:
		config.timer_reg = timer;
		break;
	case TEST_MODE_GRTC_TIMER:
		config.use_grtc = true;
		config.timer_reg = timer;
		break;
	case TEST_MODE_GRTC:
		config.use_grtc = true;
		break;
	default:
		zassert_false(true);
		break;
	}

	/* Initialize the notifier. */
	if (notifier_type == PPI_SEQ_NOTIFIER_SYS_TIMER) {
		notifier.type = PPI_SEQ_NOTIFIER_SYS_TIMER;
		notifier.sys_timer.offset = (1000000 * xfer_len) / (MHZ(8) / 8) + 10;
	} else {
		notifier.type = PPI_SEQ_NOTIFIER_NRFX_TIMER;
		notifier.nrfx_timer.timer.p_reg =
			(NRF_TIMER_Type *)DT_REG_ADDR(DT_NODELABEL(dut_cnt_timer));
		notifier.nrfx_timer.end_seq_event =
				nrfx_spim_end_event_address_get(&spim_data.spim);
		notifier.nrfx_timer.extra_main_ops = (op == TEST_OP_DOUBLE) ? 1 : 0;
	}
	config.notifier = &notifier;
	config.callback = ppi_seq_i2c_spi_internal_cb;

	k_sem_init(&spim_data.end_sem, 0, 1);

	rv = ppi_seq_i2c_spi_init(&spim_data.seq, &config, dev, true);
	zassert_ok(rv, "Unexpected error:%d", rv);

	static bool once;
	if (once == false && config.use_grtc) {
		int chan = nrfx_gppi_ep_channel_get((uint32_t)&NRF_GRTC->EVENTS_COMPARE[0]);
		LOG_INF("grtc channel %d", chan);
		int err = ppi_trace_dppi_ch_trace(4, chan, NRF_DPPIC20);
		if (err < 0) {
			LOG_ERR("ppi trace failed");
		}
		once = true;

		void *handle = ppi_trace_pair_config(2,
				(uint32_t)&NRF_POWER->EVENTS_SLEEPENTER,
				(uint32_t)&NRF_POWER->EVENTS_SLEEPEXIT);
		/*void *handle = ppi_trace_config(2, (uint32_t)&spis_data.spis.p_reg->EVENTS_END);*/
		ppi_trace_enable(handle);
	}
}

static void spis_prepare(enum test_ppi_seq_op op, enum test_ppi_seq_mode mode)
{
	uint32_t period = (mode == TEST_MODE_RTC) ? SEQ_PERIOD_RTC : SEQ_PERIOD;

	spis_data.cb_cnt = 0;
	spis_data.rx_cnt = 0;
	spis_data.tx_cnt = 0;
	spis_data.last_timestamp = 0;
	if (op == TEST_OP_DOUBLE) {
		spis_data.exp_diff[0] = period - SEQ_OFFSET;
		spis_data.exp_diff[1] = SEQ_OFFSET;
		spis_data.timestamp_max = 2;
	} else {
		spis_data.exp_diff[0] = period;
		spis_data.timestamp_max = 1;
	}

	switch (mode) {
	case TEST_MODE_TIMER:
		spis_data.delta = 10;
		break;
	case TEST_MODE_GRTC_TIMER:
		spis_data.delta = 4;
		break;
	case TEST_MODE_RTC:
		spis_data.delta = 8;
		break;
	default:
		spis_data.delta = 4;
		break;
	}

	next_spis();
}

static uint32_t single_run(struct ppi_seq_i2c_spi_job *job, uint32_t period, uint32_t timeout)
{
	int rv;
	uint32_t t;
	uint32_t prev_byte_cnt = spim_data.rx_cnt;
	uint32_t prev_cb_cnt = spim_data.cb_cnt;
	uint32_t prev_spis_cnt = spis_data.rx_cnt;
	uint32_t exp_bytes = XFER_CNT * XFER_LEN;

	t = k_cycle_get_32();
	rv = ppi_seq_i2c_spi_start(&spim_data.seq, period, job);
	zassert_ok(rv, "Unexpected error:%d", rv);

	rv = k_sem_take(&spim_data.end_sem, K_USEC(timeout));
	zassert_ok(rv, "Unexpected error:%d", rv);

	zassert_equal(spim_data.rx_cnt - prev_byte_cnt, exp_bytes,
		"Unexpected amount of bytes received: %d, exp:%d",
		spim_data.rx_cnt - prev_byte_cnt, exp_bytes);
	zassert_equal(spim_data.cb_cnt - prev_cb_cnt, 1);
	zassert_equal(spis_data.rx_cnt - prev_spis_cnt, exp_bytes);

	return spim_data.t_cb - t;
}

static void test_single_run(enum test_ppi_seq_mode mode, enum test_ppi_seq_op op,
			enum ppi_seq_notifier_type notifier)
{
	struct ppi_seq_i2c_spi_job job = {
		.desc = {
			.spim = {
				.p_tx_buffer = spim_data.tx_buf0[0],
				.tx_length = XFER_LEN,
				.p_rx_buffer = spim_data.rx_buf0[0],
				.rx_length = XFER_LEN,
			}
		},
		.batch_cnt = (op == TEST_OP_DOUBLE) ? XFER_CNT / 2 : XFER_CNT,
		.repeat = 1,
		.extra_xfers = (op == TEST_OP_DOUBLE) ? 1 : 0,
		.tx_postinc = true,
		.cb = ppi_seq_spim_cb
	};
	uint32_t t;
	uint32_t period = (mode == TEST_MODE_RTC) ? SEQ_PERIOD_RTC : SEQ_PERIOD;
	uint32_t exp_time = (job.batch_cnt - 1) * period + period / 2;

	seq_spim_init(mode, op, notifier, XFER_LEN);

	prepare_buf(spim_data.tx_buf0[0], XFER_LEN * XFER_CNT, spim_data.tx_cnt);
	spim_data.tx_cnt += XFER_LEN * XFER_CNT;

	spis_prepare(op, mode);

	t = single_run(&job, period, exp_time + 1000);

	if (notifier == PPI_SEQ_NOTIFIER_SYS_TIMER) {
		/* Due to 1 tick accuracy and processing of k_timer delay is increased. */
		exp_time += 50;
	}

	printk("t:%d exp_time:%d\n", t , exp_time);
	zassert_within(t, exp_time, period / 2,
			"Time %d us not within:%d with delta:%d",
			t, exp_time, period / 2);
}

static void test_multi_run(enum test_ppi_seq_mode mode, enum test_ppi_seq_op op,
			enum ppi_seq_notifier_type notifier)
{
	uint32_t rpt = 100;
	struct ppi_seq_i2c_spi_job job = {
		.desc = {
			.spim = {
				.p_tx_buffer = spim_data.tx_buf0[0],
				.tx_length = XFER_LEN,
				.p_rx_buffer = spim_data.rx_buf0[0],
				.rx_length = XFER_LEN,
			}
		},
		.tx_second_buf = spim_data.tx_buf1[0],
		.rx_second_buf = spim_data.rx_buf1[0],
		.batch_cnt = (op == TEST_OP_DOUBLE) ? XFER_CNT / 2 : XFER_CNT,
		.extra_xfers = (op == TEST_OP_DOUBLE) ? 1 : 0,
		.repeat = rpt,
		.tx_postinc = true,
		.cb = ppi_seq_spim_cb
	};
	uint32_t batch_len = job.batch_cnt * job.desc.spim.tx_length * (1 + job.extra_xfers);
	uint32_t period = (mode == TEST_MODE_RTC) ? SEQ_PERIOD_RTC : SEQ_PERIOD;
	uint32_t exp_time = (rpt * job.batch_cnt - 1) * period + period / 2;
	uint32_t t;
	int rv;
	int dev;

	seq_spim_init(mode, op, notifier, XFER_LEN);

	prepare_buf(spim_data.tx_buf0[0], batch_len, 0);
	spim_data.tx_cnt += batch_len;
	prepare_buf(spim_data.tx_buf1[0], batch_len, spim_data.tx_cnt);
	spim_data.tx_cnt += batch_len;

	spis_prepare(op, mode);

	spim_data.rpt = rpt - 1;
	t = k_cycle_get_32();
	rv = ppi_seq_i2c_spi_start(&spim_data.seq, period, &job);
	zassert_ok(rv, "Unexpected error:%d", rv);

	rv = k_sem_take(&spim_data.end_sem, K_USEC((uint32_t)((11 * exp_time) / 10)));
	zassert_ok(rv, "Unexpected error:%d", rv);
	t = k_cycle_get_32() - t;

	dev = exp_time - t;
	dev = (dev < 0) ? -dev : dev;
	dev = (10000 * dev) / exp_time;
	printk("Expected time:%d us, took:%d us. Difference depends on the trigger source, "
		"deviation:%d.%d, max SPIS event delta:%d (accepted:%d)\n",
		exp_time, t, dev / 100, dev % 100, spis_data.max_delta, spis_data.delta);

	zassert_equal(spim_data.rx_cnt, rpt * batch_len);
	zassert_equal(spim_data.cb_cnt, rpt);
}

static void test_multi_run_async_stop(enum test_ppi_seq_mode mode, enum test_ppi_seq_op op,
			enum ppi_seq_notifier_type notifier, bool immediate)
{
	struct ppi_seq_i2c_spi_job job = {
		.desc = {
			.spim = {
				.p_tx_buffer = spim_data.tx_buf0[0],
				.tx_length = XFER_LEN,
				.p_rx_buffer = spim_data.rx_buf0[0],
				.rx_length = XFER_LEN,
			}
		},
		.tx_second_buf = spim_data.tx_buf1[0],
		.rx_second_buf = spim_data.rx_buf1[0],
		.batch_cnt = (op == TEST_OP_DOUBLE) ? XFER_CNT / 2 : XFER_CNT,
		.extra_xfers = (op == TEST_OP_DOUBLE) ? 1 : 0,
		.repeat = UINT32_MAX,
		.tx_postinc = true,
		.cb = ppi_seq_spim_cb
	};
	uint32_t batch_len = job.batch_cnt * job.desc.spim.tx_length * (1 + job.extra_xfers);
	uint32_t period = (mode == TEST_MODE_RTC) ? SEQ_PERIOD_RTC : SEQ_PERIOD;
	uint32_t timeout = job.batch_cnt * period;
	int rv;

	seq_spim_init(mode, op, notifier, XFER_LEN);

	for (int i = 0; i < 3; i++) {
		spis_data.stop = false;
		spim_data.stop = false;

		spim_data.tx_cnt = 0;
		spim_data.rx_cnt = 0;

		spis_prepare(op, mode);

		prepare_buf(spim_data.tx_buf0[0], batch_len, spim_data.tx_cnt);
		spim_data.tx_cnt += batch_len;
		prepare_buf(spim_data.tx_buf1[0], batch_len, spim_data.tx_cnt);
		spim_data.tx_cnt += batch_len;

		rv = ppi_seq_i2c_spi_start(&spim_data.seq, period, &job);
		zassert_ok(rv, "Unexpected error:%d", rv);

		k_msleep(50);

		uint32_t cb_cnt = spim_data.cb_cnt;

		spis_data.stop = true;
		spim_data.stop = true;
		rv = ppi_seq_i2c_spi_stop(&spim_data.seq, immediate);
		zassert_ok(rv, "Unexpected error:%d", rv);
		if (immediate) {
			/* Stop event is NOT called. */
			rv = k_sem_take(&spim_data.end_sem, K_USEC(timeout));
			zassert_equal(rv, -EAGAIN);

			/* No more callbacks. */
			zassert_equal(cb_cnt, spim_data.cb_cnt);
		} else {
			/* Stop event is called. */
			rv = k_sem_take(&spim_data.end_sem, K_USEC(timeout));
			zassert_ok(rv, "Unexpected error:%d", rv);

			/* One more callback. */
			zassert_equal(cb_cnt + 1, spim_data.cb_cnt);
		}

		k_msleep(1);
	}
}

ZTEST(ppi_seq_spi, test_single_run_with_grtc_interval_and_sys_timer)
{
	test_single_run(TEST_MODE_GRTC, TEST_OP_SINGLE_HW_SS, PPI_SEQ_NOTIFIER_SYS_TIMER);
}

ZTEST(ppi_seq_spi, test_single_run_with_grtc_interval_and_cnt_timer)
{
	test_single_run(TEST_MODE_GRTC, TEST_OP_SINGLE_HW_SS, PPI_SEQ_NOTIFIER_NRFX_TIMER);
}

ZTEST(ppi_seq_spi, test_single_run_with_cnt_timer_gpiote_ss)
{
	test_single_run(TEST_MODE_GRTC_TIMER, TEST_OP_SINGLE_GPIOTE_SS, PPI_SEQ_NOTIFIER_NRFX_TIMER);
}

ZTEST(ppi_seq_spi, test_single_run_with_sys_timer_gpiote_ss)
{
	test_single_run(TEST_MODE_GRTC_TIMER, TEST_OP_SINGLE_GPIOTE_SS, PPI_SEQ_NOTIFIER_SYS_TIMER);
}

ZTEST(ppi_seq_spi, test_single_run_with_trig_timer_and_sys_timer_two_ops)
{
	test_single_run(TEST_MODE_GRTC_TIMER, TEST_OP_DOUBLE, PPI_SEQ_NOTIFIER_SYS_TIMER);
}

ZTEST(ppi_seq_spi, test_single_run_with_trig_timer_and_cnt_timer_two_ops)
{
	test_single_run(TEST_MODE_GRTC_TIMER, TEST_OP_DOUBLE, PPI_SEQ_NOTIFIER_NRFX_TIMER);
}

ZTEST(ppi_seq_spi, test_single_run_with_trig_timer_and_cnt_timer)
{
	test_single_run(TEST_MODE_TIMER, TEST_OP_SINGLE_HW_SS, PPI_SEQ_NOTIFIER_NRFX_TIMER);
}

ZTEST(ppi_seq_spi, test_multi_run_with_trig_timer_and_cnt_timer)
{
	test_multi_run(TEST_MODE_TIMER, TEST_OP_SINGLE_HW_SS, PPI_SEQ_NOTIFIER_NRFX_TIMER);
}

ZTEST(ppi_seq_spi, test_multi_run_with_trig_timer_and_cnt_timer_two_ops)
{
	test_multi_run(TEST_MODE_GRTC_TIMER, TEST_OP_DOUBLE, PPI_SEQ_NOTIFIER_NRFX_TIMER);
}

ZTEST(ppi_seq_spi, test_multi_run_with_trig_timer_and_sys_timer_two_ops)
{
	test_multi_run(TEST_MODE_GRTC_TIMER, TEST_OP_DOUBLE, PPI_SEQ_NOTIFIER_SYS_TIMER);
}

ZTEST(ppi_seq_spi, test_multi_run_with_grtc_and_cnt_timer)
{
	test_multi_run(TEST_MODE_GRTC, TEST_OP_SINGLE_HW_SS, PPI_SEQ_NOTIFIER_NRFX_TIMER);
}

ZTEST(ppi_seq_spi, test_multi_run_with_grtc_and_sys_timer)
{
	test_multi_run(TEST_MODE_GRTC, TEST_OP_SINGLE_HW_SS, PPI_SEQ_NOTIFIER_SYS_TIMER);
}

ZTEST(ppi_seq_spi, test_multi_run_async_stop_with_grtc_and_sys_timer)
{
	test_multi_run_async_stop(TEST_MODE_GRTC, TEST_OP_SINGLE_HW_SS,
				  PPI_SEQ_NOTIFIER_SYS_TIMER, false);
}

ZTEST(ppi_seq_spi, test_multi_run_immediate_async_stop_with_grtc_and_sys_timer)
{
	test_multi_run_async_stop(TEST_MODE_GRTC, TEST_OP_SINGLE_HW_SS,
				  PPI_SEQ_NOTIFIER_SYS_TIMER, true);
}

#ifdef RTC_PRESENT
#include <hal/nrf_rtc.h>
static void rtc_prepare(void)
{
	/* The first time RTC is running it can start with a delay which impacts the test. */
	nrf_rtc_task_trigger(NRF_RTC10, NRF_RTC_TASK_START);
	k_msleep(1);
	nrf_rtc_task_trigger(NRF_RTC10, NRF_RTC_TASK_STOP);
	nrf_rtc_task_trigger(NRF_RTC10, NRF_RTC_TASK_CLEAR);
}

ZTEST(ppi_seq_spi, test_single_run_with_rtc_and_cnt_timer)
{
	rtc_prepare();
	test_single_run(TEST_MODE_RTC, TEST_OP_SINGLE_HW_SS, PPI_SEQ_NOTIFIER_NRFX_TIMER);
}

ZTEST(ppi_seq_spi, test_single_run_with_rtc_and_sys_timer)
{
	rtc_prepare();
	test_single_run(TEST_MODE_RTC, TEST_OP_SINGLE_HW_SS, PPI_SEQ_NOTIFIER_SYS_TIMER);
}

ZTEST(ppi_seq_spi, test_multi_run_with_rtc_and_cnt_timer)
{
	rtc_prepare();
	test_multi_run(TEST_MODE_RTC, TEST_OP_SINGLE_HW_SS, PPI_SEQ_NOTIFIER_NRFX_TIMER);
}


ZTEST(ppi_seq_spi, test_multi_run_with_rtc_and_sys_timer)
{
	rtc_prepare();
	test_multi_run(TEST_MODE_RTC, TEST_OP_SINGLE_HW_SS, PPI_SEQ_NOTIFIER_SYS_TIMER);
}
#endif

static void before(void *not_used)
{
	ARG_UNUSED(not_used);

	memset(&spis_data, 0, sizeof(spis_data));
	memset(&spim_data, 0, sizeof(spim_data));

	spis_init();
}

static void after(void *not_used)
{
	ARG_UNUSED(not_used);

	spis_uninit();
	if (spim_data.ss_task) {
		nrfx_gpiote_pin_uninit(spim_data.gpiote, spim_data.ss_pin);
		nrfx_gpiote_channel_free(spim_data.gpiote, spim_data.gpiote_task_ch);
	}
	nrfx_spim_uninit(&spim_data.spim);
	ppi_seq_i2c_spi_uninit(&spim_data.seq);
}

#include <hal/nrf_gpio.h>
static void *suite_setup(void)
{
	nrf_gpio_cfg_output(2*32+8);
	nrf_gpio_cfg_output(2*32+9);
	nrf_gpio_cfg_output(2*32+10);
	return NULL;
}

ZTEST_SUITE(ppi_seq_spi, NULL, suite_setup, before, after, NULL);
