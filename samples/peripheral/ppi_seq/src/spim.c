/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "common.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <nrfx_gpiote.h>
#include <nrfx_spim.h>
#include <drivers/ppi_seq_i2c_spi.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/gpio/gpio_nrf.h>
#include <zephyr/debug/cpu_load.h>
LOG_MODULE_DECLARE(ppi_seq);

#define SPI_FREQ MHZ(8)

#define SPIM_NODE DT_NODELABEL(sample_spi)
static nrfx_spim_t spim;
static NRF_TIMER_Type *trig_timer = (NRF_TIMER_Type *)DT_REG_ADDR(DT_NODELABEL(trig_timer));
static struct ppi_seq_i2c_spi seq_spim;
static struct ppi_seq_config seq_config;
static struct ppi_seq_notifier notifier;
static struct ppi_seq_extra_op extra_op;
static struct k_sem spi_sem;

#ifdef CONFIG_SOC_NRF54L15_CPUAPP
static const void *rtc_reg = (void *)0x50105000;
#endif

static uint32_t current_xfer_length = 8;
static uint32_t current_second_xfer_offset;

static int spim_init(void)
{
	int rv;
	nrfx_spim_config_t config;

	IRQ_CONNECT(DT_IRQN(SPIM_NODE), DT_IRQ_BY_IDX(SPIM_NODE, 0, priority),
			nrfx_spim_irq_handler, &spim, 0);

	spim.p_reg = (NRF_SPIM_Type *)DT_REG_ADDR(SPIM_NODE);

	PINCTRL_DT_DEFINE(SPIM_NODE);
	rv = pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(SPIM_NODE), PINCTRL_STATE_DEFAULT);
	if (rv < 0) {
		LOG_ERR("SPI pinctrl initialization failed:%d", rv);
		return rv;
	}

	/* CS configure. */
	uint32_t pin = DT_GPIO_PIN(SPIM_NODE, cs_gpios);
	uint32_t port = DT_PROP(DT_GPIO_CTLR(SPIM_NODE, cs_gpios), port);
	uint32_t ss_pin = 32 * port + pin;

	nrf_gpio_pin_set(ss_pin);
	nrf_gpio_cfg_output(ss_pin);
	nrf_spim_csn_configure(spim.p_reg, ss_pin, NRF_SPIM_CSN_POL_LOW, 16);

	config.ss_pin = NRF_SPIM_PIN_NOT_CONNECTED;
	config.frequency = SPI_FREQ;
	config.mode = NRF_SPIM_MODE_0;
	config.use_hw_ss = true;
	config.rx_delay = 1;
	config.bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST;
	config.skip_gpio_cfg = true;
	config.skip_psel_cfg = true;

	rv = nrfx_spim_init(&spim, &config, NULL, NULL);
	if (rv < 0) {
		LOG_ERR("Failed to initialize nrfx_spim instance %d", rv);
	}

	return rv;
}

static void spim_uninit(void)
{
	NRF_SPIM_Type *spim_reg = (NRF_SPIM_Type *)DT_REG_ADDR(SPIM_NODE);
	uint32_t pin = DT_GPIO_PIN(SPIM_NODE, cs_gpios);
	uint32_t port = DT_PROP(DT_GPIO_CTLR(SPIM_NODE, cs_gpios), port);
	uint32_t ss_pin = 32 * port + pin;

	nrfx_spim_uninit(&spim);

	nrf_gpio_cfg_default(nrf_spim_miso_pin_get(spim_reg));
	nrf_gpio_cfg_default(nrf_spim_mosi_pin_get(spim_reg));
	nrf_gpio_cfg_default(nrf_spim_sck_pin_get(spim_reg));
	nrf_gpio_cfg_default(ss_pin);
	nrf_spim_csn_configure(spim.p_reg, NRF_SPIM_PIN_NOT_CONNECTED, NRF_SPIM_CSN_POL_LOW, 0);
}

static int initialize_single_xfer(const struct shell *sh, struct ppi_seq_generic_config *config)
{
	notifier.type = config->notifier;
	if (notifier.type == PPI_SEQ_NOTIFIER_SYS_TIMER) {
		notifier.sys_timer.offset = (1000000 * current_xfer_length) / (SPI_FREQ / 8) + 10;
	} else {
		notifier.nrfx_timer.timer.p_reg =
			(NRF_TIMER_Type *)DT_REG_ADDR(DT_NODELABEL(cnt_timer));
		notifier.nrfx_timer.end_seq_event = nrfx_spim_end_event_address_get(&spim);
		notifier.nrfx_timer.extra_main_ops = (current_second_xfer_offset != 0) ? 1 : 0;
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
		seq_config.use_grtc = (current_second_xfer_offset != 0);
	}

	if (current_second_xfer_offset != 0) {
		extra_op.task = nrfx_spim_start_task_address_get(&spim);
		extra_op.offset = current_second_xfer_offset;
		seq_config.extra_ops = &extra_op;
		seq_config.extra_ops_count = 1;
		if (config->mode == PPI_SEQ_SAMPLE_GRTC) {
			shell_error(sh, "Double transfer batch does not support GRTC only, "
					"requires RTC or TIMER");
			return -ENOTSUP;
		}
	} else {
		seq_config.extra_ops = NULL;
		seq_config.extra_ops_count = 0;
	}

	seq_config.callback = ppi_seq_spim_internal_cb;
	seq_config.task = nrfx_spim_start_task_address_get(&spim);
	seq_spim.spim = &spim;

	return ppi_seq_spim_init(&seq_spim, &seq_config);
}

