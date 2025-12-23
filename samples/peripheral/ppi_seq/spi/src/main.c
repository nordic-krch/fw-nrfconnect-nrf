/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <debug/cpu_load.h>
#include <drivers/ppi_seq.h>
#include <drivers/ppi_seq_spim.h>
#include <zephyr/drivers/pinctrl.h>
#include <nrfx_gpiote.h>
#include <hal/nrf_gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio/gpio_nrf.h>
#include <zephyr/shell/shell.h>
LOG_MODULE_REGISTER(ppi_seq_spi);

#define SPI_FREQ MHZ(8)

#define SPIM_NODE DT_NODELABEL(sample_spi)
static nrfx_spim_t spim;
static NRF_TIMER_Type *trig_timer = (NRF_TIMER_Type *)DT_REG_ADDR(DT_NODELABEL(trig_timer));
static struct ppi_seq_spim seq_spim;
static struct ppi_seq_config seq_config;
static struct ppi_seq_notifier notifier;
static struct k_sem spi_sem;

static void spim_event_handler(nrfx_spim_event_t const *event, void *context)
{
	k_sem_give((struct k_sem *)context);
}

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
	nrf_spim_csn_configure(spim.p_reg, ss_pin, NRF_SPIM_CSN_POL_LOW, 32);

	config.ss_pin = NRF_SPIM_PIN_NOT_CONNECTED;
	config.frequency = SPI_FREQ;
	config.mode = NRF_SPIM_MODE_0;
	config.use_hw_ss = true;
	config.rx_delay = 1;
	config.bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST;
	config.skip_gpio_cfg = true;
	config.skip_psel_cfg = true;

	rv = nrfx_spim_init(&spim, &config, spim_event_handler, &spi_sem);
	if (rv < 0) {
		LOG_ERR("Failed to initialize nrfx_spim instance %d", rv);
	}

	return rv;
}

static int initialize_single_xfer(enum ppi_seq_notifier_type notifier_type,
				  bool use_timer, uint32_t period, uint32_t xfer_length)
{
	notifier.type = notifier_type;
	if (notifier_type == PPI_SEQ_NOTIFIER_SYS_TIMER) {
		notifier.sys_timer.offset = (1000000 * xfer_length) / (SPI_FREQ / 8) + 10;
		notifier.sys_timer.extra_main_ops = 0;
	} else {
		notifier.nrfx_timer.timer.p_reg =
			(NRF_TIMER_Type *)DT_REG_ADDR(DT_NODELABEL(cnt_timer));
		notifier.nrfx_timer.end_seq_event = nrfx_spim_end_event_address_get(&spim);
	}

	seq_config.notifier = &notifier;
	seq_config.timer_reg = use_timer ? trig_timer : NULL;
	seq_config.callback = ppi_seq_spim_internal_cb;
	seq_config.period = period;
	seq_config.task = nrfx_spim_start_task_address_get(&spim);
	seq_config.extra_ops = NULL;
	seq_config.extra_ops_count = 0;

	return ppi_seq_spim_init(&seq_spim, &seq_config);
}

static void single_xfer(enum ppi_seq_notifier_type notifier_type, bool use_timer,
		uint32_t period, uint32_t xfer_length, uint32_t cb_count, uint32_t repeat)
{
	uint8_t *tx_buf = malloc(xfer_length);
	uint8_t *rx_buf = malloc(xfer_length);
	nrfx_spim_xfer_desc_t desc = {
		.p_tx_buffer = tx_buf,
		.tx_length = sizeof(xfer_length),
		.p_rx_buffer = rx_buf,
		.rx_length = sizeof(xfer_length),
	};
	uint32_t t;
	int load;
	int rv;

	printk("Running set of SPI transfers using software loop\n");
	k_sem_init(&spi_sem, 0, 1);
	if (IS_ENABLED(CONFIG_NRF_CPU_LOAD)) {
		cpu_load_reset();
	}
	t = k_cycle_get_32();
	for (int i = 0; i < cb_count * repeat; i++) {
		rv = nrfx_spim_xfer(&spim, &desc, 0);
		if (rv < 0) {
			printk("SPIM transfer failed: %d\n", rv);
			return;
		}
		rv = k_sem_take(&spi_sem, K_MSEC(10));
		if (rv < 0) {
			printk("Failed to complete the SPIM transfer: %d\n", rv);
			return;
		}
		k_sleep(K_USEC(period));
	}
	t = k_cycle_get_32() - t;
	load = cpu_load_get();
	free(tx_buf);
	free(rx_buf);
	printk("Performed %d SPI transfers with %d us sleep period. LOAD:%d.%d "
		"took:%d us (should take:%d us)\n",
		cb_count * repeat, period, load / 1000, load % 1000, t,
		period * cb_count * repeat);
}

