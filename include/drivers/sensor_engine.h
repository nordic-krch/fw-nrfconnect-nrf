/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef NRF_DRIVERS_SENSOR_ENGINE_H__
#define NRF_DRIVERS_SENSOR_ENGINE_H__

#include <string.h>
#include <nrfx_spim.h>
#include <nrfx_timer.h>
#include <helpers/nrfx_gppi.h>
#include <zephyr/kernel.h>

struct sensor_engine_seq {
	uint8_t *tx_buf[2];
	uint8_t *rx_buf[2];
	size_t op_len;
	size_t op_cnt;
};

struct sensor_engine_data {
	uint8_t *tx_buf;
	uint8_t *rx_buf;
	size_t op_len;
	size_t op_cnt;
};

struct sensor_engine_config;

struct sensor_engine {
	const struct sensor_engine_config *config;
	struct sensor_engine_seq seq;
	struct k_timer timer;
	nrfx_gppi_handle_t ppi_spi_start;
	nrfx_gppi_handle_t ppi_cnt;
	nrfx_gppi_handle_t ppi_stop;
	nrfx_gppi_group_handle_t ppi_group;
	int32_t grtc_chan;
	uint32_t curr_idx;
	uint32_t k_timeout;
	bool stop_req;
};

typedef void (*sensor_engine_seq_cb_t)(struct sensor_engine *engine,
				struct sensor_engine_data *data, void *context);
typedef void (*sensor_engine_stop_cb_t)(struct sensor_engine *engine, void *context);

enum sensor_engine_mode {
	SENSOR_ENGINE_MODE_TIMER,
	SENSOR_ENGINE_MODE_RTC,
	SENSOR_ENGINE_MODE_GRTC,
};

struct sensor_engine_config {
	nrfx_spim_t *spi;
	union {
		nrfx_timer_t *trig_timer;
		void *rtc;
	};
	nrfx_timer_t *cnt_timer;
	void *context;
	sensor_engine_seq_cb_t callback;
	sensor_engine_stop_cb_t stop_callback;
	uint32_t period;
	uint32_t gap;
	uint32_t xfer_time;
	enum sensor_engine_mode mode;
	uint32_t flags;
};

int sensor_engine_init(struct sensor_engine *engine, const struct sensor_engine_config *config);
void sensor_engine_uninit(struct sensor_engine *engine);

int sensor_engine_start(struct sensor_engine *engine, struct sensor_engine_seq *seq, bool immediate);

int sensor_engine_stop(struct sensor_engine *engine, bool immediate);


#endif /* NRF_DRIVERS_SENSOR_ENGINE_H__ */
