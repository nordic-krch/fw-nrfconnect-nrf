/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef NRF_PPI_SEQ_SPIM_H__
#define NRF_PPI_SEQ_SPIM_H__

#include <string.h>
#include <nrfx_spim.h>
#include <nrfx_twim.h>
#include <ppi_seq/ppi_seq.h>

/** @brief Data passed to the callback on the batch completion. */
struct ppi_seq_i2c_spi_data {
	/** Transfer details. RX and TX (if TX postincrementation was used) buffers
	 * contain data for all transfers executed in the batch.
	 */
	union {
		nrfx_spim_xfer_desc_t spim;
		nrfx_twim_xfer_desc_t twim;
	} desc;

	/** Number of completed batches. */
	uint8_t batch_cnt;

	/** Number of extra transfers in each period. Basic setup has one transfer per period. */
	uint8_t extra_xfers;
};

/* Forward declaration. */
struct ppi_seq_i2c_spi;

/** @brief Callback called on batch completion.
 *
 * @param seq  I2C/SPI PPI sequencer instance.
 * @param data Structure with data for the completed batch.
 * @param last True to indicate that it is the last callback and sequencer will be stopped.
 */
typedef void (*ppi_seq_i2c_spi_cb_t)(struct ppi_seq_i2c_spi *seq, struct ppi_seq_i2c_spi_data *data,
		bool last);

/** @brief I2C/SPI PPI Sequencer job description.
 *
 * Descriptor provides pair of RX and TX buffers that will be used alternately for a batch.
 * First pair of buffers is provided in the standard nrfx_spim transfer descriptor.
 * RX buffer must be allocated for batch_cnt * (extra_xfers + 1) transfers.
 * TX buffer must have filled data for batch_cnt * (extra_xfers + 1) transfers if postincrementation
 * is used.
 */
struct ppi_seq_i2c_spi_job {
	/** Transfer descriptor. Specifies RX and TX length and provides first pair of buffers. */
	union {
		nrfx_spim_xfer_desc_t spim;
		nrfx_twim_xfer_desc_t twim;
	} desc;

	/** Second TX buffer. Can be NULL if TX postincrementation is not used. */
	uint8_t *tx_second_buf;

	/** Second RX buffer. */
	uint8_t *rx_second_buf;

	/** Number of times that batch shall be repeated. UINT32_MAX will continue until stopped. */
	size_t repeat;

	/** Number of periods after which there will be a callback and buffer switching. */
	uint8_t batch_cnt;

	/** Number of extra transfers in a period. */
	uint8_t extra_xfers;

	/** True to use TX postincrementation. It is needed if different data need to be transfered
	 * in each transfer.
	 */
	bool tx_postinc;

	/** Callback. */
	ppi_seq_i2c_spi_cb_t cb;
};

union ppi_seq_i2c_spi_dev {
	nrfx_spim_t *spim;
	nrfx_twim_t *twim;
};

/** I2C/SPI PPI Sequencer structure. */
struct ppi_seq_i2c_spi {
	/** PPI sequencer. */
	struct ppi_seq ppi_seq;

	/** Placeholder for storing the current job. */
	struct ppi_seq_i2c_spi_job job;

	/** Pointer to the SPIM/TWIM driver instance. Need to be initialize outside of
	 * that module.
	 */
	union ppi_seq_i2c_spi_dev dev;

	/** Flag for internal use. */
	bool primary_buf;

	/** Flag indicating that SPI is used. */
	bool spi;
};

/** @brief Callback that need to be used for configuring the PPI sequencer. */
static inline void ppi_seq_i2c_spi_internal_cb(struct ppi_seq *ppi_seq, bool last)
{
	struct ppi_seq_i2c_spi *seq = CONTAINER_OF(ppi_seq, struct ppi_seq_i2c_spi, ppi_seq);
	struct ppi_seq_i2c_spi_job *job = &seq->job;
	struct ppi_seq_i2c_spi_data data;

	data.batch_cnt = job->batch_cnt;
	data.extra_xfers = job->extra_xfers;
	if (IS_ENABLED(CONFIG_PPI_SEQ_SPIM) && (seq->spi == true)) {
		data.desc.spim = job->desc.spim;
		if (seq->primary_buf == false) {
			if (job->tx_postinc) {
				data.desc.spim.p_tx_buffer = job->tx_second_buf;
			}
			data.desc.spim.p_rx_buffer = job->rx_second_buf;
		}
		if (last == false) {
			seq->dev.spim->p_reg->DMA.RX.PTR = (uint32_t)(seq->primary_buf ?
					job->rx_second_buf : job->desc.spim.p_rx_buffer);
			if (job->tx_postinc) {
				seq->dev.spim->p_reg->DMA.TX.PTR = (uint32_t)(seq->primary_buf ?
					job->tx_second_buf : job->desc.spim.p_tx_buffer);
			}
		}
	} else if (IS_ENABLED(CONFIG_PPI_SEQ_TWIM)) {
		data.desc.twim = job->desc.twim;
		if (seq->primary_buf == false) {
			if (job->tx_postinc) {
				data.desc.twim.p_primary_buf = job->tx_second_buf;
			}
			data.desc.twim.p_secondary_buf = job->rx_second_buf;
		}
		if (last == false) {
			seq->dev.twim->p_twim->DMA.RX.PTR = (uint32_t)(seq->primary_buf ?
				job->rx_second_buf : job->desc.twim.p_secondary_buf);
			if (job->tx_postinc) {
				seq->dev.twim->p_twim->DMA.TX.PTR = (uint32_t)(seq->primary_buf ?
					job->tx_second_buf : job->desc.twim.p_primary_buf);
			}
		}
	}

	seq->primary_buf = !seq->primary_buf;
	seq->job.cb(seq, &data, last);
}

