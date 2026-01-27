/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "common.h"
#include <string.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ppi_seq);

struct ppi_seq_generic_config current_config = {
	.mode = PPI_SEQ_SAMPLE_GRTC,
	.notifier = PPI_SEQ_NOTIFIER_SYS_TIMER,
	.period = 1000,
	.batch_cnt = 16,
	.repeat = 4,
};

int stop_shell_uart(const struct shell *sh)
{
	static const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
	int pm_cnt = 0;
	int rv;

	if (IS_ENABLED(CONFIG_PM_DEVICE_RUNTIME)) {
		enum pm_device_state state;

		/* Disable UART to reduce the current consumption. */
		do {
			rv = pm_device_state_get(uart, &state);
			if (rv < 0) {
				return rv;
			}
			if (state != PM_DEVICE_STATE_ACTIVE) {
				break;
			}
			pm_device_runtime_put(uart);
			pm_cnt++;
		} while (1);
	}

	return pm_cnt;
}

void restart_shell_uart(const struct shell *sh, int pm_cnt)
{
	static const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));

	if (IS_ENABLED(CONFIG_PM_DEVICE_RUNTIME)) {
		for (int i = 0; i < pm_cnt; i++) {
			pm_device_runtime_get(uart);
		}
	}
}

void print_generic_status(const struct shell *sh)
{
	shell_print(sh, "Current configuration:");
	shell_print(sh, "\tPeriod source method: %s",
		current_config.mode == PPI_SEQ_SAMPLE_RTC ? "RTC" :
		current_config.mode == PPI_SEQ_SAMPLE_GRTC ? "GRTC" : "TIMER");
	shell_print(sh, "\tBatch notifier: %s",
		current_config.notifier == PPI_SEQ_NOTIFIER_NRFX_TIMER ?
		"nrfx_timer" : "k_timer");
	shell_print(sh, "\tPeriod: %d us", current_config.period);
	shell_print(sh, "\tBatch count: %d", current_config.batch_cnt);
	shell_print(sh, "\tNumber of batches in the sequence: %d", current_config.repeat);
}

static int ppi_seq_status_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	print_generic_status(sh);
	return 0;
}

static int config_mode_grtc_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	ARG_UNUSED(sh);
	current_config.mode = PPI_SEQ_SAMPLE_GRTC;
	return 0;
}

static int config_mode_rtc_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	ARG_UNUSED(sh);
	current_config.mode = PPI_SEQ_SAMPLE_RTC;
	return 0;
}

static int config_mode_timer_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	ARG_UNUSED(sh);
	current_config.mode = PPI_SEQ_SAMPLE_TIMER;
	return 0;
}

static int config_notifier_k_timer_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	ARG_UNUSED(sh);
	current_config.notifier = PPI_SEQ_NOTIFIER_SYS_TIMER;
	return 0;
}

static int config_notifier_nrfx_timer_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	ARG_UNUSED(sh);
	current_config.notifier = PPI_SEQ_NOTIFIER_NRFX_TIMER;
	return 0;
}

static int config_period_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(sh);
	int rv = 0;

	current_config.period = shell_strtoul(argv[1], 0, &rv);

	return rv;
}

static int config_batch_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(sh);
	int rv = 0;

	current_config.batch_cnt = shell_strtoul(argv[1], 0, &rv);

	return rv;
}

static int config_repeat_handler(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(sh);
	int rv = 0;

	current_config.repeat = shell_strtoul(argv[1], 0, &rv);

	return rv;
}

struct ppi_seq_generic_config *get_generic_config(void)
{
	return &current_config;
}

SHELL_SUBCMD_ADD((ppi_sequence, config), repeat, NULL, "Number of batches in the sequence",
		config_repeat_handler, 2, 0);
SHELL_SUBCMD_ADD((ppi_sequence, config), batch_count, NULL, "Number of periods in a batch",
		config_batch_handler, 2, 0);
SHELL_SUBCMD_ADD((ppi_sequence, config), period, NULL, "Period (in microseconds)",
		config_period_handler, 2, 0);

SHELL_SUBCMD_ADD((ppi_sequence, config, notifier), k_timer, NULL,
		"Configure k_timer notifier\n", config_notifier_k_timer_handler, 1, 0);
SHELL_SUBCMD_ADD((ppi_sequence, config, notifier), nrfx_timer, NULL,
		"Configure nrfx_timer notifier\n", config_notifier_nrfx_timer_handler, 1, 0);
SHELL_SUBCMD_SET_CREATE(config_notifier_cmd, (ppi_sequence, config, notifier));
SHELL_SUBCMD_ADD((ppi_sequence, config), notifier, &config_notifier_cmd, "Notifier method",
		NULL, 1, 0);

SHELL_SUBCMD_ADD((ppi_sequence, config, mode), grtc, NULL,
		"Configure RTC mode\n", config_mode_grtc_handler, 1, 0);
SHELL_SUBCMD_ADD((ppi_sequence, config, mode), rtc, NULL,
		"Configure RTC mode\n", config_mode_rtc_handler, 1, 0);
SHELL_SUBCMD_ADD((ppi_sequence, config, mode), timer, NULL,
		"Configure TIMER mode\n", config_mode_timer_handler, 1, 0);
SHELL_SUBCMD_SET_CREATE(config_mode_cmd, (ppi_sequence, config, mode));
SHELL_SUBCMD_ADD((ppi_sequence, config), mode, &config_mode_cmd, "Peripheral used for period",
		NULL, 1, 0);
SHELL_SUBCMD_SET_CREATE(config_cmds, (ppi_sequence, config));
SHELL_SUBCMD_ADD((ppi_sequence), config, &config_cmds,
		"Print configuration (no arguments) or configure specific parameter",
		ppi_seq_status_handler, 1, 0);
SHELL_SUBCMD_SET_CREATE(ppi_sequence_cmd, (ppi_sequence));
SHELL_CMD_REGISTER(ppi_sequence, &ppi_sequence_cmd,
		   "SPI/SAADC operations using software and PPI sequencer", NULL);

int main(void)
{
	printk("PPI Sequencer for SPI transfers\n");

	DBG_PIN_CFG();

	return 0;
}
