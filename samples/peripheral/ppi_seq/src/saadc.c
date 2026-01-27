/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "common.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/pinctrl.h>
#include <nrfx_gpiote.h>
#include <nrfx_saadc.h>
#include <zephyr/drivers/gpio/gpio_nrf.h>
LOG_MODULE_DECLARE(ppi_seq);

#define SAADC_NODE DT_NODELABEL(adc)

static NRF_TIMER_Type *trig_timer = (NRF_TIMER_Type *)DT_REG_ADDR(DT_NODELABEL(trig_timer));
#ifdef CONFIG_SOC_NRF54L15_CPUAPP
static const void *rtc_reg = (void *)0x50105000;
#endif

static struct k_sem sem;
static uint32_t repeat_cnt;
static uint32_t buf_idx;
static nrf_saadc_value_t *manual_buffers[2];
static struct ppi_seq seq;
static struct ppi_seq_config seq_config;
static struct ppi_seq_notifier notifier;

static struct ppi_seq_generic_config *curr_cfg;

static int saadc_init(void)
{
	IRQ_CONNECT(DT_IRQN(SAADC_NODE), DT_IRQ_BY_IDX(SAADC_NODE, 0, priority),
			nrfx_saadc_irq_handler, NULL, 0);

	return nrfx_saadc_init(0);
}

static void saadc_uninit(uint32_t ch_mask)
{
	nrfx_saadc_channels_deconfig(ch_mask);
	nrfx_saadc_uninit();
}

static void saadc_event_handler(nrfx_saadc_evt_t const *event)
{
	int rv;

	switch (event->type) {
	case NRFX_SAADC_EVT_DONE:
		repeat_cnt--;
		break;
	case NRFX_SAADC_EVT_FINISHED:
		k_sem_give(&sem);
		break;
	case NRFX_SAADC_EVT_BUF_REQ:
		if (repeat_cnt > 1) {
			rv = nrfx_saadc_buffer_set(manual_buffers[buf_idx], curr_cfg->batch_cnt);
			buf_idx = buf_idx == 0 ? 1 : 0;
		}
		break;
	default:
		break;
	}
}