/** @brief Initialize the I2C/SPI PPI sequencer.
 *
 * @param seq I2C/SPI PPI sequencer.
 * @param config Configuration structure for the PPI sequencer.
 * @param use_spi If true sequencer is used with SPIM driver. If false TWIM driver is used.
 *
 * @retval 0 Successful initialization.
 * @retval negative Initialization failed.
 */
static inline int ppi_seq_i2c_spi_init(struct ppi_seq_i2c_spi *seq,
				const struct ppi_seq_config *config,
				union ppi_seq_i2c_spi_dev dev, bool use_spi)
{
	if (!IS_ENABLED(CONFIG_PPI_SEQ_SPIM) && (use_spi == true)) {
		return -ENOTSUP;
	}

	if (!IS_ENABLED(CONFIG_PPI_SEQ_TWIM) && (use_spi == false)) {
		return -ENOTSUP;
	}

	seq->dev = dev;
	seq->spi = use_spi;

	return ppi_seq_init(&seq->ppi_seq, config);
}

/** @brief Initialize the I2C/SPI PPI sequencer.
 *
 * @param seq I2C/SPI PPI sequencer.
 */
static inline void ppi_seq_i2c_spi_uninit(struct ppi_seq_i2c_spi *seq)
{
	ppi_seq_uninit(&seq->ppi_seq);
}

/** @brief Start the I2C/SPI PPI sequencer.
 *
 * @param seq I2C/SPI PPI sequencer.
 * @param period Period (in microseconds).
 * @param job Job descriptor.
 *
 * @retval 0 Successful start.
 * @retval negative Initialization failed.
 */
static inline int ppi_seq_i2c_spi_start(struct ppi_seq_i2c_spi *seq,
				     size_t period,
				     struct ppi_seq_i2c_spi_job *job)
{
	int rv;

	seq->primary_buf = true;
	seq->job = *job;
	if (IS_ENABLED(CONFIG_PPI_SEQ_SPIM) && (seq->spi == true)) {
		uint32_t flags = (job->tx_postinc ? NRFX_SPIM_FLAG_TX_POSTINC : 0) |
				     NRFX_SPIM_FLAG_RX_POSTINC |
				     NRFX_SPIM_FLAG_NO_XFER_EVT_HANDLER |
				     NRFX_SPIM_FLAG_REPEATED_XFER |
				     NRFX_SPIM_FLAG_HOLD_XFER;

		rv = nrfx_spim_xfer(seq->dev.spim, &seq->job.desc.spim, flags);
		if (rv < 0) {
			return rv;
		}
	} else if (IS_ENABLED(CONFIG_PPI_SEQ_TWIM)) {
		uint32_t flags = (job->tx_postinc ? NRFX_TWIM_FLAG_TX_POSTINC : 0) |
				     NRFX_TWIM_FLAG_RX_POSTINC |
				     NRFX_TWIM_FLAG_NO_XFER_EVT_HANDLER |
				     NRFX_TWIM_FLAG_REPEATED_XFER |
				     NRFX_TWIM_FLAG_HOLD_XFER;

		rv = nrfx_twim_xfer(seq->dev.twim, &job->desc.twim, flags);
		if (rv < 0) {
			return rv;
		}
	}

	return ppi_seq_start(&seq->ppi_seq, period, job->batch_cnt, job->repeat);
}

/** @brief Stop the I2C/SPI PPI sequencer.
 *
 * @param seq I2C/SPI PPI sequencer.
 * @param immediate If true sequencer is stopped immediately.
 *
 * @retval 0 Successful stop.
 * @retval negative Initialization failed.
 */
static inline int ppi_seq_i2c_spi_stop(struct ppi_seq_i2c_spi *seq, bool immediate)
{
	return ppi_seq_stop(&seq->ppi_seq, immediate);
}

#endif /* NRF_PPI_SEQ_SPIM_H__ */
