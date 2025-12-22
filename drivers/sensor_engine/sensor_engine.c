/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <drivers/sensor_engine.h>
#include <zephyr/drivers/timer/nrf_grtc_timer.h>
#include <nrfx_grtc.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor_engine);

typedef struct {
    __OM uint32_t TASKS_START;
    __OM uint32_t TASKS_STOP;
    __OM uint32_t TASKS_CLEAR;
    __OM uint32_t TASKS_TRIGOVRFLW;
    __IM uint32_t RESERVED[12];
    __OM uint32_t TASKS_CAPTURE[4];
    __IM uint32_t RESERVED1[12];
    __IOM uint32_t SUBSCRIBE_START;
    __IOM uint32_t SUBSCRIBE_STOP;
    __IOM uint32_t SUBSCRIBE_CLEAR;
    __IOM uint32_t SUBSCRIBE_TRIGOVRFLW;
    __IM uint32_t RESERVED2[12];
    __IOM uint32_t SUBSCRIBE_CAPTURE[4];
    __IM uint32_t RESERVED3[12];
    __IOM uint32_t EVENTS_TICK;
    __IOM uint32_t EVENTS_OVRFLW;
    __IM uint32_t RESERVED4[14];
    __IOM uint32_t EVENTS_COMPARE[4];
    __IM uint32_t RESERVED5[12];
    __IOM uint32_t PUBLISH_TICK;
    __IOM uint32_t PUBLISH_OVRFLW;
    __IM uint32_t RESERVED6[14];
    __IOM uint32_t PUBLISH_COMPARE[4];
    __IM uint32_t RESERVED7[12];
    __IOM uint32_t SHORTS;
    __IM uint32_t RESERVED8[63];
    __IOM uint32_t INTEN;
    __IOM uint32_t INTENSET;
    __IOM uint32_t INTENCLR;
    __IM uint32_t RESERVED9[13];
    __IOM uint32_t EVTEN;
    __IOM uint32_t EVTENSET;
    __IOM uint32_t EVTENCLR;
    __IM uint32_t RESERVED10[110];
    __IM uint32_t COUNTER;
    __IOM uint32_t PRESCALER;
    __IM uint32_t RESERVED11[13];
    __IOM uint32_t CC[4];
} NRF_RTC_Type;

static void rtc_init(NRF_RTC_Type *rtc, uint32_t period, uint32_t gap)
{
	uint32_t period_ticks = (uint32_t)(((uint64_t)period * 32768) / 1000000);
	rtc->PRESCALER = 0;
	rtc->SHORTS = BIT(0); /* Compare0->Clear */
	rtc->EVTEN = BIT(16); /* Compare 0 */
	rtc->CC[0] = period_ticks;
	if (gap != 0) {
		uint32_t gap_ticks = (uint32_t)(((uint64_t)gap * 32768) / 1000000);

		rtc->CC[1] = gap_ticks;
		rtc->EVTEN = BIT(16 + 1); /* Compare 0 */
	}
}

static void rtc_start(NRF_RTC_Type *rtc)
{
	rtc->TASKS_CLEAR = 1;
	rtc->TASKS_START = 1;
}

static void rtc_stop(NRF_RTC_Type *rtc)
{
	rtc->TASKS_STOP = 1;
}

static uint32_t rtc_evt_addr(NRF_RTC_Type *rtc)
{
	return (uint32_t)&rtc->EVENTS_COMPARE[0];
}

static uint32_t rtc_gap_evt_addr(NRF_RTC_Type *rtc)
{
	return (uint32_t)&rtc->EVENTS_COMPARE[1];
}

static void stop_engine(struct sensor_engine *engine, bool cnt_timer_in_use)
{
	engine->stop_req = false;
	nrfx_gppi_conn_disable(engine->ppi_spi_start);
	if (engine->config->mode == SENSOR_ENGINE_MODE_GRTC) {
		nrfx_grtc_syscounter_cc_disable((uint8_t)engine->grtc_chan);
		nrfx_grtc_syscounter_cc_interval_set((uint8_t)engine->grtc_chan, 0);
	} else if (engine->config->mode == SENSOR_ENGINE_MODE_RTC) {
		rtc_stop(engine->config->rtc);
	} else {
		nrfx_timer_disable(engine->config->trig_timer);
	}
	if (cnt_timer_in_use) {
		nrfx_timer_disable(engine->config->cnt_timer);
	} else {
		k_timer_stop(&engine->timer);
	}
	/* TODO Abort increases the current. */
	/*nrfx_spim_abort(engine->config->spi);*/
}