static int manual_run(const struct shell *sh, struct ppi_seq_generic_config *config, int *load)
{
	int rv;
	nrfx_saadc_channel_t channel_config =
		NRFX_SAADC_DEFAULT_CHANNEL_SE(NRFX_ANALOG_EXTERNAL_AIN4, 0);
	uint32_t total = config->batch_cnt * config->repeat * config->period;
	nrfx_saadc_adv_config_t saadc_config = {
		.oversampling = NRF_SAADC_OVERSAMPLE_DISABLED,
		.burst = NRF_SAADC_BURST_DISABLED,
		.internal_timer_cc = 0,
		.start_on_end = true,
	};
	nrfx_gppi_handle_t gppi_handle;
	nrfx_timer_config_t timer_config = {
		.frequency = MHZ(1),
		.mode = NRF_TIMER_MODE_TIMER,
		.bit_width = NRF_TIMER_BIT_WIDTH_32,
		.p_context = NULL
	};
	nrfx_timer_t timer = {};
	uint32_t ch_mask = BIT(0);
	uint32_t evt_timer = 0, task_saadc = 0;
	uint32_t t = 0;

	/* Initialize TIMER that will be sampling SAADC. */
	timer.p_reg = trig_timer;
	rv = nrfx_timer_init(&timer, &timer_config, NULL);
	if (rv < 0) {
		shell_error(sh, "Failed to initialize the sampling TIMER:%d", rv);
		goto finalize;
	}
	nrfx_timer_extended_compare(&timer, 0, config->period,
			NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, false);

	/* Setup PPI connection between TIMER and SAADC. */
	task_saadc = nrf_saadc_task_address_get(NRF_SAADC, NRF_SAADC_TASK_SAMPLE);
	evt_timer = nrfx_timer_event_address_get(&timer, nrf_timer_compare_event_get(0));
	rv = nrfx_gppi_conn_alloc(evt_timer, task_saadc, &gppi_handle);
	if (rv < 0) {
		shell_error(sh, "Failed to allocate PPI connection:%d", rv);
		goto finalize;
	}
	nrfx_gppi_conn_enable(gppi_handle);

	/* Prepare buffers for SAADC. */
	manual_buffers[0] = malloc(config->batch_cnt * sizeof(nrf_saadc_value_t));
	manual_buffers[1] = malloc(config->batch_cnt * sizeof(nrf_saadc_value_t));

	memset(manual_buffers[0], 0xaa, config->batch_cnt * sizeof(nrf_saadc_value_t));
	memset(manual_buffers[1], 0xaa, config->batch_cnt * sizeof(nrf_saadc_value_t));
	channel_config.channel_config.acq_time = 10 * 8 - 1;
	rv = nrfx_saadc_channels_config(&channel_config, ch_mask);
	if (rv < 0) {
		shell_error(sh, "Failed to configure the SAADC channel:%d", rv);
		goto finalize;
	}

	rv = nrfx_saadc_advanced_mode_set(ch_mask, NRF_SAADC_RESOLUTION_10BIT,
			&saadc_config, saadc_event_handler);
	if (rv < 0) {
		shell_error(sh, "Failed to configure the SAADC:%d", rv);
		goto finalize;
	}

	buf_idx = 1;
	rv = nrfx_saadc_buffer_set(manual_buffers[0], config->batch_cnt);
	if (rv < 0) {
		shell_error(sh, "Failed to set SAADC buffer:%d", rv);
		goto finalize;
	}

	k_sem_init(&sem, 0, 1);
	repeat_cnt = config->repeat;
	rv = nrfx_saadc_mode_trigger();
	if (rv < 0) {
		shell_error(sh, "Failed to trigger SAADC:%d", rv);
		goto finalize;
	}

	DBG_PIN_TOGGLE();
	if (IS_ENABLED(CONFIG_CPU_LOAD)) {
		(void)cpu_load_get(true);
	}
	t = k_cycle_get_32();
	nrfx_timer_enable(&timer);

	rv = k_sem_take(&sem, K_USEC(total + 10000));
	if (rv < 0) {
		shell_error(sh, "Failed to complete the manual run on time:%d", rv);
		goto finalize;
	}
	t = k_cycle_get_32() - t;
	if (IS_ENABLED(CONFIG_CPU_LOAD)) {
		*load = cpu_load_get(true);
	}
	DBG_PIN_TOGGLE();

finalize:
	if (rv < 0) shell_error(sh, "eee:%d", rv);
	nrfx_timer_disable(&timer);
	nrfx_timer_uninit(&timer);
	nrfx_gppi_conn_disable(gppi_handle);
	nrfx_gppi_conn_free(evt_timer, task_saadc, gppi_handle);

	free(manual_buffers[0]);
	free(manual_buffers[1]);
	return rv < 0 ? rv : t;
}

static void saadc_ppi_seq_cb(struct ppi_seq *ppi_seq, bool last)
{
	if (last) {
		k_sem_give(&sem);
	}
}

static nrfx_gppi_handle_t gppi_saadc_started_sample;
static nrfx_gppi_handle_t gppi_saadc_end_stop;

static int initialize_ppi_seq(const struct shell *sh, struct ppi_seq_generic_config *config)
{
	uint32_t evt_end = nrf_saadc_event_address_get(NRF_SAADC, NRF_SAADC_EVENT_END);
	uint32_t evt_started = nrf_saadc_event_address_get(NRF_SAADC, NRF_SAADC_EVENT_STARTED);
	uint32_t task_start = nrf_saadc_task_address_get(NRF_SAADC, NRF_SAADC_TASK_START);
	uint32_t task_sample = nrf_saadc_task_address_get(NRF_SAADC, NRF_SAADC_TASK_SAMPLE);
	uint32_t task_stop = nrf_saadc_task_address_get(NRF_SAADC, NRF_SAADC_TASK_STOP);
	int rv;

	rv = nrfx_gppi_conn_alloc(evt_started, task_sample, &gppi_saadc_started_sample);
	if (rv < 0) {
		return rv;
	}
	nrfx_gppi_conn_enable(gppi_saadc_started_sample);

	rv = nrfx_gppi_conn_alloc(evt_end, task_stop, &gppi_saadc_end_stop);
	if (rv < 0) {
		return rv;
	}
	nrfx_gppi_conn_enable(gppi_saadc_end_stop);

	notifier.type = config->notifier;
	if (notifier.type == PPI_SEQ_NOTIFIER_SYS_TIMER) {
		notifier.sys_timer.offset = 10;
	} else {
		notifier.nrfx_timer.timer.p_reg =
			(NRF_TIMER_Type *)DT_REG_ADDR(DT_NODELABEL(cnt_timer));
		notifier.nrfx_timer.end_seq_event = evt_end;
		notifier.nrfx_timer.extra_main_ops = 0;
	}

	seq_config.notifier = &notifier;
	if (config->mode == PPI_SEQ_SAMPLE_GRTC) {
		seq_config.timer_reg = NULL;
		seq_config.rtc = NULL;
		seq_config.use_grtc = true;
	} else if (config->mode == PPI_SEQ_SAMPLE_RTC) {
		seq_config.timer_reg = NULL;
		seq_config.rtc = (void *)rtc_reg;
		seq_config.use_grtc = false;
	} else {
		seq_config.timer_reg = trig_timer;
		seq_config.rtc = NULL;
		seq_config.use_grtc = false;
	}

	seq_config.extra_ops = NULL;
	seq_config.extra_ops_count = 0;

	seq_config.callback = saadc_ppi_seq_cb;
	seq_config.task = task_start;

	return ppi_seq_init(&seq, &seq_config);
}

