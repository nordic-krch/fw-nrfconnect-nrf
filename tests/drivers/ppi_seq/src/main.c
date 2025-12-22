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
#include <drivers/ppi_seq.h>
#include <drivers/ppi_seq_spim.h>
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
static nrfx_spim_t spim;
static nrfx_spis_t spis;

#define XFER_LEN 8
#define XFER_CNT 16

struct spis_test_data {
	uint32_t cnt;
	uint32_t exp_len;
	nrfx_spis_t *spis;
	volatile bool stop;
	bool err;
	uint8_t rx_buf[XFER_LEN];
	uint8_t tx_buf[XFER_LEN];
	uint32_t last_timestamp;
	uint32_t timestamp_idx;
	uint32_t timestamp_max;
	uint32_t exp_diff[4];
};

struct engine_test_data {
	struct ppi_seq_spim engine;
	nrfx_gpiote_t *gpiote;
	uint8_t gpiote_task_ch;
	uint32_t ss_pin;
	uint32_t ss_task;

	uint8_t tx_buf0[XFER_CNT][XFER_LEN];
	uint8_t rx_buf0[XFER_CNT][XFER_LEN];

	uint8_t tx_buf1[XFER_CNT][XFER_LEN];
	uint8_t rx_buf1[XFER_CNT][XFER_LEN];

	uint32_t exp_len;
	uint32_t exp_cnt;

	volatile uint32_t cb_cnt;
	volatile uint32_t tx_byte_cnt;
	volatile uint32_t rx_byte_cnt;
	uint32_t rpt;
	bool stop_immediate;
	volatile bool stop;
	uint32_t t_cb;
	bool err;

	struct k_sem end_sem;
};

static struct spis_test_data spis_data;
static struct engine_test_data engine_data;

static NRF_TIMER_Type *timer = (NRF_TIMER_Type *)DT_REG_ADDR(DT_NODELABEL(dut_trig_timer));

static void *rtc_prepare_and_get(void)
{
#ifdef CONFIG_SOC_NRF54L15_CPUAPP
	void *reg = (void *)0x50105000;
	*(uint32_t *)reg = 1;
	k_msleep(1);
	*(uint32_t *)((uint32_t)reg + 4) = 1;

	return reg;
#else
	return NULL;
#endif
}

static void next_spis(struct spis_test_data *data)
{
	int rv;

	for (int i = 0; i < data->exp_len; i++) {
		data->tx_buf[i] = data->cnt + i;
	}

	rv = nrfx_spis_buffers_set(data->spis, data->tx_buf, data->exp_len,
				   data->rx_buf, data->exp_len);
	zassert_ok(rv, "Unexpected error:%d", rv);
}

static void spis_event_handler(nrfx_spis_event_t const *event, void *context)
{
	struct spis_test_data *data = context;

	if ((event->evt_type == NRFX_SPIS_XFER_DONE) && (data->stop == false)) {
		uint8_t *rx_buf = event->p_rx_buf;

		/* Check that interrupt happens at expected moment. */
	NRF_P2->OUTSET=BIT(9);
		uint32_t now = nrfx_grtc_syscounter_get();
		uint32_t diff = now - data->last_timestamp;

	NRF_P2->OUTCLR=BIT(9);
		if (0 && data->last_timestamp > 0) {
			NRF_P2->OUTSET=BIT(10);
			zassert_within(diff, data->exp_diff[data->timestamp_idx] + 10, 35,
				"Unexpected spis interrupt after %d from last,"
				"expected %d us", diff, data->exp_diff[data->timestamp_idx] + 10);
			NRF_P2->OUTCLR=BIT(10);
		}
		data->last_timestamp = now;
		data->timestamp_idx++;
		data->timestamp_idx = (data->timestamp_idx == data->timestamp_max) ?
			0 : data->timestamp_idx;

		zassert_equal(data->exp_len, event->rx_amount);
		zassert_equal(data->exp_len, event->tx_amount);
		for (int i = 0; i < data->exp_len; i++) {
			uint8_t exp_byte = (uint8_t)(data->cnt + i);

			if (!data->err) {
				data->err = rx_buf[i] != exp_byte;
				if (data->err) {
					/*NRF_P2->OUTSET=BIT(9);*/
					LOG_ERR("spis Rx err");
					/*NRF_P2->OUTCLR=BIT(9);*/
				}
				zassert_false(data->err,
					"Unexpected idx:%d val:%02x exp:%02x",
					i, rx_buf[i], exp_byte);
			}
		}
		data->cnt += data->exp_len;
		next_spis(data);
	}
}

