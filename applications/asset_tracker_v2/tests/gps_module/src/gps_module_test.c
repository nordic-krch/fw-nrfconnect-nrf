/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>
#include <stdbool.h>
#include <stdlib.h>
#include <mock_modules_common.h>
#include "app_module_event.h"

extern struct event_listener __event_listener_gps_module;

//** STUFF TO AVOID LINKER ERRORS**/

//#include "event_manager.h"

void sys_reboot(int reason)
{
}

void *k_malloc(size_t size)
{
	return malloc(size);
}

/* Dummy structs to please linker. The EVENT_SUBSCRIBE macros in gps_module.c
 * depend on these to exist. But since we are unit testing, we dont need
 * these subscriptions.
 */
const struct event_type __event_type_gps_module_event;
const struct event_type __event_type_app_module_event;
const struct event_type __event_type_data_module_event;
const struct event_type __event_type_util_module_event;

/** STUFF TO AVOID LINER ERRORS END **/

/* Suite teardown shall finalize with mandatory call to generic_suiteTearDown. */
extern int generic_suiteTearDown(int num_failures);

//extern bool gps_module_event_handler(struct event_header *);

int test_suiteTearDown(int num_failures)
{
	return generic_suiteTearDown(num_failures);
}

void test_app_evt_start(void)
{
	struct app_module_event *app_module_event = new_app_module_event();

	app_module_event->type = APP_EVT_START;

	static struct module_data expected_module_data = {
		.name = "gps",
		.msg_q = NULL,
		.supports_shutdown = true,
	};

	__wrap_module_start_ExpectAndReturn(&expected_module_data, 0);

	bool ret = __event_listener_gps_module.notification(
		(struct event_header *)app_module_event);

	free(app_module_event);

	TEST_ASSERT_EQUAL(0, ret);
}

/* It is required to be added to each test. That is because unity is using
 * different main signature (returns int) and zephyr expects main which does
 * not return value.
 */
extern int unity_main(void);

void main(void)
{
	(void)unity_main();
}