static void uninit_ppi_seq(void)
{
	ppi_seq_uninit(&seq);
	uint32_t evt_started = nrf_saadc_event_address_get(NRF_SAADC, NRF_SAADC_EVENT_STARTED);
	uint32_t task_sample = nrf_saadc_task_address_get(NRF_SAADC, NRF_SAADC_TASK_SAMPLE);
	uint32_t evt_end = nrf_saadc_event_address_get(NRF_SAADC, NRF_SAADC_EVENT_END);
	uint32_t task_stop = nrf_saadc_task_address_get(NRF_SAADC, NRF_SAADC_TASK_STOP);

	nrfx_gppi_conn_disable(gppi_saadc_started_sample);
	nrfx_gppi_conn_disable(gppi_saadc_end_stop);
	nrfx_gppi_conn_free(evt_started, task_sample, gppi_saadc_started_sample);
	nrfx_gppi_conn_free(evt_end, task_stop, gppi_saadc_end_stop);
}

static int seq_run(const struct shell *sh, struct ppi_seq_generic_config *config, int *load)
{
	int rv;
	nrfx_saadc_channel_t channel_config =
		NRFX_SAADC_DEFAULT_CHANNEL_SE(NRFX_ANALOG_EXTERNAL_AIN4, 0);
	uint32_t total = config->batch_cnt * config->repeat * config->period;
	uint32_t ch_mask = BIT(0);
	nrfx_saadc_adv_config_t saadc_config = {
		.oversampling = NRF_SAADC_OVERSAMPLE_DISABLED,

		.burst = NRF_SAADC_BURST_DISABLED,
		.internal_timer_cc = 0,
		.start_on_end = true,
	};
	uint32_t t = 0;

	manual_buffers[0] = malloc(config->batch_cnt * sizeof(nrf_saadc_value_t));
	manual_buffers[1] = malloc(config->batch_cnt * sizeof(nrf_saadc_value_t));
	memset(manual_buffers[0], 0xaa, config->batch_cnt * sizeof(nrf_saadc_value_t));
	memset(manual_buffers[1], 0xaa, config->batch_cnt * sizeof(nrf_saadc_value_t));

	channel_config.channel_config.acq_time = 10 * 8 - 1;
	rv = nrfx_saadc_channels_config(&channel_config, ch_mask);
	if (rv < 0) {
		shell_error(sh, "Failed to configure the SAADC channel:%d", rv);
		goto finalize;
	}

	rv = nrfx_saadc_advanced_mode_set(ch_mask, NRF_SAADC_RESOLUTION_10BIT,
			&saadc_config, NULL);
	if (rv < 0) {
		shell_error(sh, "Failed to configure the SAADC:%d", rv);
		goto finalize;
	}

	buf_idx = 1;
	nrf_saadc_buffer_init(NRF_SAADC, manual_buffers[0], 1);
	nrf_saadc_enable(NRF_SAADC);
	*(uint32_t *)((uint32_t)NRF_SAADC + 0x63c) = 1;
	NRF_SAADC->EVENTS_STARTED=0;
	NRF_SAADC->EVENTS_DONE=0;
	NRF_SAADC->EVENTS_END=0;

	k_sem_init(&sem, 0, 1);
	repeat_cnt = config->repeat;

	DBG_PIN_TOGGLE();
	if (IS_ENABLED(CONFIG_CPU_LOAD)) {
		(void)cpu_load_get(true);
	}
	t = k_cycle_get_32();
	rv = ppi_seq_start(&seq, config->period, config->batch_cnt, config->repeat);
	if (rv < 0) {
		shell_error(sh, "Failed to trigger SAADC:%d", rv);
		goto finalize;
	}

	rv = k_sem_take(&sem, K_USEC(total + 10000));
	if (rv < 0) {
		shell_error(sh, "Failed to complete the PPI sequencer run on time:%d", rv);
		goto finalize;
	}
	t = k_cycle_get_32() - t;
	if (IS_ENABLED(CONFIG_CPU_LOAD)) {
		*load = cpu_load_get(true);
	}
	DBG_PIN_TOGGLE();

	printk("saadc evts: started:%d done:%d end:%d\n",
		NRF_SAADC->EVENTS_STARTED,
		NRF_SAADC->EVENTS_DONE,
		NRF_SAADC->EVENTS_END);

finalize:
	uint8_t *buf =(uint8_t *)manual_buffers[0];
	for (int i = 0; i < curr_cfg->batch_cnt * sizeof(nrf_saadc_value_t); i++) {
		printk("%02x ", buf[i]);
	}
	printk("\n");
	printk("saadc: ptr:%08x buf:%08x\n", NRF_SAADC->RESULT.PTR, (uint32_t)manual_buffers[0]);
	free(manual_buffers[0]);
	free(manual_buffers[1]);
	return rv < 0 ? rv : t;
}