static void spim_event_handler(nrfx_spim_event_t const *event, void *context)
{
	TC_PRINT("Unexpected SPIM interrupt handler call.");
	zassert_false(true);
}

static void spim_init(nrfx_spim_t *instance, bool use_hw_ss)
{
	int rv;
	nrfx_spim_config_t config;

	instance->p_reg = (NRF_SPIM_Type *)DT_REG_ADDR(SPIM_NODE);

	PINCTRL_DT_DEFINE(SPIM_NODE);
	rv = pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(SPIM_NODE), PINCTRL_STATE_DEFAULT);
	zassert_ok(rv, "Unexpected error:%d", rv);

	uint32_t pin = DT_GPIO_PIN(SPIM_NODE, cs_gpios);
	/*uint32_t flags = DT_GPIO_FLAGS(SPIM_NODE, cs_gpios);*/
	uint32_t port = DT_PROP(DT_GPIO_CTLR(SPIM_NODE, cs_gpios), port);
	uint32_t ss_pin = 32 * port + pin;

	/* CS configure. */
	if (use_hw_ss) {
		nrf_gpio_pin_set(ss_pin);
		nrf_gpio_cfg_output(ss_pin);
		nrf_spim_csn_configure(instance->p_reg, ss_pin, NRF_SPIM_CSN_POL_LOW, 32);
		engine_data.ss_task = 0;
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

		rv = nrfx_gpiote_channel_alloc(gpiote, &engine_data.gpiote_task_ch);
		zassert_ok(rv, "Unexpected error:%d", rv);

		task_config.task_ch = engine_data.gpiote_task_ch;

		rv = nrfx_gpiote_output_configure(gpiote, ss_pin, &out_config, &task_config);
		zassert_ok(rv, "Unexpected error:%d", rv);
		nrfx_gpiote_out_task_enable(gpiote, ss_pin);

		engine_data.ss_task = nrfx_gpiote_out_task_address_get(gpiote, ss_pin);
		engine_data.ss_pin = ss_pin;
		engine_data.gpiote = gpiote;
	}

	config.ss_pin = NRF_SPIM_PIN_NOT_CONNECTED;
	config.frequency = MHZ(8);
	config.mode = NRF_SPIM_MODE_0;
	config.use_hw_ss = use_hw_ss;
	config.rx_delay = 1;
	config.bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST;
	config.skip_gpio_cfg = true;
	config.skip_psel_cfg = true;

	rv = nrfx_spim_init(instance, &config, spim_event_handler, NULL);
	zassert_ok(rv, "Unexpected error:%d", rv);
}

static void spis_init(nrfx_spis_t *instance, void *context)
{
	int rv;
	nrfx_spis_config_t config;

	instance->p_reg = (NRF_SPIS_Type *)DT_REG_ADDR(SPIS_NODE);

	PINCTRL_DT_DEFINE(SPIS_NODE);
	rv = pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(SPIS_NODE), PINCTRL_STATE_DEFAULT);
	zassert_ok(rv, "Unexpected error:%d", rv);

	config.mode = NRF_SPIS_MODE_0;
	config.bit_order = NRF_SPIS_BIT_ORDER_MSB_FIRST;
	config.skip_gpio_cfg = true;
	config.skip_psel_cfg = true;

	rv = nrfx_spis_init(instance, &config, spis_event_handler, context);
	zassert_ok(rv, "Unexpected error:%d", rv);

	IRQ_CONNECT(DT_IRQN(SPIS_NODE), DT_IRQ_BY_IDX(SPIS_NODE, 0, priority),
			nrfx_spis_irq_handler, &spis, 0);
}

static uint32_t prepare_buf(uint8_t *buf, size_t cnt, uint32_t init_val)
{
	for (int i = 0; i < cnt; i++) {
		buf[i] = (uint8_t)(init_val + i);
	}

	return init_val + cnt;
}