static int single_cmd_handler(const struct shell *sh, size_t argc, char **argv,
			      enum ppi_seq_notifier_type notifier_type, bool use_timer)
{
	int rv = 0;
	uint32_t period, xfer_length, cb_count, repeat;

	period = shell_strtoul(argv[1], 0, &rv);
	if (rv < 0) {
		return rv;
	}
	xfer_length = shell_strtoul(argv[2], 0, &rv);
	if (rv < 0) {
		return rv;
	}
	cb_count = shell_strtoul(argv[3], 0, &rv);
	if (rv < 0) {
		return rv;
	}
	repeat = shell_strtoul(argv[4], 0, &rv);
	if (rv < 0) {
		return rv;
	}

	single_xfer(notifier_type, use_timer, period, xfer_length, cb_count, repeat);

	return 0;
}

static int single_timer_sys_timer_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);

	return single_cmd_handler(sh, argc, argv, PPI_SEQ_NOTIFIER_SYS_TIMER, true);
}

static int single_timer_nrfx_timer_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);

	return single_cmd_handler(sh, argc, argv, PPI_SEQ_NOTIFIER_NRFX_TIMER, true);
}

static int single_grtc_sys_timer_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);

	return single_cmd_handler(sh, argc, argv, PPI_SEQ_NOTIFIER_SYS_TIMER, false);
}

static int single_grtc_nrfx_timer_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);

	return single_cmd_handler(sh, argc, argv, PPI_SEQ_NOTIFIER_NRFX_TIMER, false);
}

#define HELP_COMMON "4 arguments are needed: <period> <xfer_length> <callback count> <repeat>\n" \
		" - period - in microseconds a distance between each transfer\n" \
		" - xfer_length - number of bytes in the transfer\n" \
		" - callback_count - after how many transfers callback is called\n" \
		" - repeat - how many callbacks before stopping the sequencer\n"

SHELL_SUBCMD_ADD((spi_sequence, single, timer), sys_timer, NULL,
		"System timer used for the notifier\n" HELP_COMMON,
		single_timer_sys_timer_handler, 5, 0);
SHELL_SUBCMD_ADD((spi_sequence, single, timer), nrfx_timer, NULL,
		"nrfx timer in counter mode used for the notifier\n" HELP_COMMON,
		single_timer_nrfx_timer_handler, 5, 0);

SHELL_SUBCMD_ADD((spi_sequence, single, grtc), sys_timer, NULL,
		"System timer used for the notifier\n" HELP_COMMON,
		single_grtc_sys_timer_handler, 5, 0);
SHELL_SUBCMD_ADD((spi_sequence, single, grtc), nrfx_timer, NULL,
		"nrfx timer in counter mode used for the notifier\n" HELP_COMMON,
		single_grtc_nrfx_timer_handler, 5, 0);

SHELL_SUBCMD_SET_CREATE(single_grtc_cmd, (spi_sequence, single, grtc));
SHELL_SUBCMD_SET_CREATE(single_timer_cmd, (spi_sequence, single, timer));

SHELL_SUBCMD_ADD((spi_sequence, single), grtc, &single_grtc_cmd, "GRTC used for triggering",
		NULL, 1, 0);
SHELL_SUBCMD_ADD((spi_sequence, single), timer, &single_timer_cmd, "TIMER used for triggering",
		NULL, 1, 0);

SHELL_SUBCMD_SET_CREATE(single_seq_cmd, (spi_sequence, single));

SHELL_SUBCMD_ADD((spi_sequence), single, &single_seq_cmd, "Single SPI transfer in a sequence",
		NULL, 1, 0);
SHELL_SUBCMD_SET_CREATE(spi_sequence_cmd, (spi_sequence));
SHELL_CMD_REGISTER(spi_sequence, &spi_sequence_cmd,
		   "Demo command using section for subcommand registration", NULL);

int main(void)
{
	printk("PPI Sequencer for SPI transfers\n");

	if(spim_init() < 0) {
		printk("SPI driver initialization failed\n");
		return 0;
	}
}