static void print_status(const struct shell *sh)
{
	print_generic_status(sh);
}

static int ppi_seq_run_handler(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t pm_cnt = 0;
	int t_manual = 0, t_ppi = 0;
	int load = 0, load_ppi = 0;
	int rv;

	curr_cfg = get_generic_config();
	print_status(sh);

	rv = saadc_init();
	if (rv < 0) {
		goto finalize;
	}

	pm_cnt = stop_shell_uart(sh);

	if (argc > 1) {
		uint32_t t = shell_strtoul(argv[1], 0, &rv);
		for (int i = 0; i < 10; i++) {
			k_msleep(t);
		}
	}

	rv =  manual_run(sh, curr_cfg, &load);
	if (rv < 0) {
		goto finalize;
	}
	t_manual = rv;

	restart_shell_uart(sh, pm_cnt);
	uint8_t *buf =(uint8_t *)manual_buffers[0];
	for (int i = 0; i < curr_cfg->batch_cnt * sizeof(nrf_saadc_value_t); i++) {
		printk("%02x ", buf[i]);
	}
	printk("\n");
	free(manual_buffers[0]);
	pm_cnt = stop_shell_uart(sh);


	rv = initialize_ppi_seq(sh, curr_cfg);
	if (rv < 0) {
		goto finalize;
	}

	rv = seq_run(sh, curr_cfg, &load_ppi);
	if (rv < 0) {
		goto finalize;
	}
	t_ppi = rv;

finalize:
	restart_shell_uart(sh, pm_cnt);

	if (rv >= 0) {
		shell_print(sh, "- Manual operation:\n\tLOAD:%d.%d%%\n\ttook:%d us\n",
			load / 10, load % 10, t_manual);
		shell_print(sh, "- PPI Sequencer transfers:\n\tLOAD:%d.%d%%\n\ttook:%d us\n",
			load_ppi / 10, load_ppi % 10, t_ppi);
	} else if (rv == -ENOTSUP) {
		shell_error(sh, "Configuration not supported\n");
	} else {
		shell_error(sh, "Command failed with:%d\n", rv);
	}
	uninit_ppi_seq();
	saadc_uninit(BIT(0));
	return rv;
}

static int ppi_seq_status_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	print_status(sh);

	return 0;
}

SHELL_SUBCMD_ADD((ppi_sequence), saadc_status, NULL, "Print current SPI configuration",
		ppi_seq_status_handler, 1, 0);
SHELL_SUBCMD_ADD((ppi_sequence), saadc_run, NULL,
		"Run SAADC sequence using the current configuration",
		ppi_seq_run_handler, 1, 1);