static void uninitialize(void)
{
	ppi_seq_spim_uninit(&seq_spim);
	spim_uninit();
}

static void ppi_seq_spim_cb(struct ppi_seq_spim *engine, struct ppi_seq_spim_data *data,
		bool last)
{
	NRF_P2->OUTSET=BIT(8);
	if (last) {
		k_sem_give(&spi_sem);
	}
	NRF_P2->OUTCLR=BIT(8);
}

static int perform_single_xfer_set(struct ppi_seq_generic_config *config,
				   uint32_t timeout, int *load)
{
	uint8_t *bufs[4];
	uint32_t t;
	int rv;

	for (int i = 0; i < ARRAY_SIZE(bufs); i++) {
		bufs[i] = malloc(current_xfer_length * config->batch_cnt);
		if (bufs[i] == NULL) {
			return -ENOMEM;
		}
		for (int j = 0; j < current_xfer_length * config->batch_cnt; j++) {
			bufs[i][j] = j % current_xfer_length;
		}
	}

	struct ppi_seq_spim_job job = {
		.desc = {
			.p_tx_buffer = bufs[0],
			.tx_length = current_xfer_length,
			.p_rx_buffer = bufs[1],
			.rx_length = current_xfer_length,
		},
		.tx_second_buf = bufs[2],
		.rx_second_buf = bufs[3],
		.batch_cnt = config->batch_cnt,
		.extra_xfers = (current_second_xfer_offset != 0) ? 1 : 0,
		.repeat = config->repeat,
		.tx_postinc = true,
		.cb = ppi_seq_spim_cb
	};

	k_sem_init(&spi_sem, 0, 1);

	DBG_PIN_TOGGLE();
	if (IS_ENABLED(CONFIG_CPU_LOAD)) {
		(void)cpu_load_get(true);
	}
	t = k_cycle_get_32();

	rv = ppi_seq_spim_start(&seq_spim, config->period, &job);
	if (rv < 0) {
		return rv;
	}

	rv = k_sem_take(&spi_sem, K_USEC(timeout));
	if (rv < 0) {
		return rv;
	}

	t = k_cycle_get_32() - t;
	*load = cpu_load_get(true);
	DBG_PIN_TOGGLE();

	for (int i = 0; i < ARRAY_SIZE(bufs); i++) {
		free(bufs[i]);
	}

	return t;
}

struct manual_xfer_data {
	struct k_timer timer;
	nrfx_spim_xfer_desc_t desc;
	uint32_t xfer_cnt;
	bool do_sem;
};

static void timeout_handler(struct k_timer *timer)
{
	struct manual_xfer_data *data = k_timer_user_data_get(timer);

	nrfx_spim_xfer(&spim, &data->desc, 0);
	data->xfer_cnt--;
	if (data->xfer_cnt == 0) {
		k_timer_stop(timer);
		if (data->do_sem) {
			k_sem_give(&spi_sem);
		}
	}
}

static int manual_xfer(struct ppi_seq_generic_config *config, int *load)
{
	uint8_t *tx_buf = malloc(current_xfer_length);
	uint8_t *rx_buf = malloc(current_xfer_length);
	struct manual_xfer_data test_data;
	struct manual_xfer_data test_data2;
	nrfx_spim_xfer_desc_t desc = {
		.p_tx_buffer = tx_buf,
		.tx_length = sizeof(current_xfer_length),
		.p_rx_buffer = rx_buf,
		.rx_length = sizeof(current_xfer_length),
	};
	uint32_t total_time = config->period * config->batch_cnt * config->repeat;
	uint32_t t;
	int rv;

	k_timer_init(&test_data.timer, timeout_handler, NULL);
	k_timer_user_data_set(&test_data.timer, &test_data);

	test_data.desc = desc;
	if (current_second_xfer_offset) {
		test_data2.desc = desc;
		test_data2.xfer_cnt = (config->batch_cnt / 2) * config->repeat;
		test_data.xfer_cnt = (config->batch_cnt / 2) * config->repeat;
		test_data2.do_sem = true;
		test_data.do_sem = false;
		k_timer_init(&test_data2.timer, timeout_handler, NULL);
		k_timer_user_data_set(&test_data2.timer, &test_data2);
	} else {
		test_data.xfer_cnt = config->batch_cnt * config->repeat;
		test_data.do_sem = true;
	}

	for (int i = 0; i < current_xfer_length; i++) {
		tx_buf[i] = i;
	}

	k_sem_init(&spi_sem, 0, 1);
	DBG_PIN_TOGGLE();
	if (IS_ENABLED(CONFIG_CPU_LOAD)) {
		(void)cpu_load_get(true);
	}
	t = k_cycle_get_32();
	k_timer_start(&test_data.timer, K_USEC(config->period), K_USEC(config->period));
	if (current_second_xfer_offset) {
		k_timer_start(&test_data2.timer, K_USEC(config->period + current_second_xfer_offset),
						K_USEC(config->period));
	}

	rv = k_sem_take(&spi_sem, K_USEC(total_time + 10000));
	if (rv < 0) {
		return rv;
	}

	t = k_cycle_get_32() - t;
	if (IS_ENABLED(CONFIG_CPU_LOAD)) {
		*load = cpu_load_get(true);
	}
	DBG_PIN_TOGGLE();
	free(tx_buf);
	free(rx_buf);
	return t;
}

