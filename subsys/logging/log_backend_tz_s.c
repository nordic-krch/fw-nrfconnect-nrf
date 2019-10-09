/*
 * Copyright (c) 2019 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <logging/log_backend_tz_s.h>
#include <cortex_m/tz.h>
#include <logging/log_core.h>
#include <logging/log_ctrl.h>
#include <logging/log_backend.h>
#include <logging/log_output.h>
#include <sys/slist.h>

static u8_t buf[8];
static sys_slist_t msg_list;
static u32_t timestamp;
static bool enabled;
TZ_NONSECURE_FUNC_PTR_DECLARE(timestamp_update_req_fptr);
TZ_NONSECURE_FUNC_PTR_DECLARE(new_log_notify_clbk);

struct log_output_ctx {
	u32_t offset;
	struct log_msg *msg;
};

static u32_t get_timestamp(void);

static int data_out(u8_t *data, size_t length, void *ctx)
{
	int err = 0;
	struct log_output_ctx *out_ctx = (struct log_output_ctx *)ctx;

	err = log_msg_hexdump_extend(out_ctx->msg, length);
	if (err != 0) {
		return length;
	}

	log_msg_hexdump_data_put(out_ctx->msg, data, &length, out_ctx->offset);

	out_ctx->offset += length;

	return length;
}

LOG_OUTPUT_DEFINE(log_output, data_out, buf, sizeof(buf));

static void put(const struct log_backend *const backend,
		struct log_msg *msg)
{
	log_msg_get(msg);

	sys_slist_append(&msg_list, (sys_snode_t *)msg);

	if (enabled) {
		new_log_notify_clbk();
	}
}


static void init(void)
{
	sys_slist_init(&msg_list);
	log_set_timestamp_func(get_timestamp, 0);
}

static void panic(struct log_backend const *const backend)
{

}

static void dropped(const struct log_backend *const backend, u32_t cnt)
{

}

const struct log_backend_api api = {
	.put = IS_ENABLED(CONFIG_LOG_IMMEDIATE) ? NULL : put,
	.panic = panic,
	.init = init,
	.dropped = IS_ENABLED(CONFIG_LOG_IMMEDIATE) ? NULL : dropped,
};

LOG_BACKEND_DEFINE(log_backend_tz_s, api, true);

__TZ_NONSECURE_ENTRY_FUNC
u32_t log_backend_tz_s_get_domain_count(void)
{
	return 1;
}

__TZ_NONSECURE_ENTRY_FUNC
u32_t log_backend_tz_s_get_source_count(u32_t domain_id)
{
	return log_sources_count(LOCAL_DOMAIN_ID);
}

__TZ_NONSECURE_ENTRY_FUNC
void log_backend_tz_s_get_domain_name(u32_t domain_id, char *name, u32_t *len)
{
	if (name != NULL) {
		if (domain_id == 0) {
			strncpy(name, CONFIG_LOG_DOMAIN_NAME, *len);
		} else {
			name[0] = '\0';
		}
	}

	*len = strlen(CONFIG_LOG_DOMAIN_NAME);
}

__TZ_NONSECURE_ENTRY_FUNC
void log_backend_tz_s_get_source_name(u32_t domain_id, u32_t src_id,
					char *name, u32_t len)
{
	if (domain_id == 0) {
		strncpy(name, log_source_name_get(NULL, 0, 0, src_id), len);
	} else {
		name[0] = '\0';
	}
}

__TZ_NONSECURE_ENTRY_FUNC
void log_backend_tz_s_get_compiled_level(u32_t domain_id, u32_t src_id,
					 u8_t * level)
{
	if (domain_id == 0) {
		*level = log_compiled_level_get(src_id);
	} else {
		*level = 0;
	}
}

__TZ_NONSECURE_ENTRY_FUNC
void log_backend_tz_s_set_runtime_level(u32_t domain_id, u32_t src_id,
					u8_t level)
{
	if (!IS_ENABLED(CONFIG_LOG_RUNTIME_FILTERING)) {
		return;
	}

	log_filter_set(&log_backend_tz_s, domain_id, src_id, level);
}

__TZ_NONSECURE_ENTRY_FUNC
void log_backend_tz_s_enable(log_backend_tz_s_notify_clbk_t new_log_clbk,
		log_backend_tz_s_notify_clbk_t timestamp_req)
{
	new_log_notify_clbk = TZ_NONSECURE_FUNC_PTR_CREATE(new_log_clbk);

	timestamp_update_req_fptr =
			TZ_NONSECURE_FUNC_PTR_CREATE(timestamp_req);

	if (log_backend_tz_s_no_msg() == false) {
		new_log_notify_clbk();
	}

	enabled = true;
}

static u32_t get_timestamp(void)
{
	if (!enabled) {
		return 0;
	}

	if (TZ_NONSECURE_FUNC_PTR_IS_NS(timestamp_update_req_fptr)) {
		timestamp_update_req_fptr();
	}

	return timestamp;
}

__TZ_NONSECURE_ENTRY_FUNC
void log_backend_tz_s_timestamp_update(u32_t new_timestamp)
{
	timestamp = new_timestamp;
}

static bool get_msg_chunk(struct log_msg *msg, union log_msg_chunk *chunk,
			  u32_t progress_cnt)
{
	bool more;
	static struct log_msg_cont *cont;

	if (progress_cnt == 0) {
		memcpy(chunk, msg, sizeof(struct log_msg));
		more =  (msg->hdr.params.hexdump.length >
				LOG_MSG_HEXDUMP_BYTES_SINGLE_CHUNK);
	} else {
		cont = (progress_cnt == 1) ? msg->payload.ext.next : cont->next;
		memcpy(chunk, cont, sizeof(struct log_msg_cont));
		more = (cont->next != NULL);

	}

	return more;
}

struct log_msg *msg_to_stringified_msg(struct log_msg *msg)
{
	struct log_msg *out_msg = (struct log_msg *)log_msg_chunk_alloc();

	memcpy(&out_msg->hdr.params, &msg->hdr.params, sizeof(msg->hdr.params));
	out_msg->hdr.params.generic.type = LOG_MSG_TYPE_HEXDUMP;
	out_msg->hdr.params.generic.ext = 1;
	out_msg->hdr.ref_cnt = 1;
	out_msg->hdr.timestamp = msg->hdr.timestamp;
	out_msg->hdr.ids = msg->hdr.ids;
	out_msg->hdr.params.hexdump.length = 0;

	if (out_msg != NULL) {
		struct log_output_ctx out_ctx = {
			.offset = 0,
			.msg = out_msg
		};

		log_output_ctx_set(&log_output, &out_ctx);
		log_output_msg_process(&log_output, msg,
					LOG_OUTPUT_FLAG_CRLF_NONE);
	}

	return out_msg;
}

__TZ_NONSECURE_ENTRY_FUNC
bool log_backend_tz_s_no_msg(void)
{
	return sys_slist_is_empty(&msg_list);
}

__TZ_NONSECURE_ENTRY_FUNC
bool log_backend_tz_s_get_msg(union log_msg_chunk *chunk)
{
	bool more;
	static struct log_msg *out_msg = NULL;
	static u32_t progress_cnt;

	if (out_msg == NULL) {
		struct log_msg *msg;
		msg = (struct log_msg *)sys_slist_get(&msg_list);
		if (msg == NULL) {
			return false;
		}

		out_msg = msg_to_stringified_msg(msg);
		progress_cnt = 0;
		log_msg_put(msg);
	}
	more = get_msg_chunk(out_msg, chunk, progress_cnt++);
	if (!more) {
		log_msg_put(out_msg);
		out_msg = NULL;
	}

	return more;
}

