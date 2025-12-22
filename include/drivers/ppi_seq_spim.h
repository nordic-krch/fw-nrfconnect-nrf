/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef NRF_DRIVERS_PPI_SEQ_SPIM_H__
#define NRF_DRIVERS_PPI_SEQ_SPIM_H__

#include <string.h>
#include <nrfx_spim.h>
#include <drivers/ppi_seq.h>

struct ppi_seq_spim_data {
	uint8_t *tx_buf;
	uint8_t *rx_buf;
	size_t op_len;
	size_t op_cnt;
};

struct ppi_seq_spim;

typedef void (*ppi_seq_spim_cb_t)(struct ppi_seq_spim *spim_engine, struct ppi_seq_spim_data *data,
		bool last);

struct ppi_seq_spim_seq {
	uint8_t *tx_buf[2];
	uint8_t *rx_buf[2];
	size_t op_len;
	size_t op_cnt;
	size_t repeat;
	ppi_seq_spim_cb_t cb;
};

struct ppi_seq_spim {
	struct ppi_seq ppi_engine;
	struct ppi_seq_spim_seq seq;
	uint32_t idx;
	nrfx_spim_t *spim;
};

static inline void ppi_seq_spim_internal_cb(struct ppi_seq *ppi_seq, bool last)
{
	struct ppi_seq_spim *engine = CONTAINER_OF(ppi_seq, struct ppi_seq_spim, ppi_engine);
	uint32_t prev_idx = engine->idx;

	if (!last) {
		engine->idx = (engine->idx + 1) & 0x1;
		engine->spim->p_reg->DMA.TX.PTR = (uint32_t)engine->seq.tx_buf[engine->idx];
		engine->spim->p_reg->DMA.RX.PTR = (uint32_t)engine->seq.rx_buf[engine->idx];
	}

	struct ppi_seq_spim_data data = {
		.tx_buf = engine->seq.tx_buf[prev_idx],
		.rx_buf = engine->seq.rx_buf[prev_idx],
		.op_len = engine->seq.op_len,
		.op_cnt = engine->seq.op_cnt,
	};

	engine->seq.cb(engine, &data, last);
}

static inline int ppi_seq_spim_init(struct ppi_seq_spim *spim_engine,
				const struct ppi_seq_config *config)
{
	return ppi_seq_init(&spim_engine->ppi_engine, config);
}

static inline void ppi_seq_spim_uninit(struct ppi_seq_spim *spim_engine)
{
	ppi_seq_uninit(&spim_engine->ppi_engine);
}

static inline int ppi_seq_spim_start(struct ppi_seq_spim *engine,
					struct ppi_seq_spim_seq *seq, bool immediate)
{
	nrfx_spim_xfer_desc_t xfer_desc = {
		.p_tx_buffer = seq->tx_buf[0],
		.tx_length = seq->op_len,
		.p_rx_buffer = seq->rx_buf[0],
		.rx_length = seq->op_len,
	};
	static const uint32_t spi_flags = NRFX_SPIM_FLAG_TX_POSTINC |
			     NRFX_SPIM_FLAG_RX_POSTINC |
			     NRFX_SPIM_FLAG_NO_XFER_EVT_HANDLER |
			     NRFX_SPIM_FLAG_REPEATED_XFER |
			     NRFX_SPIM_FLAG_HOLD_XFER;
	int rv;

	engine->idx = 0;
	engine->seq = *seq;
	rv = nrfx_spim_xfer(engine->spim, &xfer_desc, spi_flags);
	if (rv < 0) {
		return rv;
	}

	rv = ppi_seq_start(&engine->ppi_engine, seq->op_cnt, seq->repeat, immediate);
	if (rv < 0) {
		return rv;
	}

	if (immediate && !ppi_seq_with_grtc_used(&engine->ppi_engine)) {
		*(uint32_t *)engine->ppi_engine.config->task = 1;
	}

	return 0;
}

static inline int ppi_seq_spim_stop(struct ppi_seq_spim *engine, bool immediate)
{
	return ppi_seq_stop(&engine->ppi_engine, immediate);
}

#endif /* NRF_DRIVERS_PPI_SEQ_SPIM_H__ */
