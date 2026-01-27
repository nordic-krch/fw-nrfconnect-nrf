/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef COMMON_H__
#define COMMON_H__

#include <hal/nrf_gpio.h>
#include <drivers/ppi_seq.h>
#include <zephyr/shell/shell.h>
#include <zephyr/debug/cpu_load.h>
#include <zephyr/logging/log.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USE_DBG_PIN 1

#if USE_DBG_PIN
#define DBG_PIN (2 * 32 + 10)
#define DBG_PIN_TOGGLE() do { \
	nrf_gpio_pin_set(DBG_PIN); \
	k_busy_wait(10); \
	nrf_gpio_pin_clear(DBG_PIN); \
} while (0)

#define DBG_PIN_CFG() nrf_gpio_cfg_output(DBG_PIN)
#else
#define DBG_PIN_TOGGLE()
#define DBG_PIN_CFG()
#endif

enum ppi_seq_sample_mode {
	PPI_SEQ_SAMPLE_GRTC,
	PPI_SEQ_SAMPLE_RTC,
	PPI_SEQ_SAMPLE_TIMER,
};

struct ppi_seq_generic_config {
	enum ppi_seq_sample_mode mode;
	enum ppi_seq_notifier_type notifier;
	uint32_t period;
	uint32_t repeat;
	uint32_t batch_cnt;
};

int stop_shell_uart(const struct shell *sh);
void restart_shell_uart(const struct shell *sh, int pm_cnt);
struct ppi_seq_generic_config *get_generic_config(void);
void print_generic_status(const struct shell *sh);

#ifdef __cplusplus
}
#endif


#endif /* COMMON_H__ */