static void timer_handler(struct sensor_engine *engine, bool cnt_timer_in_use)
{
	uint32_t prev_idx = engine->curr_idx;
	bool stop;

	engine->curr_idx = (engine->curr_idx + 1) & 0x1;
	if (engine->seq.tx_buf[1] && !engine->stop_req) {
		/* Update pointers and enable PPI to start next scheduled transfer. */
		engine->config->spi->p_reg->DMA.TX.PTR =
			(uint32_t)engine->seq.tx_buf[engine->curr_idx];
		engine->config->spi->p_reg->DMA.RX.PTR =
			(uint32_t)engine->seq.rx_buf[engine->curr_idx];
		if (engine->config->cnt_timer != NULL) {
			nrfx_gppi_group_enable(engine->ppi_group);
		}
		stop = false;
	} else {
		/* Stop everything. */
		stop_engine(engine, cnt_timer_in_use);
		stop = true;
	}

	if (engine->config->callback) {
		struct sensor_engine_data data = {
			.tx_buf = engine->seq.tx_buf[prev_idx],
			.rx_buf = engine->seq.rx_buf[prev_idx],
			.op_len = engine->seq.op_len,
			.op_cnt = engine->seq.op_cnt,
		};
		engine->config->callback(engine, &data, engine->config->context);
	}

	if (stop) {
		engine->seq.op_len = 0;
		if (engine->config->stop_callback) {
			engine->config->stop_callback(engine, engine->config->context);
		}
	}
}

static void nrfx_timer_handler(nrf_timer_event_t event_type, void *context)
{
	LOG_DBG("completion TIMER");
	timer_handler((struct sensor_engine *)context, true);
}

static void k_timer_handler(struct k_timer *timer)
{
	LOG_DBG("completion k_timer");
	timer_handler((struct sensor_engine *)k_timer_user_data_get(timer), false);
}

