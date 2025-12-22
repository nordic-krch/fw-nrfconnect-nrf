/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pinctrl.h>
#include <nrfx_spim.h>
#include <nrfx_spis.h>
#include <debug/cpu_load.h>
#include <nrf_sys_event.h>
#include <helpers/nrfx_gppi.h>
#include <drivers/sensor_engine.h>

LOG_MODULE_REGISTER(app);

#define SPIM_NODE DT_NODELABEL(dut_spi)
static nrfx_spim_t spim;

static struct k_sem spi_sem;
static void spim_event_handler(nrfx_spim_event_t const *event, void *context)
{
	k_sem_give((struct k_sem *)context);
	/*LOG_INF("SPIM event");*/
}

static int spim_init(nrfx_spim_t *instance)
{
	int rv;
	nrfx_spim_config_t config;
	bool active_high = true;

	instance->p_reg = (NRF_SPIM_Type *)DT_REG_ADDR(SPIM_NODE);

	PINCTRL_DT_DEFINE(SPIM_NODE);
	rv = pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(SPIM_NODE), PINCTRL_STATE_DEFAULT);
	if (rv < 0) {
		LOG_ERR("SPIM pinctrl setting failed");
		return rv;
	}

	uint32_t pin = DT_GPIO_PIN(SPIM_NODE, cs_gpios);
	/*uint32_t flags = DT_GPIO_FLAGS(SPIM_NODE, cs_gpios);*/
	uint32_t port = DT_PROP(DT_GPIO_CTLR(SPIM_NODE, cs_gpios), port);
	uint32_t cs_pin = 32 * port + pin;

	/* CS configure. */
	if (active_high) {
		nrf_gpio_pin_clear(cs_pin);
	} else {
		nrf_gpio_pin_set(cs_pin);
	}
	nrf_gpio_cfg_output(cs_pin);
	nrf_spim_csn_configure(instance->p_reg, cs_pin,
			active_high ? NRF_SPIM_CSN_POL_HIGH : NRF_SPIM_CSN_POL_LOW, 4);

	config.frequency = MHZ(8);
	config.mode = NRF_SPIM_MODE_0;
	config.use_hw_ss = true;
	config.ss_active_high = true;
	config.rx_delay = 1;
	config.bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST;
	config.skip_gpio_cfg = true;
	config.skip_psel_cfg = true;

	rv = nrfx_spim_init(instance, &config, spim_event_handler, &spi_sem);

	return rv;
}

static void sensor_engine_seq_cb(struct sensor_engine *engine, struct sensor_engine_data *data, void *context)
{
	k_sem_give((struct k_sem *)context);
}

static void sensor_engine_stop_cb(struct sensor_engine *engine, void *context)
{
}

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

