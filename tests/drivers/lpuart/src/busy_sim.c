/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <drivers/entropy.h>
#include <drivers/counter.h>
#include "busy_sim.h"
#include <hal/nrf_gpio.h>
#include <sys/ring_buffer.h>

struct busy_sim {
	uint32_t idle_avg;
	uint32_t active_avg;
	uint16_t idle_delta;
	uint16_t active_delta;
	uint32_t us_tick;
	const struct device *entropy;
	const struct device *counter;
	struct counter_alarm_cfg alarm_cfg;
	int8_t pin;
};

static struct busy_sim sim;

#define BUFFER_SIZE 32
RING_BUF_DECLARE(rbuf, BUFFER_SIZE);
static void rng_pool_work_handler(struct k_work *work)
{
	uint8_t *data;
	uint32_t len;

	len = ring_buf_put_claim(&rbuf, &data, BUFFER_SIZE - 1);
	if (len) {
		int err = entropy_get_entropy(sim.entropy, data, len);

		if (err == 0) {
			ring_buf_put_finish(&rbuf, len);
			return;
		}
	}

	k_work_submit(work);
}

static K_WORK_DEFINE(rng_pool_work, rng_pool_work_handler);

static uint32_t get_timeout(bool idle)
{
	uint32_t avg = idle ? sim.idle_avg : sim.active_avg;
	uint32_t delta = idle ? sim.idle_delta : sim.active_delta;
	uint16_t rand;
	uint32_t len;

	len = ring_buf_get(&rbuf, (uint8_t *)&rand, sizeof(rand));
	if (len < sizeof(rand)) {
		k_work_submit(&rng_pool_work);
	}

	avg *= sim.us_tick;
	delta *= sim.us_tick;

	return avg - delta + 2 * (rand % delta);
}


#define TEST_PIN -1//10
void counter_alarm_callback(const struct device *dev,
			    uint8_t chan_id, uint32_t ticks,
			    void *user_data)
{
	int err;

	sim.alarm_cfg.ticks = get_timeout(true);

	/* Busy loop */
	if (TEST_PIN >= 0) {
		nrf_gpio_pin_set(TEST_PIN);
	}

	k_busy_wait(get_timeout(false) / sim.us_tick);

	if (TEST_PIN >= 0) {
		nrf_gpio_pin_clear(TEST_PIN);
	}

	err = counter_set_channel_alarm(sim.counter, 0, &sim.alarm_cfg);
	__ASSERT_NO_MSG(err == 0);

}

void busy_sim_start(uint32_t active_avg, uint32_t active_delta,
		    uint32_t idle_avg, uint32_t idle_delta)
{
	int err;

	sim.active_avg = active_avg;
	sim.active_delta = active_delta;
	sim.idle_avg = idle_avg;
	sim.idle_delta = idle_delta;

	k_work_submit(&rng_pool_work);

	sim.alarm_cfg.ticks = counter_us_to_ticks(sim.counter, 100);
	err = counter_set_channel_alarm(sim.counter, 0, &sim.alarm_cfg);
	__ASSERT_NO_MSG(err == 0);

	err = counter_start(sim.counter);
	__ASSERT_NO_MSG(err == 0);
}

void busy_sim_stop(void)
{
	k_work_cancel(&rng_pool_work);

	int err = counter_stop(sim.counter);
	__ASSERT_NO_MSG(err == 0);
}

static int busy_sim_init(const struct device *unused)
{
	ARG_UNUSED(unused);
	uint32_t freq;

	if (TEST_PIN >= 0) {
		nrf_gpio_cfg_output(TEST_PIN);
	}

	sim.counter = DEVICE_DT_GET(DT_CHOSEN(zephyr_busy_sim_counter));
	if (!device_is_ready(sim.counter)) {
		return -EIO;
	}

	freq = counter_get_frequency(sim.counter);
	if (freq < 1000000) {
		return -EINVAL;
	}

	sim.us_tick = freq / 1000000;
	sim.alarm_cfg.callback = counter_alarm_callback;
	sim.alarm_cfg.flags = COUNTER_ALARM_CFG_EXPIRE_WHEN_LATE;

	sim.entropy = device_get_binding(DT_CHOSEN_ZEPHYR_ENTROPY_LABEL);
	if (!device_is_ready(sim.entropy)) {
		return -EIO;
	}

	return 0;
}

SYS_INIT(busy_sim_init, APPLICATION, 0);