int sensor_engine_init(struct sensor_engine *engine, const struct sensor_engine_config *config)
{
	uint32_t spi_start_tsk = nrfx_spim_start_task_address_get(config->spi);
	uint32_t group_dis_tsk;
	uint32_t periodic_evt;
	uint32_t gap_evt;
	nrfx_timer_config_t timer_config = {
		.frequency = MHZ(1),
		.mode = NRF_TIMER_MODE_TIMER,
		.bit_width = NRF_TIMER_BIT_WIDTH_32,
		.p_context = engine
	};
	int rv;

	engine->config = config;

	if (config->mode == SENSOR_ENGINE_MODE_GRTC) {
		rv = z_nrf_grtc_timer_special_chan_alloc();
		if (rv < 0) {
			return rv;
		}

		engine->grtc_chan = rv;
		nrfx_grtc_syscounter_cc_interval_set((uint8_t)engine->grtc_chan, config->period);
		periodic_evt = z_nrf_grtc_timer_compare_evt_address_get(rv);
	} else if (config->mode == SENSOR_ENGINE_MODE_RTC) {
		rtc_init(config->rtc, config->period, config->gap);
		periodic_evt = rtc_evt_addr(config->rtc);
	} else {
		if (config->cnt_timer == NULL) {
			/* If TIMER is used for triggering then TIMER need to be used also for
			 * transfer counting. System timer cannot be used because it is driven
			 * from different source than TIMER (HFCLK vs LFCLK) and if clock source
			 * are not accurate it lead to undefined behavior.
			 */
			return -ENOTSUP;
		}
		/* Setup TIMER for repeating transactions. */
		rv = nrfx_timer_init(config->trig_timer, &timer_config, NULL);
		if (rv < 0) {
			return rv;
		}

		nrfx_timer_extended_compare(config->trig_timer, NRF_TIMER_CC_CHANNEL0,
					    config->period, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK,
					    false);
		periodic_evt = nrfx_timer_event_address_get(config->trig_timer,
							nrf_timer_compare_event_get(0));
	}


	k_timer_init(&engine->timer, k_timer_handler, NULL);
	k_timer_user_data_set(&engine->timer, engine);

	rv = nrfx_gppi_conn_alloc(periodic_evt, spi_start_tsk, &engine->ppi_spi_start);
	if (rv < 0) {
		return rv;
	}

	if (config->gap != 0) {
		if (config->mode == SENSOR_ENGINE_MODE_RTC) {
			gap_evt = rtc_gap_evt_addr(config->rtc);
		} else if (config->trig_timer != 0) {
			uint32_t shorts = 0;

			gap_evt = nrfx_timer_event_address_get(config->trig_timer,
						 nrf_timer_compare_event_get(1));

			if (config->mode == SENSOR_ENGINE_MODE_GRTC) {
				nrfx_gppi_ep_attach(nrfx_timer_task_address_get(config->trig_timer,
						       NRF_TIMER_TASK_START), engine->ppi_spi_start);
				shorts = NRF_TIMER_SHORT_COMPARE1_STOP_MASK |
					 NRF_TIMER_SHORT_COMPARE1_CLEAR_MASK;
			}
			nrfx_timer_extended_compare(config->trig_timer, NRF_TIMER_CC_CHANNEL1,
						    config->gap, shorts, false);
		} else {
			/* GRTC has only one INTERVAL channel so it is not supported .*/
			return -ENOTSUP;
		}
		nrfx_gppi_ep_attach(gap_evt, engine->ppi_spi_start);
	}

	if (config->cnt_timer == NULL) {
		/* Counting TIMER is optional and needed only for very small gaps where
		 * interrupt latency exceeds the gap size (usually < 100 us)
		 */
		return 0;
	}

	uint32_t spi_end_evt = nrfx_spim_end_event_address_get(config->spi);
	uint32_t spi_stop_tsk = nrf_spim_task_address_get(config->spi->p_reg,
							  NRF_SPIM_TASK_STOP);
	uint32_t cnt_tsk = nrfx_timer_task_address_get(config->cnt_timer,
						       NRF_TIMER_TASK_COUNT);
	uint32_t seq_done_evt = nrfx_timer_event_address_get(config->cnt_timer,
						 nrf_timer_compare_event_get(0));

	/* Setup TIMER for counting transactions. */
	timer_config.mode = NRF_TIMER_MODE_LOW_POWER_COUNTER;
	rv = nrfx_timer_init(config->cnt_timer, &timer_config, nrfx_timer_handler);
	if (rv < 0) {
		return rv;
	}

	rv = nrfx_gppi_conn_alloc(spi_end_evt, cnt_tsk, &engine->ppi_cnt);
	if (rv < 0) {
		return rv;
	}
	nrfx_gppi_ep_attach(spi_stop_tsk, engine->ppi_cnt);
	nrfx_gppi_conn_enable(engine->ppi_cnt);

	rv = nrfx_gppi_group_alloc(nrfx_gppi_domain_id_get(spi_start_tsk),
				   &engine->ppi_group);
	if (rv < 0) {
		return rv;
	}
	nrfx_gppi_group_ep_add(engine->ppi_group, spi_start_tsk);

	group_dis_tsk = nrfx_gppi_group_task_dis_addr(engine->ppi_group);
	rv = nrfx_gppi_conn_alloc(seq_done_evt, group_dis_tsk, &engine->ppi_stop);
	if (rv < 0) {
		return rv;
	}
	nrfx_gppi_conn_enable(engine->ppi_stop);

	return rv;
}

void sensor_engine_uninit(struct sensor_engine *engine)
{
	uint32_t spi_start_tsk = nrfx_spim_start_task_address_get(engine->config->spi);
	uint32_t periodic_evt;

	if (engine->config->mode == SENSOR_ENGINE_MODE_GRTC) {
		periodic_evt = z_nrf_grtc_timer_compare_evt_address_get(engine->grtc_chan);
		z_nrf_grtc_timer_chan_free(engine->grtc_chan);
	} else if (engine->config->mode == SENSOR_ENGINE_MODE_RTC) {
		periodic_evt = rtc_evt_addr(engine->config->rtc);
	} else /*if (engine->config->trig_timer)*/ {
		/* Uninit TIMER for repeating transactions. */
		nrfx_timer_uninit(engine->config->trig_timer);
		periodic_evt = nrfx_timer_event_address_get(engine->config->trig_timer,
							nrf_timer_compare_event_get(0));
	}
	nrfx_gppi_conn_disable(engine->ppi_spi_start);
	nrfx_gppi_conn_free(periodic_evt, spi_start_tsk, engine->ppi_spi_start);

	if (engine->config->cnt_timer == NULL) {
		return;
	}

	uint32_t spi_end_evt = nrfx_spim_end_event_address_get(engine->config->spi);
	uint32_t spi_stop_tsk = nrf_spim_task_address_get(engine->config->spi->p_reg,
							  NRF_SPIM_TASK_STOP);
	uint32_t cnt_tsk = nrfx_timer_task_address_get(engine->config->cnt_timer,
						       NRF_TIMER_TASK_COUNT);
	uint32_t seq_done_evt = nrfx_timer_event_address_get(engine->config->cnt_timer,
						 nrf_timer_compare_event_get(0));
	uint32_t group_dis_tsk = nrfx_gppi_group_task_dis_addr(engine->ppi_group);

	if (engine->config->gap != 0) {
		uint32_t gap_evt;

		if (engine->config->mode == SENSOR_ENGINE_MODE_RTC) {
			gap_evt = rtc_gap_evt_addr(engine->config->rtc);
		} else {
			gap_evt = nrfx_timer_event_address_get(engine->config->trig_timer,
						 nrf_timer_compare_event_get(1));
			if (engine->config->mode == SENSOR_ENGINE_MODE_GRTC) {
				uint32_t t = nrfx_timer_task_address_get(engine->config->trig_timer,
									 NRF_TIMER_TASK_START);

				nrfx_gppi_ep_clear(t);
			}
		}
		nrfx_gppi_ep_clear(gap_evt);
	}

	nrfx_gppi_ep_clear(spi_stop_tsk);
	nrfx_gppi_group_free(engine->ppi_group);
	nrfx_gppi_conn_disable(engine->ppi_cnt);
	nrfx_gppi_conn_free(spi_end_evt, cnt_tsk, engine->ppi_cnt);
	nrfx_gppi_conn_disable(engine->ppi_stop);
	nrfx_gppi_conn_free(seq_done_evt, group_dis_tsk, engine->ppi_stop);

	nrfx_timer_uninit(engine->config->cnt_timer);
}

