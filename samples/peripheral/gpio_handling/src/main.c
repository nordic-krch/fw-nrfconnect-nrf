/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <nrfx_gpiote.h>
#include <nrfx_egu.h>
#include <helpers/nrfx_gppi.h>

LOG_MODULE_REGISTER(app);

/* Sample requires that 0.4 and 0.26 pins are shortened. */
#define PIN_IN 26
#define PIN_OUT 4

static const struct device *gpio = DEVICE_DT_GET(DT_NODELABEL(gpio0));

struct gpio_data {
	struct gpio_callback gpio_cb;
	volatile uint32_t stamp;
};

static struct gpio_data data;
static uint32_t t_off;

void timer_init(void)
{
	NRF_TIMER0->BITMODE = 3;
	NRF_TIMER0->PRESCALER = 0;
	NRF_TIMER0->TASKS_START = 1;
}

uint32_t timer_now(void)
{
	NRF_TIMER0->TASKS_CAPTURE[0] = 1;
	return NRF_TIMER0->CC[0];
}

void report_diff(uint32_t n, uint32_t p)
{
	uint32_t d = n - p - t_off;
	uint32_t diff = (d * 100) / 16;
	LOG_INF("diff %d.%dus", diff / 100, diff % 100);
}

static void gpio_callback(const struct device *dev,
			  struct gpio_callback *gpio_cb, uint32_t pins)
{
	uint32_t now = timer_now();

	struct gpio_data *data = CONTAINER_OF(gpio_cb, struct gpio_data, gpio_cb);

	data->stamp = now;

	gpio_pin_interrupt_configure(dev, PIN_IN, GPIO_INT_DISABLE);
}

int gpio_int_setup(int mode)
{
	int err;

	err = gpio_pin_configure(gpio, PIN_IN, GPIO_INPUT);
	if (err < 0) {
		return err;
	}

	gpio_init_callback(&data.gpio_cb, gpio_callback, BIT(PIN_IN));

	err = gpio_add_callback(gpio, &data.gpio_cb);
	if (err < 0) {
		return err;
	}

	err = gpio_pin_interrupt_configure(gpio, PIN_IN, mode);
	if (err < 0) {
		return err;
	}

	return 0;
}

/* Using zephyr GPIO driver. Since handler must iterate over all registered callbacks,
 * measured value is the best case scenario. It will take longer if more callbacks are
 * registered and if multiple events will occur at once.
 */
int gpio_int_test(int mode)
{
	int err;

	nrf_gpio_cfg_output(PIN_OUT);
	nrf_gpio_pin_clear(PIN_OUT);

	data.stamp = 0;

	err = gpio_int_setup(mode);
	if (err < 0) {
		return err;
	}

	uint32_t now = timer_now();

	nrf_gpio_pin_set(PIN_OUT);

	k_busy_wait(1000);
	nrf_gpio_pin_clear(PIN_OUT);

	report_diff(data.stamp, now);

	return 0;
}

static void nrfx_gpiote_handler(nrfx_gpiote_pin_t     pin,
                                nrfx_gpiote_trigger_t trigger,
                                void *                p_context)
{
	uint32_t now = timer_now();
	volatile uint32_t *stamp = (volatile uint32_t *)p_context;

	*stamp = now;
	nrfx_gpiote_trigger_disable(pin);
}

/* Using nrfx_gpiote driver directly. It is faster than zephyr GPIO driver.
 * Measured time is the best case scenario as it can go up if more pins have
 * interrupt enabled and there multiple pins change state at once.
 */
int nrfx_gpiote_int_test(int edge)
{
	nrfx_gpiote_input_config_t input_config = NRFX_GPIOTE_DEFAULT_INPUT_CONFIG;
	uint8_t ch;
	volatile uint32_t stamp = 0;
	nrfx_err_t nerr;

	nerr = nrfx_gpiote_channel_alloc(&ch);
	if (nerr != NRFX_SUCCESS) {
		return -EIO;
	}

	nrfx_gpiote_trigger_config_t trigger_config;

	if (edge) {
		trigger_config.trigger = NRFX_GPIOTE_TRIGGER_LOTOHI;
		trigger_config.p_in_channel = &ch;
	} else {
		trigger_config.trigger = NRFX_GPIOTE_TRIGGER_HIGH;
		trigger_config.p_in_channel = NULL;
	}

	nrfx_gpiote_handler_config_t handler_config = {
		.handler = nrfx_gpiote_handler,
		.p_context = (void *)&stamp
	};

	nrf_gpio_cfg_output(PIN_OUT);
	nrf_gpio_pin_clear(PIN_OUT);

	data.stamp = 0;

	nerr = nrfx_gpiote_input_configure(PIN_IN, &input_config, &trigger_config, &handler_config);
	if (nerr != NRFX_SUCCESS) {
		return -EIO;
	}

	nrfx_gpiote_trigger_enable(PIN_IN, true);

	uint32_t now = timer_now();
	nrf_gpio_pin_set(PIN_OUT);

	k_busy_wait(1000);

	if (stamp == 0) {
		LOG_ERR("test failed");
	}
	report_diff(stamp, now);

	nrfx_gpiote_pin_uninit(PIN_IN);
	nrfx_gpiote_channel_free(ch);

	return 0;
}