static int run(const struct shell *sh, struct ppi_seq_generic_config *config)
{
	int rv;
	int t_manual = 0, t_ppi = 0;
	int load = 0, load_ppi = 0;
	int total = config->repeat * config->batch_cnt;
	int pm_cnt;
	int line = 0;

	rv = spim_init();
	if (rv < 0) {
		shell_error(sh, "Failed to initialize SPIM");
		return rv;
	}

	if (current_second_xfer_offset != 0) {
		shell_print(sh, "Performing %d SPI transfers (engine callback every %d transfers)"
			" with %d us sleep period, 2 transfers per period (seconds after %d us)",
			total, config->batch_cnt, config->period, current_second_xfer_offset);
		total /= 2;
	} else {
		shell_print(sh, "Performing %d SPI transfers (engine callback every %d transfers)"
			" with %d us sleep period", total, config->batch_cnt, config->period);
	}
	shell_print(sh, "Transfers should take: %d us\n", (total - 1) * config->period);

	/* Ensure that all is printed as uart is forced to be off. */
	k_msleep(1);
	pm_cnt = stop_shell_uart(sh);
	k_msleep(100);

	rv = manual_xfer(config, &load);
	if (rv < 0) {
		line = __LINE__;
		goto finalize;
	}
	t_manual = rv;

	rv = initialize_single_xfer(sh, config);
	if (rv < 0) {
		line = __LINE__;
		goto finalize;
	}

	rv = perform_single_xfer_set(config, config->period * (total + 1) + 1000, &load_ppi);
	if (rv < 0) {
		line = __LINE__;
		goto finalize;
	}
	t_ppi = rv;
	uninitialize();

finalize:
	restart_shell_uart(sh, pm_cnt);

	if (rv >= 0) {
		shell_print(sh, "- Manual transfers:\n\tLOAD:%d.%d%%\n\ttook:%d us\n",
			load / 10, load % 10, t_manual);
		shell_print(sh, "- PPI Sequencer transfers:\n\tLOAD:%d.%d%%\n\ttook:%d us\n",
			load_ppi / 10, load_ppi % 10, t_ppi);
	} else if (rv == -ENOTSUP) {
		shell_error(sh, "Configuration not supported\n");
	} else {
		shell_error(sh, "Command failed with:%d line:%d\n", rv, line);
	}

	return rv;
}

static void print_status(const struct shell *sh)
{
	print_generic_status(sh);
	shell_print(sh, "\tTransfer length: %d", current_xfer_length);
	if (current_second_xfer_offset != 0) {
		shell_print(sh, "\tSecond transfer in the sequence after: %d us",
				current_second_xfer_offset);
	}
}

static int ppi_seq_status_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	print_status(sh);

	return 0;
}

static int ppi_seq_run_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	print_status(sh);

	return run(sh, get_generic_config());
}

static int config_xfer_length_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(sh);
	int rv = 0;

	current_xfer_length = shell_strtoul(argv[1], 0, &rv);

	return rv;
}

static int config_2nd_xfer_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(sh);
	int rv = 0;

	current_second_xfer_offset = shell_strtoul(argv[1], 0, &rv);

	return rv;
}

SHELL_SUBCMD_ADD((ppi_sequence, config), spi_second_xfer_offset, NULL,
		"Offset (in microseconds) for second transfer in the sequence "
		"(0 for single transfer per period)",
		config_2nd_xfer_handler, 2, 0);
SHELL_SUBCMD_ADD((ppi_sequence, config), spi_xfer_length, NULL, "Transfer length",
		config_xfer_length_handler, 2, 0);

SHELL_SUBCMD_ADD((ppi_sequence), spi_run, NULL, "Run SPI sequence using the current configuration",
		ppi_seq_run_handler, 1, 0);
SHELL_SUBCMD_ADD((ppi_sequence), spi_status, NULL, "Print current SPI configuration",
		ppi_seq_status_handler, 1, 0);