#define MIN_LATENCY_US 100

int sensor_engine_start(struct sensor_engine *engine, struct sensor_engine_seq *seq, bool immediate)
{
	__ASSERT_NO_MSG(seq != NULL);
	nrfx_spim_xfer_desc_t xfer_desc = {
		.p_tx_buffer = seq->tx_buf[0],
		.tx_length = seq->op_len,
		.p_rx_buffer = seq->rx_buf[0],
		.rx_length = seq->op_len,
	};
	uint32_t spi_flags = NRFX_SPIM_FLAG_TX_POSTINC |
			     NRFX_SPIM_FLAG_RX_POSTINC |
			     NRFX_SPIM_FLAG_NO_XFER_EVT_HANDLER |
			     NRFX_SPIM_FLAG_REPEATED_XFER |
			     (immediate ? 0 : NRFX_SPIM_FLAG_HOLD_XFER);
	int rv;

	if (engine->seq.op_len != 0) {
		return -EBUSY;
	}

	engine->seq = *seq;
	engine->curr_idx = 0;

	if (engine->config->cnt_timer == NULL) {
		bool single_run = seq->tx_buf[1] == NULL;
		uint32_t rpt_cnt = (engine->config->gap > 0) ? seq->op_cnt / 2 : seq->op_cnt;
		uint32_t cnt = immediate ? rpt_cnt - 1: rpt_cnt;
		uint32_t off = engine->config->period / 4 + engine->config->xfer_time;
		k_timeout_t timeout = K_USEC(engine->config->period * cnt + off);
		k_timeout_t rpt_timeout = single_run ?
			K_NO_WAIT : K_USEC(engine->config->period * rpt_cnt);

		k_timer_start(&engine->timer, timeout, rpt_timeout);
	} else {
		nrfx_timer_enable(engine->config->cnt_timer);
		nrfx_timer_extended_compare(engine->config->cnt_timer, NRF_TIMER_CC_CHANNEL0,
					    seq->op_cnt, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, true);

	}

	nrfx_gppi_conn_enable(engine->ppi_spi_start);

	rv = nrfx_spim_xfer(engine->config->spi, &xfer_desc, spi_flags);
	if (rv < 0) {
		return rv;
	}

	if (engine->config->mode == SENSOR_ENGINE_MODE_GRTC) {
		nrfx_grtc_syscounter_cc_rel_set((uint8_t)engine->grtc_chan, engine->config->period,
						NRFX_GRTC_CC_RELATIVE_SYSCOUNTER);
	} else if (engine->config->mode == SENSOR_ENGINE_MODE_RTC) {
		rtc_start(engine->config->rtc);
	} else {
		nrfx_timer_enable(engine->config->trig_timer);
	}

	return 0;
}

int sensor_engine_stop(struct sensor_engine *engine, bool immediate)
{
	if (engine->seq.op_len == 0) {
		return -EALREADY;
	}

	if (!immediate) {
		engine->stop_req = true;
		return 0;
	}

	stop_engine(engine, engine->config->cnt_timer != NULL);
	engine->seq.op_len = 0;

	return 0;
}
