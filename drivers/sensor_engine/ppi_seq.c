/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <drivers/ppi_seq.h>
#include <zephyr/drivers/timer/nrf_grtc_timer.h>
#include <nrfx_grtc.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ppi_seq);

static void timer_handler(struct ppi_seq *ppi_seq);

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

static void rtc_cc_set(NRF_RTC_Type *rtc, uint32_t cc, uint32_t us)
{
	uint32_t ticks = (uint32_t)(((uint64_t)us * 32768) / 1000000);

	rtc->EVTEN = BIT(16 + cc);
	rtc->CC[cc] = ticks;
}

static void rtc_init(NRF_RTC_Type *rtc, uint32_t period)
{
	rtc->PRESCALER = 0;
	rtc->SHORTS = BIT(0); /* Compare0->Clear */
	rtc_cc_set(rtc, 0, period);
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

#if 0
static uint32_t rtc_task_start_addr(NRF_RTC_Type *rtc)
{
	return (uint32_t)&rtc->TASKS_START;
}

static uint32_t rtc_task_stop_addr(NRF_RTC_Type *rtc)
{
	return (uint32_t)&rtc->TASKS_STOP;
}
#endif

static uint32_t rtc_evt_addr(NRF_RTC_Type *rtc, uint32_t cc)
{
	return (uint32_t)&rtc->EVENTS_COMPARE[cc];
}

static void nrfx_timer_handler(nrf_timer_event_t event_type, void *context)
{
	LOG_DBG("completion TIMER");
	timer_handler((struct ppi_seq *)context);
}

static void k_timer_handler(struct k_timer *timer)
{
	NRF_P2->OUTSET=BIT(8);
	LOG_DBG("completion k_timer");
	timer_handler((struct ppi_seq *)k_timer_user_data_get(timer));
	NRF_P2->OUTCLR=BIT(8);
}

static int ppi_alloc(struct ppi_seq *ppi_seq, uint32_t eep, uint32_t tep, bool enable)
{
	int rv;

	if (ppi_seq->ppi_cnt == ARRAY_SIZE(ppi_seq->ppi_pool)) {
		return -ENODEV;
	}

	rv = nrfx_gppi_conn_alloc(eep, tep, &ppi_seq->ppi_pool[ppi_seq->ppi_cnt]);
	if (rv < 0) {
		return rv;
	}

	if (enable) {
		nrfx_gppi_conn_enable(ppi_seq->ppi_pool[ppi_seq->ppi_cnt]);
	}

	rv = ppi_seq->ppi_cnt;
	ppi_seq->ppi_cnt++;
	return rv;
}

/** @brief Initialize mechanism for notifying about sequence completion.
 *
 * There are 2 methods:
 *  - nrfx_timer in counter mode
 *  - system_timer
 */
static int ppi_seq_notifier_init(struct ppi_seq *ppi_seq)
{
	struct ppi_seq_notifier *notifier = ppi_seq->config->notifier;
	int rv;

	if (notifier->type == PPI_SEQ_NOTIFIER_SYS_TIMER) {
		k_timer_init(&notifier->sys_timer.timer, k_timer_handler, NULL);
		k_timer_user_data_set(&notifier->sys_timer.timer, ppi_seq);
		/* Calculate offset when system timer should expire. It should be after sequence
		 * is completed but enough time before next sequence starts.
		 */
		notifier->sys_timer.offset += ppi_seq->config->period / 4;
		if (ppi_seq->config->extra_ops) {
			for (size_t i = 0; i < ppi_seq->config->extra_ops_count; i++) {
				notifier->sys_timer.offset +=
					ppi_seq->config->extra_ops[i].offset;
			}
		}
		return 0;
	}

	nrfx_timer_config_t timer_config = {
		.frequency = MHZ(1),
		.mode = NRF_TIMER_MODE_LOW_POWER_COUNTER,
		.bit_width = NRF_TIMER_BIT_WIDTH_32,
		.p_context = ppi_seq
	};

	uint32_t cnt_task = nrfx_timer_task_address_get(&notifier->nrfx_timer.timer,
						        NRF_TIMER_TASK_COUNT);

	rv = nrfx_timer_init(&notifier->nrfx_timer.timer, &timer_config, nrfx_timer_handler);
	if (rv < 0) {
		return rv;
	}

	rv = ppi_alloc(ppi_seq, notifier->nrfx_timer.end_seq_event, cnt_task, true);
	if (rv < 0) {
		return rv;
	}

	return 0;
}

/** @brief Uninitialize mechanism for notifying about sequence completion. */
static void ppi_seq_notifier_uninit(struct ppi_seq *ppi_seq)
{
	struct ppi_seq_notifier *notifier = ppi_seq->config->notifier;

	if (notifier->type == PPI_SEQ_NOTIFIER_SYS_TIMER) {
		return;
	}

	uint32_t cnt_task = nrfx_timer_task_address_get(&notifier->nrfx_timer.timer,
						        NRF_TIMER_TASK_COUNT);

	nrfx_timer_uninit(&notifier->nrfx_timer.timer);
	nrfx_gppi_ep_clear(notifier->nrfx_timer.end_seq_event);
	nrfx_gppi_ep_clear(cnt_task);
}

/** @brief Stop mechanism used for notifying completion of the sequence. */
static void ppi_seq_notifier_stop(struct ppi_seq *ppi_seq)
{
	struct ppi_seq_notifier *notifier = ppi_seq->config->notifier;

	if (notifier->type == PPI_SEQ_NOTIFIER_SYS_TIMER) {
		k_timer_stop(&notifier->sys_timer.timer);
		return;
	}

	nrfx_timer_disable(&notifier->nrfx_timer.timer);
}

/** @brief Start mechanism for notifying a completion of the sequence. */
static void ppi_seq_notifier_start(struct ppi_seq *ppi_seq, size_t cb_length, bool immediate)
{
	struct ppi_seq_notifier *notifier = ppi_seq->config->notifier;

	if (notifier->type == PPI_SEQ_NOTIFIER_SYS_TIMER) {
		uint32_t cycles = cb_length / (1 + notifier->sys_timer.extra_main_ops);
		uint32_t rpt = cycles - (immediate ? 1 : 0);
		uint32_t first = rpt * ppi_seq->config->period + notifier->sys_timer.offset;
		uint32_t next = cycles * ppi_seq->config->period;

		k_timer_start(&notifier->sys_timer.timer, K_USEC(first), K_USEC(next));
		return;
	}

	uint32_t val = cb_length;
	nrf_timer_cc_channel_t ch = NRF_TIMER_CC_CHANNEL0;
	uint32_t shorts = NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK;

	nrfx_timer_enable(&notifier->nrfx_timer.timer);
	nrfx_timer_extended_compare(&notifier->nrfx_timer.timer, ch, val, shorts, true);
}

static void stop_ppi_seq(struct ppi_seq *ppi_seq)
{
	nrfx_gppi_conn_disable(ppi_seq->ppi_pool[0]);
	if ((ppi_seq->config->use_grtc == true) || (ppi_seq->config->timer_reg == NULL)) {
		nrfx_grtc_syscounter_cc_disable((uint8_t)ppi_seq->grtc_chan);
	} else if (ppi_seq->config->rtc != NULL) {
		rtc_stop(ppi_seq->config->rtc);
	} else {
		nrfx_timer_disable(&ppi_seq->timer);
	}

	ppi_seq_notifier_stop(ppi_seq);
	ppi_seq->in_use = 0;
	/* TODO Abort increases the current. */
	/*nrfx_spim_abort(ppi_seq->config->spi);*/
}

static void timer_handler(struct ppi_seq *ppi_seq)
{
	bool stop = false;

	ppi_seq->repeat--;
	if (ppi_seq->repeat == 0) {
		stop_ppi_seq(ppi_seq);
		stop = true;
	}

	ppi_seq->config->callback(ppi_seq, stop);
}

static int eep_tep_connect(struct ppi_seq *ppi_seq, uint32_t eep, uint32_t tep)
{
	int ch = nrfx_gppi_ep_channel_get(tep);
	uint32_t domain_id;
	nrfx_gppi_handle_t handle;

	if (ch < 0) {
		return ppi_alloc(ppi_seq, eep, tep, true);
	}

	domain_id = nrfx_gppi_domain_id_get(tep);
	for (int i = 0; i < ppi_seq->ppi_cnt; i++) {
		handle = ppi_seq->ppi_pool[i];
		if (nrfx_gppi_domain_channel_get(handle, domain_id) == ch) {
			nrfx_gppi_ep_attach(eep, handle);
			return 0;
		}
	}

	return -EINVAL;
}

static int extra_ops_with_rtc_init(struct ppi_seq *ppi_seq)
{
	return -ENOTSUP;
}

static int extra_ops_with_timer_init(struct ppi_seq *ppi_seq)
{
	const struct ppi_seq_config *config = ppi_seq->config;
	bool same_evt = config->task == config->extra_ops[config->extra_ops_count - 1].task;
	uint32_t max_ops = same_evt ? (NRF_TIMER_CC_COUNT_MAX - 1) : NRF_TIMER_CC_COUNT_MAX;
	uint32_t last_ch = same_evt ? config->extra_ops_count : (config->extra_ops_count - 1);
	int rv;

	if (config->extra_ops_count > max_ops) {
		/* Not enough channels to handle that. */
		return -EINVAL;
	}

	uint32_t tep = nrfx_timer_task_address_get(&ppi_seq->timer, NRF_TIMER_TASK_START);

	rv = nrfx_gppi_ep_attach(tep, ppi_seq->ppi_pool[0]);
	if (rv < 0) {
		LOG_ERR("Task cannot be attached to the connection. "
			"Use TIMER from the same domain as the target task %d", rv);
	}

	for (size_t i = 0; i < config->extra_ops_count; i++) {
		uint32_t eep = nrfx_timer_event_address_get(&ppi_seq->timer,
							nrf_timer_compare_event_get(i));

		rv = eep_tep_connect(ppi_seq, eep, config->extra_ops[i].task);
		if (rv < 0) {
			return rv;
		}

		nrfx_timer_compare(&ppi_seq->timer, i, config->extra_ops[i].offset, false);
	}

	/* Add stop clear when extra events cycle ends. */
	if (same_evt) {
		nrfx_timer_compare(&ppi_seq->timer, config->extra_ops_count,
				config->extra_ops[config->extra_ops_count - 1].offset + 1, false);
	}
	nrf_timer_shorts_set(ppi_seq->timer.p_reg,
			nrf_timer_short_compare_stop_get(last_ch) |
			nrf_timer_short_compare_clear_get(last_ch));

	return 0;
}

static void extra_ops_with_timer_uninit(struct ppi_seq *ppi_seq)
{
	nrfx_gppi_ep_clear(nrfx_timer_task_address_get(&ppi_seq->timer, NRF_TIMER_TASK_START));
	for (size_t i = 0; i < ppi_seq->config->extra_ops_count; i++) {
		uint32_t eep = nrfx_timer_event_address_get(&ppi_seq->timer,
							nrf_timer_compare_event_get(i));

		nrfx_gppi_ep_clear(eep);
		nrfx_gppi_ep_clear(ppi_seq->config->extra_ops[i].task);
	}
}

static void extra_ops_with_rtc_uninit(struct ppi_seq *ppi_seq)
{
}

static int extra_ops_init(struct ppi_seq *ppi_seq)
{
	if (ppi_seq->config->extra_ops == NULL) {
		return 0;
	}

	if (ppi_seq->config->use_grtc == false) {
		return -EINVAL;
	}

	if (ppi_seq->config->timer_reg) {
		return extra_ops_with_timer_init(ppi_seq);
	}

	return extra_ops_with_rtc_init(ppi_seq);
}

static void extra_ops_uninit(struct ppi_seq *ppi_seq)
{
	if (ppi_seq->config->timer_reg) {
		extra_ops_with_timer_uninit(ppi_seq);
		return;
	}

	extra_ops_with_rtc_uninit(ppi_seq);
}

int ppi_seq_init(struct ppi_seq *ppi_seq, const struct ppi_seq_config *config)
{
	uint32_t periodic_evt = 0;
	int rv;

	ppi_seq->config = config;

	ppi_seq->grtc_chan = UINT8_MAX;
	if ((config->use_grtc == true) || ((config->timer_reg == NULL) && (config->rtc == NULL))) {
		rv = z_nrf_grtc_timer_special_chan_alloc();
		if (rv < 0) {
			return rv;
		}

		ppi_seq->grtc_chan = (uint8_t)rv;
		nrfx_grtc_syscounter_cc_interval_set(ppi_seq->grtc_chan, config->period);
		periodic_evt = z_nrf_grtc_timer_compare_evt_address_get(rv);
	}

	if (config->rtc != NULL) {
		rtc_init(config->rtc, config->period);
		if (periodic_evt == 0) {
			periodic_evt = rtc_evt_addr(config->rtc, 0);
		}
	}

	if (config->timer_reg) {
		__ASSERT_NO_MSG(config->timer_reg != NULL);
		if (!config->use_grtc && (config->notifier->type == PPI_SEQ_NOTIFIER_SYS_TIMER)) {
			/* If TIMER is used for triggering then TIMER need to be used also for
			 * completion notifier. System timer cannot be used because it is driven
			 * from different source than TIMER (HFCLK vs LFCLK) and if clock source
			 * are not accurate it lead to undefined behavior.
			 */
			return -ENOTSUP;
		}

		static const nrfx_timer_config_t timer_config = {
			.frequency = MHZ(1),
			.mode = NRF_TIMER_MODE_TIMER,
			.bit_width = NRF_TIMER_BIT_WIDTH_32,
		};

		ppi_seq->timer.p_reg = config->timer_reg;
		/* Setup TIMER for repeating transactions. */
		rv = nrfx_timer_init(&ppi_seq->timer, &timer_config, NULL);
		if (rv < 0) {
			return rv;
		}

		if (periodic_evt == 0) {
			nrfx_timer_extended_compare(&ppi_seq->timer, NRF_TIMER_CC_CHANNEL0,
					    config->period, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK,
					    false);
			periodic_evt = nrfx_timer_event_address_get(&ppi_seq->timer,
							nrf_timer_compare_event_get(0));
		}
	}

	rv = ppi_alloc(ppi_seq, periodic_evt, config->task, false);
	if (rv < 0) {
		return rv;
	}

	rv = ppi_seq_notifier_init(ppi_seq);
	if (rv < 0) {
		return rv;
	}

	return extra_ops_init(ppi_seq);
}

void ppi_seq_uninit(struct ppi_seq *ppi_seq)
{
	uint32_t periodic_evt = 0;

	(void)ppi_seq_stop(ppi_seq, true);

	extra_ops_uninit(ppi_seq);

	if ((ppi_seq->config->use_grtc == true) ||
	    ((ppi_seq->config->timer_reg == NULL) && (ppi_seq->config->rtc == NULL))) {
		periodic_evt = z_nrf_grtc_timer_compare_evt_address_get(ppi_seq->grtc_chan);
		nrfx_grtc_syscounter_cc_interval_set(ppi_seq->grtc_chan, 0);
		z_nrf_grtc_timer_chan_free(ppi_seq->grtc_chan);
	}

	if ((ppi_seq->config->rtc != NULL) && (periodic_evt == 0)) {
		periodic_evt = rtc_evt_addr(ppi_seq->config->rtc, 0);
	} else if (ppi_seq->config->timer_reg != NULL) {
		/* Uninit TIMER for repeating transactions. */
		if (periodic_evt == 0) {
			periodic_evt = nrfx_timer_event_address_get(&ppi_seq->timer,
							nrf_timer_compare_event_get(0));
		}
		nrfx_timer_uninit(&ppi_seq->timer);
	}

	nrfx_gppi_ep_clear(periodic_evt);
	nrfx_gppi_ep_clear(ppi_seq->config->task);

	ppi_seq_notifier_uninit(ppi_seq);

	for (uint8_t i = 0; i < ppi_seq->ppi_cnt; i++) {
		nrfx_gppi_conn_disable(ppi_seq->ppi_pool[i]);
		nrfx_gppi_domain_conn_free(ppi_seq->ppi_pool[i]);
	}
	ppi_seq->ppi_cnt = 0;
}

int ppi_seq_start(struct ppi_seq *ppi_seq, size_t cb_length, int repeat, bool immediate)
{
	bool use_grtc = ppi_seq_with_grtc_used(ppi_seq);

	if (atomic_cas(&ppi_seq->in_use, 0, 1) == false) {
		return -EALREADY;
	}

	ppi_seq->repeat = repeat;
	ppi_seq_notifier_start(ppi_seq, cb_length, immediate);
	nrfx_gppi_conn_enable(ppi_seq->ppi_pool[0]);

	if (use_grtc) {
		nrfx_grtc_syscounter_cc_rel_set(ppi_seq->grtc_chan,
						immediate ? 1 : ppi_seq->config->period,
						NRFX_GRTC_CC_RELATIVE_SYSCOUNTER);
	} else if (ppi_seq->config->rtc != NULL) {
		rtc_start(ppi_seq->config->rtc);
	} else {
		nrfx_timer_enable(&ppi_seq->timer);
	}

	return 0;
}

int ppi_seq_stop(struct ppi_seq *ppi_seq, bool immediate)
{
	if (ppi_seq->in_use == 0) {
		return -EALREADY;
	}

	if (!immediate) {
		ppi_seq->repeat = 1;
		return 0;
	}

	stop_ppi_seq(ppi_seq);

	return 0;
}