#define USE_RTC 1
int main(void)
{
	int rv;
	static nrfx_timer_t rpt_timer;
	static nrfx_timer_t cnt_timer;
	static struct sensor_engine engine;
	static struct k_sem sem;
	struct sensor_engine_config engine_config = {
		.spi = &spim,
		COND_CODE_1(USE_RTC, (.rtc = (void *)0x50105000), (.rpt_timer = &rpt_timer)),
		.cnt_timer = &cnt_timer,
		.callback = sensor_engine_seq_cb,
		.stop_callback = sensor_engine_stop_cb,
		.context = &sem,
		.flags = USE_RTC ? SPI_ENGINE_USE_RTC : 0,
		.freq = MHZ(8),
	};
	int load;
	uint32_t gaps[] = {100000, 50000, 25000, 12000, 6000, 3000, 1000};

	printk("Sensor hub using SPI\n");

	IRQ_CONNECT(DT_IRQN(SPIM_NODE), DT_IRQ_BY_IDX(SPIM_NODE, 0, priority),
			nrfx_spim_irq_handler, &spim, 0);

	rv = spim_init(&spim);
	if (rv < 0) {
		LOG_ERR("SPIM init failed: %d", rv);
	}

	nrf_gpio_cfg_output(32+12);
	nrf_gpio_cfg_output(32+13);
	nrf_gpio_cfg_output(32+14);
#if 1

	/*spim.p_reg->ENABLE=7;*/
	/*NRF_SPIM30->ENABLE=7;*/


	NRF_TIMER21->MODE=1;
	/*NRF_TIMER21->TASKS_START=1;*/
	/*NRF_TIMER21->TASKS_COUNT=1;*/
	/*NRF_TIMER21->TASKS_COUNT=1;*/
	/*NRF_TIMER21->TASKS_COUNT=1;*/
	/*NRF_TIMER21->TASKS_COUNT=1;*/
	/*NRF_TIMER21->TASKS_CAPTURE[0]=1;*/
	/*printk("cc:%d\n", NRF_TIMER21->CC[0]);*/

	k_msleep(300);
	/*spim.p_reg->ENABLE=0;*/
	/*NRF_SPIM30->ENABLE=0;*/

	NRF_TIMER21->TASKS_STOP=1;
#endif
#define RPT_CNT 16
#define PKT_LEN 8
#define USE_SW 0

	static uint8_t tx_buf0[RPT_CNT][PKT_LEN];
	static uint8_t rx_buf0[RPT_CNT][PKT_LEN];
	struct sensor_engine_data spi_data = {
		.tx_buf = (uint8_t *)tx_buf0,
		.rx_buf = (uint8_t *)rx_buf0,
		.op_len = PKT_LEN,
		.op_cnt = RPT_CNT
	};

	if (USE_SW) {
#if 0
		*(uint32_t *)((uint32_t)NRF_RRAMC + 0x514) = 3;
		NRF_RRAMC->POWER.LOWPOWERCONFIG=1;
		NRF_GPIOTE20->CONFIG[0] = 3 | (12 <<4) | (1 << 9) | (3 << 16);
		nrfx_gppi_conn_alloc((uint32_t)&NRF_SPIM22->EVENTS_END,
			(uint32_t)&NRF_GPIOTE20->TASKS_OUT[0], &h);
		nrfx_gppi_conn_enable(h);
#endif

		nrfx_spim_xfer_desc_t desc = {
			.p_tx_buffer = tx_buf0[0],
			.tx_length = sizeof(tx_buf0[0]),
			.p_rx_buffer = rx_buf0[0],
			.rx_length = sizeof(rx_buf0[0]),
		};
		for (int j = 0; j < ARRAY_SIZE(gaps); j++) {
			k_sem_init(&spi_sem, 0, 1);
			for (int i = 0; i < RPT_CNT; i++) {

				rv = nrfx_spim_xfer(&spim, &desc, 0);
				if (rv < 0) {
					printk("EE spim %d\n", rv);
				}
				rv = k_sem_take(&spi_sem, K_MSEC(10));
				if (rv < 0) {
					printk("EE sem\n");
				}
				k_sleep(K_USEC(gaps[j]));
			}
			k_busy_wait(10000);
		}
		goto done;
	}

	rpt_timer.p_reg = NRF_TIMER22;
	cnt_timer.p_reg = NRF_TIMER21;
	IRQ_CONNECT(DT_IRQN(DT_NODELABEL(timer21)),
		    DT_IRQ_BY_IDX(DT_NODELABEL(timer21), 0, priority),
			nrfx_timer_irq_handler, &cnt_timer, 0);
	/*IRQ_CONNECT(DT_IRQN(DT_NODELABEL(timer20)),*/
		    /*DT_IRQ_BY_IDX(DT_NODELABEL(timer20), 0, priority),*/
			/*nrfx_timer_irq_handler, &rpt_timer, 0);*/


	for (int i = 0; i < RPT_CNT; i++) {
		tx_buf0[i][0] = 0x50 | BIT(7);
	}

	for (int j = 0; j < ARRAY_SIZE(gaps); j++) {
		k_sem_init(&sem, 0, 1);

		engine_config.period = gaps[j];
		rv = sensor_engine_init(&engine, &engine_config);
		if (rv < 0) {
			LOG_ERR("Failed to initialize the engine: rv:%d", rv);
		}

		if (IS_ENABLED(CONFIG_NRF_CPU_LOAD)) {
			cpu_load_reset();
		}
		rv = sensor_engine_single_start(&engine, &spi_data, gaps[j], false);
		if (rv < 0) {
			LOG_ERR("engine start failed");
		}

		rv = k_sem_take(&sem, K_USEC(gaps[j] * spi_data.op_cnt + 1000));
		if (rv < 0) {
			LOG_ERR("engine sequence not completed on time");
		}

		sensor_engine_uninit(&engine);
		k_busy_wait(10000);
	}

done:
	printk("DONE\n");
	if (IS_ENABLED(CONFIG_NRF_CPU_LOAD)) {
		load = cpu_load_get();
		printk("CPU load:%d.%d\n", load / 1000, load % 1000);
	}
	return 0;
}