static void egu_handler(uint8_t event_idx, void * p_context)
{
	uint32_t now = timer_now();
	volatile uint32_t *stamp = (volatile uint32_t *)p_context;

	*stamp = now;
	nrfx_gpiote_trigger_disable(PIN_IN);
}

/* Using GPIOTE with EGU and PPI to achieve the shortest reaction. Compare to other
 * methods, measured timing does not depend that much on other events that may occur
 * in the system. Especially, it does not depend on other pins (as nrfx_gpiote and
 * zephyr gpio driver).
 */
int egu_ppi_int_test(void)
{
	nrfx_err_t nerr;
	volatile uint32_t stamp = 0;
	nrfx_egu_t egu_inst = NRFX_EGU_INSTANCE(0);

	nerr = nrfx_egu_init(&egu_inst, 3, egu_handler, (void *)&stamp);
	if (nerr != NRFX_SUCCESS) {
		return -EIO;
	}

	/* Connect EGU0 nrfx handler */
	IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_EGU0), 3, nrfx_egu_0_irq_handler, 0);

	nrfx_egu_int_enable(&egu_inst, BIT(0));

	nrfx_gpiote_input_config_t input_config = NRFX_GPIOTE_DEFAULT_INPUT_CONFIG;
	uint8_t ch;

	nerr = nrfx_gpiote_channel_alloc(&ch);
	if (nerr != NRFX_SUCCESS) {
		return -EIO;
	}

	nrfx_gpiote_trigger_config_t trigger_config = {
		.trigger = NRFX_GPIOTE_TRIGGER_LOTOHI,
		.p_in_channel = &ch
	};

	nerr = nrfx_gpiote_input_configure(PIN_IN, &input_config, &trigger_config, NULL);
	if (nerr != NRFX_SUCCESS) {
		return -EIO;
	}

	/* After setting up GPIOTE event on PIN_IN use PPI to connect GPIOTE event
	 * with EGU task. If GPIOTE event will occur it will automatically trigger
	 * EGU interrupt. EGU interrupt has higher priority than GPIOTE and handles
	 * only one GPIO so it will be faster to handler. However, it can only be
	 * used with GPIOTE EVENT (not GPIO sense mechanism) which means that idle
	 * current can go down to ~20uA and not ~3uA (if sense mechanism is used).
	 */
	uint32_t egu_task = nrfx_egu_task_address_get(&egu_inst, NRF_EGU_TASK_TRIGGER0);
	uint32_t gpio_evt = nrfx_gpiote_in_event_address_get(PIN_IN);
	uint8_t ppi_ch;

	nerr = nrfx_gppi_channel_alloc(&ppi_ch);
	if (nerr != NRFX_SUCCESS) {
		return -EIO;
	}

	nrfx_gppi_channel_endpoints_setup(ppi_ch, gpio_evt, egu_task);
	nrfx_gppi_channels_enable(BIT(ppi_ch));

	nrfx_gpiote_trigger_enable(PIN_IN, false);

	nrf_gpio_cfg_output(PIN_OUT);
	nrf_gpio_pin_clear(PIN_OUT);

	data.stamp = 0;

	uint32_t now = timer_now();
	nrf_gpio_pin_set(PIN_OUT);

	k_busy_wait(1000);

	if (stamp == 0) {
		LOG_ERR("test failed");
	}
	report_diff(stamp, now);

	nrfx_gppi_channels_disable(BIT(ppi_ch));
	nrfx_gppi_channel_free(ppi_ch);
	nrfx_gpiote_pin_uninit(PIN_IN);
	nrfx_gpiote_channel_free(ch);

	return 0;
}

int main(void)
{
	int err;

	timer_init();

	nrf_gpio_cfg_output(PIN_OUT);
	nrf_gpio_pin_clear(PIN_OUT);

	t_off = timer_now();

	nrf_gpio_pin_set(PIN_OUT);
	t_off = timer_now() - t_off;

	nrf_gpio_pin_clear(PIN_OUT);

	LOG_INF("GPIO handling sample.");

	LOG_INF("Zephyr GPIO driver edge interrupt");
	err = gpio_int_test(GPIO_INT_EDGE_RISING);
	if (err < 0) {
		LOG_ERR("Test failed %d", err);
	}

	LOG_INF("Zephyr GPIO driver Level interrupt");
	err = gpio_int_test(GPIO_INT_LEVEL_HIGH);
	if (err < 0) {
		LOG_ERR("Test failed %d", err);
	}

	LOG_INF("nrfx_gpiote driver Level interrupt");
	err = nrfx_gpiote_int_test(false);
	if (err < 0) {
		LOG_ERR("Test failed %d", err);
	}

	LOG_INF("nrfx_gpiote driver edge interrupt");
	err = nrfx_gpiote_int_test(false);
	if (err < 0) {
		LOG_ERR("Test failed %d", err);
	}

	LOG_INF("nrfx_gpiote+nrfx_egu edge interrupt");
	err = egu_ppi_int_test();
	if (err < 0) {
		LOG_ERR("Test failed %d", err);
	}

	return 0;
}