static void ppi_seq_spim_cb(struct ppi_seq_spim *engine, struct ppi_seq_spim_data *data,
		bool last)
{
	struct engine_test_data *test_data = CONTAINER_OF(engine, struct engine_test_data, engine);

	NRF_P2->OUTSET=BIT(10);
	test_data->t_cb = k_cycle_get_32();
	test_data->cb_cnt++;
	if (test_data->stop == false) {
		for (int i = 0; i <  data->op_cnt; i++) {
			for (int j = 0; j < data->op_len; j++) {
				uint8_t byte = data->rx_buf[i * data->op_len + j];
				uint8_t exp_byte = (uint8_t)test_data->rx_byte_cnt;

				if (test_data->err == false) {
					test_data->err = byte != exp_byte;
					zassert_false(test_data->err,
						"Idx:%d (%d %d) byte:%02x exp:%02x",
						i * data->op_len + j, i, j, byte, exp_byte);
				}
				test_data->rx_byte_cnt++;
			}
		}
	}

	NRF_P2->OUTCLR=BIT(10);
	/* Fill data for the next buffer. */
	test_data->tx_byte_cnt =
		prepare_buf(data->tx_buf, data->op_cnt * data->op_len, test_data->tx_byte_cnt);

	if (test_data->rpt > 0) {
		test_data->rpt--;
		if (test_data->rpt == 0) {
			ppi_seq_spim_stop(engine, test_data->stop_immediate);
		}
	}

	if (last) {
		k_sem_give((struct k_sem *)&test_data->end_sem);
	}
}

enum test_ppi_seq_mode {
	TEST_MODE_RTC,
	TEST_MODE_TIMER,
	TEST_MODE_GRTC_TIMER,
	TEST_MODE_GRTC,
};

enum test_ppi_seq_notifier {
	TEST_NOTIFIER_SYS_TIMER,
	TEST_NOTIFIER_NRFX_TIMER,
};

enum test_ppi_seq_op {
	TEST_OP_SINGLE_HW_SS,
	TEST_OP_SINGLE_GPIOTE_SS,
	TEST_OP_DOUBLE,
};

