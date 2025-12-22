/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef NRF_DRIVERS_PPI_SEQ_H__
#define NRF_DRIVERS_PPI_SEQ_H__

#include <string.h>
#include <nrfx_timer.h>
#include <helpers/nrfx_gppi.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ppi_seq PPI Sequencer
 * @brief Module for autonomously triggering periodic tasks.
 *
 * @{
 */

/* Forward declaration. */
struct ppi_seq_config;

/** @brief PPI sequencer structure. */
struct ppi_seq {
	/** A pointer to the configuration structure used during the initialization. */
	const struct ppi_seq_config *config;

	/** Timer instance which may be used for triggering tasks. */
	nrfx_timer_t timer;

	/** Pool of GPPI connection handles. */
	nrfx_gppi_handle_t ppi_pool[8];

	/** Internally used variable. */
	uint32_t repeat;

	/** Flag indicating that the sequencer is busy. */
	atomic_t in_use;

	/** GRTC channel used by the sequencer. */
	uint8_t grtc_chan;

	/** Number of GPPI connection handles. */
	uint8_t ppi_cnt;
};

/** @brief Callback called after completion of each cycle.
 *
 * @param ppi_seq Sequencer instance.
 * @param last True if this is the last callback after which the sequencer is stopped.
 */
typedef void (*ppi_seq_cb_t)(struct ppi_seq *ppi_seq, bool last);

/** @brief Type of notifier used for requested number of sequences completion. */
enum ppi_seq_notifier_type {
	/** k_timer callback is called when requested number of sequences is completed. */
	PPI_SEQ_NOTIFIER_SYS_TIMER,

	/** nrfx_timer as counter counts events and triggers after reaching a certain number.*/
	PPI_SEQ_NOTIFIER_NRFX_TIMER,
};

/** @brief System timer notifier structure. */
struct ppi_seq_notifier_sys_timer {
	struct k_timer timer;
	/* Length of the last operation in the sequence in microseconds. */
	uint16_t offset;
	/* Number of additional main operations in a cycle. For example if there
	 * are 2 SPI transfers in each cycle then it should be set to 1.
	 */
	uint16_t extra_main_ops;
};

/** @brief nrfx_timer notifier structure. */
struct ppi_seq_notifier_nrfx_timer {
	/* TIMER driver instance used for counting events. */
	nrfx_timer_t timer;
	/* Address of an EVENT register which should be used to count completion events. */
	uint32_t end_seq_event;
};

/** @brief Notifier structure. */
struct ppi_seq_notifier {
	enum ppi_seq_notifier_type type;
	union {
		struct ppi_seq_notifier_sys_timer sys_timer;
		struct ppi_seq_notifier_nrfx_timer nrfx_timer;
	};
};

/** @brief Operation descriptor. */
struct ppi_seq_op {
	/** Address of a TASK register which should be triggered by the sequencer. */
	uint32_t task;

	/** Delay relative to the start of the sequence (in microseconds). */
	uint32_t offset;
};

/** @brief Sequencer configuration structure. */
struct ppi_seq_config {
	/** Notifier. */
	struct ppi_seq_notifier *notifier;

	/** TIMER peripheral register. Can be NULL if sequencer does not use TIMER. */
	NRF_TIMER_Type *timer_reg;

	/** RTC peripheral register. Can be NULL if sequencer does not use RTC. */
	void *rtc;

	/** Callback called on completion of each cycle. */
	ppi_seq_cb_t callback;

	/** Period in microseconds on which sequence is repeated. */
	uint32_t period;

	/** Address of a TASK register that should be triggered on the start of the sequence. */
	uint32_t task;

	/** Extra operations in the sequence. */
	struct ppi_seq_op *extra_ops;

	/** Number of extra operations in the sequence. */
	size_t extra_ops_count;

	/** Flag indicating that GRTC should be used. */
	bool use_grtc;
};

#define PPI_SEQ_BASE_INITIALIZE(_task, _timer_reg, _rtc, _cb, _use_grtc, _period) \
	.timer_reg = _timer_reg, \
	.rtc = (void *)_rtc, \
	.callback = _cb, \
	.period = _period, \
	.use_grtc = _use_grtc \
	.task = _task

#define PPI_SEQ_NRFX_TIMER_COUNTER_CONFIG_DEFINE(name, _task, _timer_reg, \
					_rtc, _end_detect_timer_reg, _end_evt, \
					_cb, _use_grtc, _period) \
	static const struct ppi_seq_config name = { \
		PPI_SEQ_BASE_INITIALIZE(_task, _timer_reg, _rtc, _cb, _use_grtc, _period) \
		. end_detect = { \
			.nrfx_timer = { \
				.end_sequence_event = _end_evt,\
				.events_per_period = 1,\
			} \
		}, \
		.end_detect_timer_reg = _end_detect_timer_reg, \
	}

