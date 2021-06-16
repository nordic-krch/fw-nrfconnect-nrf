/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef BUSY_SIM_H__
#define BUSY_SIM_H__

void busy_sim_start(uint32_t active_avg, uint32_t active_delta,
			  uint32_t idle_avg, uint32_t idle_delta);

void busy_sim_stop(void);

#endif /* BUSY_SIM_H__ */