static uint32_t engine_init(enum test_ppi_seq_mode mode, enum test_ppi_seq_op op,
			enum test_ppi_seq_notifier notifier, uint32_t xfer_len,
			uint32_t xfer_cnt, uint32_t *cycles, uint32_t *period)
{
	static struct ppi_seq_op ops[2];
	static struct ppi_seq_config config;
	static struct ppi_seq_notifier sys_timer_notifier;
	static struct ppi_seq_notifier nrfx_timer_notifier;
	int rv;

	IRQ_CONNECT(DT_IRQN(DT_NODELABEL(dut_cnt_timer)),
		    DT_IRQ_BY_IDX(DT_NODELABEL(dut_cnt_timer), 0, priority),
			nrfx_timer_irq_handler, &nrfx_timer_notifier.nrfx_timer.timer, 0);

	*period = mode == TEST_MODE_RTC ? 305 : 200;
	*cycles = xfer_cnt;

	spis_data.exp_len = xfer_len;
	spis_data.last_timestamp = 0;
	spim_init(&spim, op != TEST_OP_SINGLE_GPIOTE_SS);

	engine_data.cb_cnt = 0;
	engine_data.tx_byte_cnt = 0;
	engine_data.rx_byte_cnt = 0;
	engine_data.engine.spim = &spim;

	config.callback = ppi_seq_spim_internal_cb;
	sys_timer_notifier.type = PPI_SEQ_NOTIFIER_SYS_TIMER;
	sys_timer_notifier.sys_timer.offset = (1000000 * xfer_len) / (MHZ(8) / 8) + 10;
	sys_timer_notifier.sys_timer.extra_main_ops = 0;

	if (op == TEST_OP_SINGLE_GPIOTE_SS) {
		spis_data.timestamp_max = 1;
		spis_data.exp_diff[0] = *period;

		config.task = engine_data.ss_task;
		ops[0].task = nrfx_spim_start_task_address_get(&spim),
		ops[0].offset = 2;
		ops[1].task = engine_data.ss_task;
		ops[1].offset = 14;
		config.extra_ops = ops;
		config.extra_ops_count = 2;
		*cycles = xfer_cnt;
	} else if (op == TEST_OP_DOUBLE) {
		ops[0].task = nrfx_spim_start_task_address_get(&spim),
		ops[0].offset = mode == TEST_MODE_RTC ? 62 : 50;
		spis_data.timestamp_max = 2;
		spis_data.exp_diff[0] = *period - ops[0].offset;
		spis_data.exp_diff[1] = ops[0].offset;
		config.task = nrfx_spim_start_task_address_get(&spim);
		config.extra_ops = ops;
		config.extra_ops_count = 1;
		sys_timer_notifier.sys_timer.extra_main_ops++;

		*cycles = *cycles / 2;
		*period = *period * 2;
	} else {
		spis_data.timestamp_max = 1;
		spis_data.exp_diff[0] = *period;
		config.task = nrfx_spim_start_task_address_get(&spim);
		config.extra_ops = NULL;
		config.extra_ops_count = 0;
	}

	config.period = *period;

	switch (mode) {
	case TEST_MODE_RTC:
		config.use_grtc = false;
		config.timer_reg = NULL;
		config.rtc = rtc_prepare_and_get();
		if (config.rtc == NULL) {
			ztest_test_skip();
		}
		break;
	case TEST_MODE_TIMER:
		config.use_grtc = false;
		config.timer_reg = timer;
		config.rtc = NULL;
		break;
	case TEST_MODE_GRTC_TIMER:
		config.use_grtc = true;
		config.timer_reg = timer;
		config.rtc = NULL;
		break;
	case TEST_MODE_GRTC:
		config.use_grtc = true;
		config.timer_reg = NULL;
		config.rtc = NULL;
		break;
	default:
		zassert_false(true);
		break;
	}

	nrfx_timer_notifier.type = PPI_SEQ_NOTIFIER_NRFX_TIMER;
	nrfx_timer_notifier.nrfx_timer.timer.p_reg =
		(NRF_TIMER_Type *)DT_REG_ADDR(DT_NODELABEL(dut_cnt_timer));
	nrfx_timer_notifier.nrfx_timer.end_seq_event =
			nrfx_spim_end_event_address_get(&spim);

	if (notifier == TEST_NOTIFIER_SYS_TIMER) {
		config.notifier = &sys_timer_notifier;
	} else {
		config.notifier = &nrfx_timer_notifier;
	}

	k_sem_init(&engine_data.end_sem, 0, 1);

	rv = ppi_seq_spim_init(&engine_data.engine, &config);
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
	}

	return op == TEST_OP_DOUBLE ? (*period * 2) : *period;
}

static uint32_t single_run(struct ppi_seq_spim_seq *seq, uint32_t timeout, bool immediate)
{
	int rv;
	uint32_t t;
	uint32_t prev_byte_cnt = engine_data.rx_byte_cnt;
	uint32_t prev_cb_cnt = engine_data.cb_cnt;
	uint32_t prev_spis_cnt = spis_data.cnt;
	uint32_t exp_bytes = XFER_CNT * XFER_LEN;

	t = k_cycle_get_32();
	rv = ppi_seq_spim_start(&engine_data.engine, seq, immediate);
	zassert_ok(rv, "Unexpected error:%d", rv);

	rv = k_sem_take(&engine_data.end_sem, K_USEC(timeout));
	zassert_ok(rv, "Unexpected error:%d", rv);

	zassert_equal(engine_data.rx_byte_cnt - prev_byte_cnt, exp_bytes,
		"Unexpected amount of bytes received: %d, exp:%d",
		engine_data.rx_byte_cnt - prev_byte_cnt, exp_bytes);
	zassert_equal(engine_data.cb_cnt - prev_cb_cnt, 1);
	zassert_equal(spis_data.cnt - prev_spis_cnt, exp_bytes);

	return engine_data.t_cb - t;
}