#define PPI_SEQ_SYS_TIMER_COUNTER_CONFIG_DEFINE(name, _task, _timer_reg, \
					_rtc, _seq_time, _cb, _use_grtc, _period) \
	static const struct ppi_seq_config name = { \
		PPI_SEQ_BASE_INITIALIZE(_task, _timer_reg, _rtc, _cb, _use_grtc, _period) \
		. end_detect = { \
			.sys_timer = { \
				.seq_time = _seq_time,\
			} \
		}, \
	}

/** @brief Initialize the sequencer.
 *
 * @param ppi_seq Sequencer instance.
 * @param config Sequencer configuration. Must be persistent as it is used by the sequencer.
 *
 * @retval 0 on successful initialization.
 * @retval negative on initialization failure.
 */
int ppi_seq_init(struct ppi_seq *ppi_seq, const struct ppi_seq_config *config);

/** @brief Uninitialize the sequencer.
 *
 * Function releases all resources.
 *
 * @param ppi_seq Sequencer instance.
 */
void ppi_seq_uninit(struct ppi_seq *ppi_seq);

/** @brief Start the sequencer.
 *
 * When started, sequence will be repeated requested number of timers and callback will be
 * called after @p main_evt_count events will occur. If sequence consist of a single operation
 * (e.g. single SPI transfer) then there will be callback after @p main_evt_count transfers and
 * sequencer will stop after @p repeat callbacks. Sequence can be forced to stop by
 * @ref ppi_seq_stop.
 *
 * For example, if sequence period is 1000 us and each sequence consists of two SPI transfers
 * then if @p main_evt_count is 16 and @p repeat is 100 then there will be a callback after
 * each 16 SPI transfers so 8 sequences (after 8000 us) and the last callback will be after
 * 800 ms.
 *
 * Sequence can be started immediately, together with a timer that manages the sequence. In that
 * case the distance between the first operation and the second may be shorter than the period
 * because the first operation is triggered manually. If @p immediate is false then start may be
 * delayed but operations are triggered synchronously.
 *
 * @param ppi_seq Sequencer instance.
 * @param main_evt_count Number of main events per callback.
 * @param repeat Number of callbacks after which the sequencer is stopped.
 * @param immediate If true then sequence is started immediately. If false then it is started
 * synchronously which may result in a delay.
 */
int ppi_seq_start(struct ppi_seq *ppi_seq, size_t main_evt_count, int repeat, bool immediate);

/** @brief Stop the sequencer.
 *
 * @param ppi_seq Sequencer instance.
 * @param immediate Sequence is stopped immediately if true. If false, sequence will be stopped
 * after the next callback.
 *
 * @retval 0 Operation is successful.
 * @retval -EALREADY Sequencer is already stopped.
 */
int ppi_seq_stop(struct ppi_seq *ppi_seq, bool immediate);

/** @brief Check if GRTC is used.
 *
 * @param ppi_seq Sequencer instance.
 *
 * @retval true GRTC is used for the sequence period.
 * @retval false GRTC is not used for the sequence period.
 */
static inline bool ppi_seq_with_grtc_used(struct ppi_seq *ppi_seq)
{
	return ppi_seq->grtc_chan != UINT8_MAX;
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* NRF_DRIVERS_PPI_SEQ_H__ */
