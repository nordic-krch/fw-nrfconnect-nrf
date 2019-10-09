/*
 * Copyright (c) 2019 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#ifndef LOG_BACKEND_TZ_S_H__
#define LOG_BACKEND_TZ_S_H__

#include <zephyr.h>
#include <logging/log_ctrl.h>

/** @brief Get number of domains registered to log_backend.
 *
 * @return Number of domains.
 */
u32_t log_backend_tz_s_get_domain_count(void);

/** @brief Get number of sources in domain.
 *
 * @param domain_id Domain ID.
 *
 * @return Number of sources.
 */
u32_t log_backend_tz_s_get_source_count(u32_t domain_id);

/** @brief Get domain name.
 *
 * If buffer is smaller than actual name then name is trimmed.
 *
 * @param[in] domain_id	Domain ID.
 * @param[out] name	Buffer where name is saved. If NULL only length is read.
 * @param[in,out] len	Buffer size as input, name length as output.
 */
void log_backend_tz_s_get_domain_name(u32_t domain_id, char * name, u32_t *len);

/** @brief Get source name.
 *
 * If buffer is smaller than actual name then name is trimmed.
 *
 * @param[in] domain_id	Domain ID.
 * @param[in] src_id	Source ID.
 * @param[out] name	Buffer where name is saved.
 * @param[in] len	Buffer size.
 */
void log_backend_tz_s_get_source_name(u32_t domain_id, u32_t src_id,
					char * name, u32_t len);

/** @brief Get compiled level of the source.
 *
 * @param[in] domain_id	Domain ID.
 * @param[in] src_id	Source ID.
 * @param[out] level	Location to save level.
 */
void log_backend_tz_s_get_compiled_level(u32_t domain_id, u32_t src_id,
					 u8_t * level);

/** @brief Set runtime level of the source.
 *
 * @param domain_id	Domain ID.
 * @param src_id	Source ID.
 * @param level		Level.
 */
void log_backend_tz_s_set_runtime_level(u32_t domain_id, u32_t src_id,
					u8_t level);

/** @brief Prototype of notify callback. */
typedef void (*log_backend_tz_s_notify_clbk_t)(void);

/** @brief Enable backend.
 *
 * @param new_log_clbk	Callback notifying about new log message pending.
 * @param timestamp_req	Callback called when timestamp is requested.
 */
void log_backend_tz_s_enable(log_backend_tz_s_notify_clbk_t new_log_clbk,
				log_backend_tz_s_notify_clbk_t timestamp_req);

/** @brief Set current timestamp.
 *
 * Secure part is using non-secure timestamp source for own timestamps. There
 * is a special mechanism of requesting current timestamp. On that request
 * non-secure application responds with current timestamp value.
 *
 * @param new_timestamp	Current timestamp value.
 */
void log_backend_tz_s_timestamp_update(u32_t new_timestamp);

/** @brief Get a copy of next chunk of log message.
 *
 * Log messages consists of one or more chunks. Non-secure part shall request
 * chunk by chunk until notifies that there is no more chunks to copy.
 *
 * Note that since chunks are changing address space, pointers to next chunk is
 * no longer valid and need to be updated with address of chunks allocated on
 * non-secure side.
 *
 * @param[out] chunk	Buffer where next chunk can be copied.
 *
 * @return True is there is more chunks in the message, false otherwise.
 */
bool log_backend_tz_s_get_msg(union log_msg_chunk *chunk);

/** @brief Check if there is any log message pending in the backend.
 *
 * @return True if there are no message, false otherwise.
 */
bool log_backend_tz_s_no_msg(void);

#endif /* LOG_BACKEND_TZ_S_H__ */