static void test_single_run(enum test_ppi_seq_mode mode, enum test_ppi_seq_op op,
			enum test_ppi_seq_notifier notifier)
{
	struct ppi_seq_spim_seq seq = {
		.tx_buf = { engine_data.tx_buf0[0] },
		.rx_buf = { engine_data.rx_buf0[0] },
		.op_len = XFER_LEN,
		.op_cnt = XFER_CNT,
		.repeat = 1,
		.cb = ppi_seq_spim_cb
	};
	uint32_t t_immediate, t_sync;
	uint32_t rpt;
	uint32_t period;
	uint32_t timeout;

	engine_init(mode, op, notifier, XFER_LEN, XFER_CNT, &rpt, &period);
	timeout = (rpt + 2) * period;

	engine_data.tx_byte_cnt =
		prepare_buf(engine_data.tx_buf0[0], XFER_LEN * XFER_CNT, engine_data.tx_byte_cnt);

	next_spis(&spis_data);

	t_immediate = single_run(&seq, timeout, true);
	spis_data.last_timestamp = 0;
	t_sync = single_run(&seq, timeout, false);
	printk("t_im:%d t_sync:%d\n", t_immediate, t_sync);

	uint32_t exp_time_sync = rpt * period + period / 2;
	uint32_t exp_time_imm = exp_time_sync - period;

	zassert_within(t_sync - t_immediate, period, period / 4);
	zassert_within(t_immediate, exp_time_imm, period / 2,
			"Time %d us not within:%d with delta:%d",
			t_immediate, exp_time_imm, period / 2);
	zassert_within(t_sync, exp_time_sync, period / 2,
			"Time %d us not within:%d with delta:%d",
			t_sync, exp_time_sync, period / 2);
}

static void test_multi_run(enum test_ppi_seq_mode mode, enum test_ppi_seq_op op,
			enum test_ppi_seq_notifier notifier)
{
	uint32_t rpt = 4;
	struct ppi_seq_spim_seq seq = {
		.tx_buf = { engine_data.tx_buf0[0], engine_data.tx_buf1[0] },
		.rx_buf = { engine_data.rx_buf0[0], engine_data.rx_buf1[0] },
		.op_len = XFER_LEN,
		.op_cnt = XFER_CNT,
		.repeat = rpt,
		.cb = ppi_seq_spim_cb
	};
	uint32_t period;
	uint32_t cycles;
	uint32_t timeout;
	int rv;

	engine_init(mode, op, notifier, XFER_LEN, XFER_CNT, &cycles, &period);
	timeout = rpt * (cycles + 1) * period;

	engine_data.tx_byte_cnt = prepare_buf(engine_data.tx_buf0[0], XFER_LEN * XFER_CNT, 0);
	engine_data.tx_byte_cnt = prepare_buf(engine_data.tx_buf1[0], XFER_LEN * XFER_CNT,
						engine_data.tx_byte_cnt);

	next_spis(&spis_data);

	engine_data.rpt = rpt - 1;
	rv = ppi_seq_spim_start(&engine_data.engine, &seq, false);
	zassert_ok(rv, "Unexpected error:%d", rv);

	rv = k_sem_take(&engine_data.end_sem, K_USEC(timeout));
	zassert_ok(rv, "Unexpected error:%d", rv);

	zassert_equal(engine_data.rx_byte_cnt, rpt * XFER_LEN * XFER_CNT);
	zassert_equal(engine_data.cb_cnt, rpt);
}

static void test_multi_run_async_stop(enum test_ppi_seq_mode mode, enum test_ppi_seq_op op,
			enum test_ppi_seq_notifier notifier, bool immediate)
{
	struct ppi_seq_spim_seq seq = {
		.tx_buf = { engine_data.tx_buf0[0], engine_data.tx_buf1[0] },
		.rx_buf = { engine_data.rx_buf0[0], engine_data.rx_buf1[0] },
		.op_len = XFER_LEN,
		.op_cnt = XFER_CNT,
		.repeat = UINT32_MAX,
		.cb = ppi_seq_spim_cb
	};
	uint32_t period;
	uint32_t cycles;
	uint32_t timeout;
	int rv;

	engine_init(mode, op, notifier, XFER_LEN, XFER_CNT, &cycles, &period);
	timeout = cycles * period * 2;

	engine_data.tx_byte_cnt = prepare_buf(engine_data.tx_buf0[0], XFER_LEN * XFER_CNT, 0);
	engine_data.tx_byte_cnt = prepare_buf(engine_data.tx_buf1[0], XFER_LEN * XFER_CNT,
						engine_data.tx_byte_cnt);

