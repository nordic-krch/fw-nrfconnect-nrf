/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <logging/log_link.h>
#include <logging/log_backend_tz_s.h>
#include <logging/log.h>
#include <logging/log_msg.h>

static log_link_callback_t clbk;

LOG_MODULE_REGISTER(log_link_tz_s);

static void link_thread_func(void *dummy1, void *dummy2, void *dummy3);

K_THREAD_DEFINE(log_link_tz_s_tid, 1024, link_thread_func, NULL, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);
K_SEM_DEFINE(link_tz_s_sem, 0, 1);

extern void z_log_msg_enqueue(struct log_msg *msg);

static void new_data_notify(void)
{
	k_sem_give(&link_tz_s_sem);
}

static void timestamp_update_req(void)
{
	log_backend_tz_s_timestamp_update(z_log_get_timestamp());
}

static int init(const struct log_link *link, log_link_callback_t callback)
{
	clbk = callback;

	log_backend_tz_s_enable(new_data_notify, timestamp_update_req);
	link->ctrl_blk->domain_cnt = log_backend_tz_s_get_domain_count();
	return 0;
}

static u16_t get_source_count(const struct log_link *link, u8_t domain_id)
{
	return log_backend_tz_s_get_source_count(domain_id);
}

static int get_domain_name(const struct log_link *link, u8_t domain_id,
			char *buf, u32_t *length)
{
	u32_t buflen = *length;

	log_backend_tz_s_get_domain_name(domain_id, buf, length);
	if (buf) {
		buf[buflen - 1] = '\0';
	}

	return 0;
}

static int get_source_name(const struct log_link *link, u8_t domain_id,
			u16_t source_id, char *buf, u32_t length)
{
	buf[length - 1] = '\0';
	log_backend_tz_s_get_source_name(domain_id, source_id, buf, length - 1);

	return 0;
}

static int get_compiled_level(const struct log_link *link, u8_t domain_id,
			u16_t source_id, u8_t *level)
{
	log_backend_tz_s_get_compiled_level(domain_id, source_id, level);

	return 0;
}

static int set_runtime_level(const struct log_link *link, u8_t domain_id,
				u16_t source_id, u8_t level)
{
	log_backend_tz_s_set_runtime_level(domain_id, source_id, level);

	return 0;
}

static struct log_link_api api = {
	.init = init,
	.get_source_count = get_source_count,
	.get_domain_name = get_domain_name,
	.get_source_name = get_source_name,
	.get_compiled_level = get_compiled_level,
	.set_runtime_level = set_runtime_level,
};

LOG_LINK_DEF(log_link_tz_s, api, NULL);

static bool fetch_tz_log_msg(void)
{
	union log_msg_chunk *chunk = NULL;
	union log_msg_chunk *prev = NULL;
	struct log_msg *msg;
	bool more;
	int cnt = 0;

	if (log_backend_tz_s_no_msg()) {
		return false;
	}

	do {
		prev = chunk;
		chunk = log_msg_chunk_alloc();
		if (chunk == NULL) {
			return false;
		}

		more = log_backend_tz_s_get_msg(chunk);
		if (cnt == 0) {
			msg = &chunk->head;
		}
		if (cnt == 1) {
			prev->head.payload.ext.next = &chunk->cont;
		} else if (prev){
			prev->cont.next = &chunk->cont;
			chunk->cont.next = NULL;
		}
		cnt++;
	} while (more);

	msg->str = "";
	msg->hdr.params.hexdump.ext = 1;
	msg->hdr.ids.domain_id += log_link_tz_s.ctrl_blk->domain_offset;

	z_log_msg_enqueue(msg);

	return true;
}

static void link_thread_func(void *dummy1, void *dummy2, void *dummy3)
{
	__ASSERT_NO_MSG(log_backend_count_get() > 0);

	while (true) {
		k_sem_take(&link_tz_s_sem, K_FOREVER);
		while (fetch_tz_log_msg() == true) {
			/* empty */
		}
	}
}
