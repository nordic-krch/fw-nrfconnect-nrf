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
#include <drivers/sensor_engine.h>
#include <zephyr/drivers/pinctrl.h>
#include <nrfx_spim.h>
#include <nrfx_spis.h>
#include <nrfx_timer.h>
#include <nrfx_grtc.h>
#include <debug/ppi_trace.h>
#include <zephyr/logging/log.h>
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
};

struct engine_test_data {
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
	bool err;

	struct k_sem end_sem;
};

static struct spis_test_data spis_data;
static struct engine_test_data engine_data;
static struct sensor_engine engine;
static nrfx_timer_t trig_timer;
static nrfx_timer_t cnt_timer;

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

		zassert_equal(data->exp_len, event->rx_amount);
		zassert_equal(data->exp_len, event->tx_amount);
		for (int i = 0; i < data->exp_len; i++) {
			uint8_t exp_byte = (uint8_t)(data->cnt + i);

			if (!data->err) {
				data->err = rx_buf[i] != exp_byte;
				if (data->err) {
					NRF_P2->OUTSET=BIT(9);
					LOG_ERR("spis Rx err");
					NRF_P2->OUTCLR=BIT(9);
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

static void spim_init(nrfx_spim_t *instance)
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
	uint32_t cs_pin = 32 * port + pin;

	/* CS configure. */
	nrf_gpio_pin_set(cs_pin);
	nrf_gpio_cfg_output(cs_pin);
	nrf_spim_csn_configure(instance->p_reg, cs_pin, NRF_SPIM_CSN_POL_LOW, 32);

	config.frequency = MHZ(8);
	config.mode = NRF_SPIM_MODE_0;
	config.use_hw_ss = true;
	config.rx_delay = 1;
	config.bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST;
	config.skip_gpio_cfg = true;
	config.skip_psel_cfg = true;

	rv = nrfx_spim_init(instance, &config, spim_event_handler, NULL);
	zassert_ok(rv, "Unexpected error:%d", rv);

	IRQ_CONNECT(DT_IRQN(SPIM_NODE), DT_IRQ_BY_IDX(SPIM_NODE, 0, priority),
			nrfx_spim_irq_handler, &spim, 0);

	IRQ_CONNECT(DT_IRQN(DT_NODELABEL(dut_cnt_timer)),
		    DT_IRQ_BY_IDX(DT_NODELABEL(dut_cnt_timer), 0, priority),
			nrfx_timer_irq_handler, &cnt_timer, 0);
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

static void sensor_engine_seq_cb(struct sensor_engine *engine,
				 struct sensor_engine_data *data, void *context)
{
	struct engine_test_data *test_data = context;

	NRF_P2->OUTSET=BIT(10);

	test_data->cb_cnt++;
	if (test_data->stop) {
		NRF_P2->OUTCLR=BIT(10);
		return;
	}

	for (int i = 0; i <  data->op_cnt; i++) {
		for (int j = 0; j < data->op_len; j++) {
			uint8_t byte = data->rx_buf[i * data->op_len + j];
			uint8_t exp_byte = (uint8_t)test_data->rx_byte_cnt;

			if (test_data->err == false) {
				test_data->err = byte != exp_byte;
				zassert_false(test_data->err, "Idx:%d byte:%02x exp:%02x",
					i * data->op_len + j, byte, exp_byte);
			}
			test_data->rx_byte_cnt++;
		}
	}
	NRF_P2->OUTCLR=BIT(10);

	/* Fill data for the next buffer. */
	test_data->tx_byte_cnt =
		prepare_buf(data->tx_buf, data->op_cnt * data->op_len, test_data->tx_byte_cnt);

	if (test_data->rpt > 0) {
		test_data->rpt--;
		if (test_data->rpt == 0) {
			sensor_engine_stop(engine, test_data->stop_immediate);
		}
	}
}

static void sensor_engine_stop_cb(struct sensor_engine *engine, void *context)
{
	struct engine_test_data *data = context;

	k_sem_give((struct k_sem *)&data->end_sem);
}

static void engine_init(void *rtc, nrfx_timer_t *trig_timer,
			nrfx_timer_t *cnt_timer, uint32_t period, uint32_t xfer_len)
{
	static struct sensor_engine_config config;
	int rv;

	engine_data.cb_cnt = 0;
	engine_data.tx_byte_cnt = 0;
	engine_data.rx_byte_cnt = 0;
	config.spi = &spim;
	config.cnt_timer = cnt_timer;
	config.callback = sensor_engine_seq_cb;
	config.stop_callback = sensor_engine_stop_cb;
	config.xfer_time = (1000000 * xfer_len) / (MHZ(8) / 8) + 10;
	config.context = &engine_data;
	config.period = period;

	if (rtc) {
		config.rtc = rtc;
		config.mode = SENSOR_ENGINE_MODE_RTC;
	} else if (trig_timer) {
		config.trig_timer = trig_timer;
		config.mode = SENSOR_ENGINE_MODE_TIMER;
	} else {
		config.mode = SENSOR_ENGINE_MODE_GRTC;
	}

	k_sem_init(&engine_data.end_sem, 0, 1);

	rv = sensor_engine_init(&engine, &config);
	zassert_ok(rv, "Unexpected error:%d", rv);

	static bool once;
	if (once == false && config.mode == SENSOR_ENGINE_MODE_GRTC) {
		int chan = nrfx_gppi_ep_channel_get((uint32_t)&NRF_GRTC->EVENTS_COMPARE[0]);
		LOG_INF("grtc channel %d", chan);
		int err = ppi_trace_dppi_ch_trace(4, chan, NRF_DPPIC20);
		if (err < 0) {
			LOG_ERR("ppi trace failed");
		}
		once = true;
	}

}

static uint32_t single_run(struct sensor_engine_seq *seq, uint32_t timeout, bool immediate)
{
	int rv;
	uint32_t t;
	uint32_t prev_byte_cnt = engine_data.rx_byte_cnt;
	uint32_t prev_cb_cnt = engine_data.cb_cnt;
	uint32_t prev_spis_cnt = spis_data.cnt;
	uint32_t exp_bytes = XFER_CNT * XFER_LEN;

	t = k_cycle_get_32();
	rv = sensor_engine_start(&engine, seq, immediate);
	zassert_ok(rv, "Unexpected error:%d", rv);

	rv = k_sem_take(&engine_data.end_sem, K_USEC(timeout));
	zassert_ok(rv, "Unexpected error:%d", rv);

	zassert_equal(engine_data.rx_byte_cnt - prev_byte_cnt, exp_bytes,
		"Unexpected amount of bytes received: %d, exp:%d",
		engine_data.rx_byte_cnt - prev_byte_cnt, exp_bytes);
	zassert_equal(engine_data.cb_cnt - prev_cb_cnt, 1);
	zassert_equal(spis_data.cnt - prev_spis_cnt, exp_bytes);

	return k_cycle_get_32() - t;
}

static void test_single_run(void *rtc, nrfx_timer_t *trig_tmr, nrfx_timer_t *cnt_tmr)
{
	struct sensor_engine_seq seq = {
		.tx_buf = { engine_data.tx_buf0[0] },
		.rx_buf = { engine_data.rx_buf0[0] },
		.op_len = XFER_LEN,
		.op_cnt = XFER_CNT
	};
	uint32_t t_immediate, t_sync;
	uint32_t period = rtc ? 305 : 200;
	uint32_t timeout = (XFER_CNT + 2) * period;

	spis_data.exp_len = XFER_LEN;
	engine_init(rtc, trig_tmr, cnt_tmr, period, XFER_LEN);

	engine_data.tx_byte_cnt =
		prepare_buf(engine_data.tx_buf0[0], XFER_LEN * XFER_CNT, engine_data.tx_byte_cnt);

	next_spis(&spis_data);

	t_immediate = single_run(&seq, timeout, true);
	t_sync = single_run(&seq, timeout, false);

	zassert_within(t_sync - t_immediate, period, period / 4);
	zassert_within(t_immediate, (XFER_CNT - 1) * period + period / 2, period,
			"Time %d us not within:%d with delta:%d",
			t_immediate, (XFER_CNT - 1) * period, period);
	zassert_within(t_sync, (XFER_CNT * period) + period / 2, period,
			"Time %d us not within:%d with delta:%d",
			t_sync, XFER_CNT * period, period / 2);
}

static void test_multi_run(void *rtc, nrfx_timer_t *trig_tmr, nrfx_timer_t *cnt_tmr)
{
	struct sensor_engine_seq seq = {
		.tx_buf = { engine_data.tx_buf0[0], engine_data.tx_buf1[0] },
		.rx_buf = { engine_data.rx_buf0[0], engine_data.rx_buf1[0] },
		.op_len = XFER_LEN,
		.op_cnt = XFER_CNT
	};
	uint32_t period = rtc ? 305 : 200;
	uint32_t rpt = 4;
	uint32_t timeout = XFER_CNT * period * (rpt + 1);
	int rv;

	spis_data.exp_len = XFER_LEN;
	engine_init(rtc, trig_tmr, cnt_tmr, period, XFER_LEN);

	engine_data.tx_byte_cnt = prepare_buf(engine_data.tx_buf0[0], XFER_LEN * XFER_CNT, 0);
	engine_data.tx_byte_cnt = prepare_buf(engine_data.tx_buf1[0], XFER_LEN * XFER_CNT,
						engine_data.tx_byte_cnt);

	next_spis(&spis_data);

	engine_data.rpt = rpt - 1;
	rv = sensor_engine_start(&engine, &seq, false);
	zassert_ok(rv, "Unexpected error:%d", rv);

	rv = k_sem_take(&engine_data.end_sem, K_USEC(timeout));
	zassert_ok(rv, "Unexpected error:%d", rv);

	zassert_equal(engine_data.rx_byte_cnt, rpt * XFER_LEN * XFER_CNT);
	zassert_equal(engine_data.cb_cnt, rpt);
}

static void test_multi_run_async_stop(void *rtc, nrfx_timer_t *trig_tmr, nrfx_timer_t *cnt_tmr,
				bool immediate)
{
	struct sensor_engine_seq seq = {
		.tx_buf = { engine_data.tx_buf0[0], engine_data.tx_buf1[0] },
		.rx_buf = { engine_data.rx_buf0[0], engine_data.rx_buf1[0] },
		.op_len = XFER_LEN,
		.op_cnt = XFER_CNT
	};
	uint32_t period = rtc ? 305 : 200;
	uint32_t timeout = XFER_CNT * period * 2;
	int rv;

	spis_data.exp_len = XFER_LEN;
	engine_init(rtc, trig_tmr, cnt_tmr, period, XFER_LEN);

	engine_data.tx_byte_cnt = prepare_buf(engine_data.tx_buf0[0], XFER_LEN * XFER_CNT, 0);
	engine_data.tx_byte_cnt = prepare_buf(engine_data.tx_buf1[0], XFER_LEN * XFER_CNT,
						engine_data.tx_byte_cnt);

	next_spis(&spis_data);

	for (int i = 0; i < 3; i++) {
		rv = sensor_engine_start(&engine, &seq, false);
		zassert_ok(rv, "Unexpected error:%d", rv);

		k_msleep(50);

		uint32_t cb_cnt = engine_data.cb_cnt;

		spis_data.stop = true;
		engine_data.stop = true;
		sensor_engine_stop(&engine, immediate);
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

ZTEST(sensor_engine_spi, test_single_run_with_grtc_interval_and_sw_timer)
{
	test_single_run(NULL, NULL, NULL);
}

ZTEST(_sensor_engine_spi, test_single_run_with_grtc_interval_and_cnt_timer)
{
	test_single_run(NULL, NULL, &cnt_timer);
}

ZTEST(sensor_engine_spi, test_single_run_with_trig_timer_and_cnt_timer)
{
	test_single_run(NULL, &trig_timer, &cnt_timer);
}

ZTEST(sensor_engine_spi, test_multi_run_with_trig_timer_and_cnt_timer)
{
	test_multi_run(NULL, &trig_timer, &cnt_timer);
}

ZTEST(sensor_engine_spi, test_multi_run_with_grtc_and_cnt_timer)
{
	test_multi_run(NULL, NULL, &cnt_timer);
}

ZTEST(sensor_engine_spi, test_multi_run_with_grtc_and_sw_timer)
{
	test_multi_run(NULL, NULL, NULL);
}

ZTEST(sensor_engine_spi, test_multi_run_async_stop_with_grtc_and_sw_timer)
{
	test_multi_run_async_stop(NULL, NULL, NULL, false);
}

ZTEST(sensor_engine_spi, test_multi_run_immediate_async_stop_with_grtc_and_sw_timer)
{
	test_multi_run_async_stop(NULL, NULL, NULL, true);
}

#ifdef CONFIG_SOC_NRF54L15_CPUAPP
static void *rtc_prepare_and_get(void)
{
	void *reg = (void *)0x50105000;
	*(uint32_t *)reg = 1;
	k_msleep(1);
	*(uint32_t *)((uint32_t)reg + 4) = 1;

	return reg;
}

/* nrf54l15 has undocumented RTC instance that can be used for that purpose. */
ZTEST(sensor_engine_spi, test_single_run_with_rtc_and_cnt_timer)
{
	/* Start and stop RTC as there is a initial delay when starting RTC for the first time. */
	test_single_run(rtc_prepare_and_get(), NULL, &cnt_timer);
}

ZTEST(sensor_engine_spi, test_single_run_with_rtc_and_sw_timer)
{
	test_single_run(rtc_prepare_and_get(), NULL, NULL);
}

ZTEST(sensor_engine_spi, test_multi_run_with_rtc_and_cnt_timer)
{
	test_multi_run(rtc_prepare_and_get(), NULL, &cnt_timer);
}


ZTEST(sensor_engine_spi, test_multi_run_with_rtc_and_sw_timer)
{
	test_multi_run(rtc_prepare_and_get(), NULL, NULL);
}
#endif

static void before(void *not_used)
{
	memset(&spis_data, 0, sizeof(spis_data));
	memset(&engine_data, 0, sizeof(engine_data));

	spis_data.spis = &spis;
	spis_init(&spis, (void *)&spis_data);
	ARG_UNUSED(not_used);
}

static void after(void *not_used)
{
	nrfx_spis_uninit(&spis);
	sensor_engine_uninit(&engine);
	ARG_UNUSED(not_used);
}

#include <hal/nrf_gpio.h>
static void *suite_setup(void)
{
	nrf_gpio_cfg_output(2*32+8);
	nrf_gpio_cfg_output(2*32+9);
	nrf_gpio_cfg_output(2*32+10);
	trig_timer.p_reg = (NRF_TIMER_Type *)DT_REG_ADDR(DT_NODELABEL(dut_trig_timer));
	cnt_timer.p_reg = (NRF_TIMER_Type *)DT_REG_ADDR(DT_NODELABEL(dut_cnt_timer));

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

	spim_init(&spim);

	return NULL;
}

ZTEST_SUITE(_sensor_engine_spi, NULL, suite_setup, before, after, NULL);
