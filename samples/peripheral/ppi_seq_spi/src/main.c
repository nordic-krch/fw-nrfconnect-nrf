/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <stdlib.h>
#include <hal/nrf_gpio.h>
#include <ppi_seq/ppi_seq_i2c_spi.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app);

#define SPIM_NODE DT_NODELABEL(sample_spi)
#define TRANSFER_LENGTH 8
#define BATCH_LENGTH 16
#define PERIOD_US 5000
#define SPI_FREQ MHZ(8)

static uint8_t tx_buffer[TRANSFER_LENGTH];
static uint8_t rx_buffer[2][BATCH_LENGTH * TRANSFER_LENGTH];
static nrfx_spim_t spim;
static struct ppi_seq_i2c_spi seq;

static int spim_init(void)
{
	uint32_t ss_pin_idx = DT_GPIO_PIN(SPIM_NODE, cs_gpios);
	uint32_t ss_port = DT_PROP(DT_GPIO_CTLR(SPIM_NODE, cs_gpios), port);
	uint32_t ss_pin = NRF_GPIO_PIN_MAP(ss_port, ss_pin_idx);
	nrfx_spim_config_t config = {
		.ss_pin = NRF_SPIM_PIN_NOT_CONNECTED,
		.frequency = SPI_FREQ,
		.mode = NRF_SPIM_MODE_0,
		.use_hw_ss = true,
		.rx_delay = 1,
		.bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST,
		.skip_gpio_cfg = true,
		.skip_psel_cfg = true,
	};

	/* Get instance from the DT. */
	spim.p_reg = (NRF_SPIM_Type *)DT_REG_ADDR(SPIM_NODE);

	/* Manually configure HW SS in SPIM and GPIO. */
	nrf_gpio_pin_set(ss_pin);
	nrf_gpio_cfg_output(ss_pin);
	nrf_spim_csn_configure(spim.p_reg, ss_pin, NRF_SPIM_CSN_POL_LOW, 32);

	PINCTRL_DT_DEFINE(SPIM_NODE);
	(void)pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(SPIM_NODE), PINCTRL_STATE_DEFAULT);

	return nrfx_spim_init(&spim, &config, NULL, NULL);
}

static int seq_init(void)
{
	static struct ppi_seq_notifier notifier;
	static struct ppi_seq_config config;
	union ppi_seq_i2c_spi_dev dev;

	notifier.type = PPI_SEQ_NOTIFIER_SYS_TIMER;
	notifier.sys_timer.offset = (1000000 * TRANSFER_LENGTH) / (SPI_FREQ / 8);

	config.notifier = &notifier;
	config.callback = ppi_seq_i2c_spi_internal_cb;
	config.task = nrf_spim_task_address_get((NRF_SPIM_Type *)DT_REG_ADDR(SPIM_NODE),
				NRF_SPIM_TASK_START);

	dev.spim = &spim;

	return ppi_seq_i2c_spi_init(&seq, &config, dev, true);
}

static void ppi_seq_cb(struct ppi_seq_i2c_spi *seq, struct ppi_seq_i2c_spi_data *data, bool last)
{
	static uint32_t data_cnt;
	static uint32_t cb_cnt;

	data_cnt += (data->desc.spim.rx_length * seq->job.batch_cnt);
	cb_cnt++;

	if ((cb_cnt % 10) == 0) {
		LOG_INF("Batch callback:%d received bytes:%d", cb_cnt, data_cnt);
	}
}

static int seq_start(void)
{
	struct ppi_seq_i2c_spi_job job = {
		.desc = {
			.spim = {
				.p_tx_buffer = tx_buffer,
				.tx_length = TRANSFER_LENGTH,
				.p_rx_buffer = rx_buffer[0],
				.rx_length = TRANSFER_LENGTH,
			}
		},
		.rx_second_buf = rx_buffer[1],
		.repeat = UINT32_MAX,
		.batch_cnt = BATCH_LENGTH,
		.tx_postinc = false,
		.cb = ppi_seq_cb
	};

	return ppi_seq_i2c_spi_start(&seq, PERIOD_US, &job);
}

int main(void)
{
	int rv;

	printk("PPI Sequencer for SPI transfers\n");

	rv = spim_init();
	if (rv < 0) {
		LOG_ERR("SPIM initialization failed %d", rv);
	}

	rv = seq_init();
	if (rv < 0) {
		LOG_ERR("PPI sequencer initialization failed %d", rv);
	}

	rv = seq_start();
	if (rv < 0) {
		LOG_ERR("PPI sequencer start failed %d", rv);
	}

	return 0;
}