	next_spis(&spis_data);

	for (int i = 0; i < 3; i++) {
		rv = ppi_seq_spim_start(&engine_data.engine, &seq, false);
		zassert_ok(rv, "Unexpected error:%d", rv);

		k_msleep(50);

		uint32_t cb_cnt = engine_data.cb_cnt;

		spis_data.stop = true;
		engine_data.stop = true;
		rv = ppi_seq_spim_stop(&engine_data.engine, immediate);
		zassert_ok(rv, "Unexpected error:%d", rv);
		if (immediate) {
			/* Stop event is NOT called. */
			rv = k_sem_take(&engine_data.end_sem, K_USEC(timeout));
			zassert_equal(rv, -EAGAIN);

			/* No more callbacks. */
			zassert_equal(cb_cnt, engine_data.cb_cnt);
		} else {
			/* Stop event is called. */
			rv = k_sem_take(&engine_data.end_sem, K_USEC(timeout));
			zassert_ok(rv, "Unexpected error:%d", rv);

			/* One more callback. */
			zassert_equal(cb_cnt + 1, engine_data.cb_cnt);
		}

		k_msleep(1);
	}
}

ZTEST(sensor_engine_spi, test_single_run_with_grtc_interval_and_sys_timer)
{
	test_single_run(TEST_MODE_GRTC, TEST_OP_SINGLE_HW_SS, TEST_NOTIFIER_SYS_TIMER);
}

ZTEST(sensor_engine_spi, test_single_run_with_grtc_interval_and_cnt_timer)
{
	test_single_run(TEST_MODE_GRTC, TEST_OP_SINGLE_HW_SS, TEST_NOTIFIER_NRFX_TIMER);
}

ZTEST(sensor_engine_spi, test_single_run_with_cnt_timer_gpiote_ss)
{
	test_single_run(TEST_MODE_GRTC_TIMER, TEST_OP_SINGLE_GPIOTE_SS, TEST_NOTIFIER_NRFX_TIMER);
}

ZTEST(sensor_engine_spi, test_single_run_with_sys_timer_gpiote_ss)
{
	test_single_run(TEST_MODE_GRTC_TIMER, TEST_OP_SINGLE_GPIOTE_SS, TEST_NOTIFIER_SYS_TIMER);
}

ZTEST(sensor_engine_spi, test_single_run_with_trig_timer_and_sys_timer_two_ops)
{
	test_single_run(TEST_MODE_GRTC_TIMER, TEST_OP_DOUBLE, TEST_NOTIFIER_SYS_TIMER);
}

ZTEST(sensor_engine_spi, test_single_run_with_trig_timer_and_cnt_timer_two_ops)
{
	test_single_run(TEST_MODE_GRTC_TIMER, TEST_OP_DOUBLE, TEST_NOTIFIER_NRFX_TIMER);
}

ZTEST(sensor_engine_spi, test_single_run_with_trig_timer_and_cnt_timer)
{
	test_single_run(TEST_MODE_TIMER, TEST_OP_SINGLE_HW_SS, TEST_NOTIFIER_NRFX_TIMER);
}

ZTEST(sensor_engine_spi, test_multi_run_with_trig_timer_and_cnt_timer)
{
	test_multi_run(TEST_MODE_TIMER, TEST_OP_SINGLE_HW_SS, TEST_NOTIFIER_NRFX_TIMER);
}

ZTEST(sensor_engine_spi, test_multi_run_with_trig_timer_and_cnt_timer_two_ops)
{
	test_multi_run(TEST_MODE_GRTC_TIMER, TEST_OP_DOUBLE, TEST_NOTIFIER_NRFX_TIMER);
}

ZTEST(sensor_engine_spi, test_multi_run_with_trig_timer_and_sys_timer_two_ops)
{
	test_multi_run(TEST_MODE_GRTC_TIMER, TEST_OP_DOUBLE, TEST_NOTIFIER_SYS_TIMER);
}

ZTEST(sensor_engine_spi, test_multi_run_with_grtc_and_cnt_timer)
{
	test_multi_run(TEST_MODE_GRTC, TEST_OP_SINGLE_HW_SS, TEST_NOTIFIER_NRFX_TIMER);
}

ZTEST(sensor_engine_spi, test_multi_run_with_grtc_and_sys_timer)
{
	test_multi_run(TEST_MODE_GRTC, TEST_OP_SINGLE_HW_SS, TEST_NOTIFIER_SYS_TIMER);
}

ZTEST(sensor_engine_spi, test_multi_run_async_stop_with_grtc_and_sys_timer)
{
	test_multi_run_async_stop(TEST_MODE_GRTC, TEST_OP_SINGLE_HW_SS,
				  TEST_NOTIFIER_SYS_TIMER, false);
}

ZTEST(sensor_engine_spi, test_multi_run_immediate_async_stop_with_grtc_and_sys_timer)
{
	test_multi_run_async_stop(TEST_MODE_GRTC, TEST_OP_SINGLE_HW_SS,
				  TEST_NOTIFIER_SYS_TIMER, true);
}

ZTEST(sensor_engine_spi, test_single_run_with_rtc_and_cnt_timer)
{
	test_single_run(TEST_MODE_RTC, TEST_OP_SINGLE_HW_SS, TEST_NOTIFIER_NRFX_TIMER);
}

ZTEST(sensor_engine_spi, test_single_run_with_rtc_and_sys_timer)
{
	test_single_run(TEST_MODE_RTC, TEST_OP_SINGLE_HW_SS, TEST_NOTIFIER_SYS_TIMER);
}

ZTEST(sensor_engine_spi, test_multi_run_with_rtc_and_cnt_timer)
{
	test_multi_run(TEST_MODE_RTC, TEST_OP_SINGLE_HW_SS, TEST_NOTIFIER_NRFX_TIMER);
}


ZTEST(sensor_engine_spi, test_multi_run_with_rtc_and_sys_timer)
{
	test_multi_run(TEST_MODE_RTC, TEST_OP_SINGLE_HW_SS, TEST_NOTIFIER_SYS_TIMER);
}

static void before(void *not_used)
{
	memset(&spis_data, 0, sizeof(spis_data));
	memset(&engine_data, 0, sizeof(engine_data));

	NRF_RRAMC->POWER.LOWPOWERCONFIG = 1;
	spis_data.spis = &spis;
	spis_init(&spis, (void *)&spis_data);
	ARG_UNUSED(not_used);
}

static void after(void *not_used)
{
	ARG_UNUSED(not_used);

	if (engine_data.ss_task) {
		nrfx_gpiote_pin_uninit(engine_data.gpiote, engine_data.ss_pin);
		nrfx_gpiote_channel_free(engine_data.gpiote, engine_data.gpiote_task_ch);
	}
	nrfx_spis_uninit(&spis);
	nrfx_spim_uninit(&spim);
	ppi_seq_spim_uninit(&engine_data.engine);
}

#include <hal/nrf_gpio.h>
static void *suite_setup(void)
{
	nrf_gpio_cfg_output(2*32+8);
	nrf_gpio_cfg_output(2*32+9);
	nrf_gpio_cfg_output(2*32+10);

#if 0
	int ch = z_nrf_grtc_timer_special_chan_alloc();
	if (ch < 0) {
		LOG_ERR("failed");
	}
	uint32_t comp_evt = z_nrf_grtc_timer_compare_evt_address_get(ch);
	ppi_trace_enable(ppi_trace_config(4, comp_evt));
	nrfx_grtc_syscounter_cc_interval_set((uint8_t)ch, 1000);
	nrfx_grtc_syscounter_cc_rel_set((uint8_t)ch, 1000,
						NRFX_GRTC_CC_RELATIVE_SYSCOUNTER);
	k_busy_wait(10000);
	nrfx_grtc_syscounter_cc_disable((uint8_t)ch);
	while(1);
#endif

	IRQ_CONNECT(DT_IRQN(SPIM_NODE), DT_IRQ_BY_IDX(SPIM_NODE, 0, priority),
			nrfx_spim_irq_handler, &spim, 0);

	return NULL;
}

ZTEST_SUITE(sensor_engine_spi, NULL, suite_setup, before, after, NULL);
