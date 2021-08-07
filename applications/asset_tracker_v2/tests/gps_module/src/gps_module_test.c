/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>
#include <stdbool.h>
#include <stdlib.h>
#include <mock_modules_common.h>
#include <mock_event_manager.h>
#include <mock_gps.h>
#include "app_module_event.h"
#include "gps_module_event.h"
#include "data_module_event.h"

extern struct event_listener __event_listener_gps_module;

#define GPS_MODULE_EVT_HANDLER(eh) __event_listener_gps_module.notification(eh)

/* Dummy functions and objects. */

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

/** Dummy functions and objects - End.  **/

/* Suite teardown shall finalize with mandatory call to generic_suiteTearDown. */
extern int generic_suiteTearDown(int num_failures);


int test_suiteTearDown(int num_failures)
{
	return generic_suiteTearDown(num_failures);
}


static void setup_gps_module_in_init_state(void)
{
	struct app_module_event *app_module_event = new_app_module_event();

	app_module_event->type = APP_EVT_START;

	static struct module_data expected_module_data = {
		.name = "gps",
		.msg_q = NULL,
		.supports_shutdown = true,
	};

	__wrap_module_start_ExpectAndReturn(&expected_module_data, 0);

	bool ret = GPS_MODULE_EVT_HANDLER((struct event_header *)app_module_event);
	TEST_ASSERT_EQUAL(0, ret);

	free(app_module_event);
}

static void setup_gps_module_in_running_state()
{
	setup_gps_module_in_init_state();

	struct data_module_event *data_module_event = new_data_module_event();

	data_module_event->type = DATA_EVT_CONFIG_INIT;
	data_module_event->data.cfg.gps_timeout = 60;

	bool ret = GPS_MODULE_EVT_HANDLER((struct event_header *)data_module_event);
	TEST_ASSERT_EQUAL(0, ret);

	free(data_module_event);
}

/* Test whether sending a APP_EVT_DATA_GET event to the GPS module generates
 * the GPS_EVT_ACTIVE event, when the GPS module is in the running state. */
void test_gps_start(void)
{
	/* Pre-condition. */
	setup_gps_module_in_running_state();

	/* Setup expectations. */
	struct gps_module_event *exp_event = new_gps_module_event();
	exp_event->type = GPS_EVT_ACTIVE;

	__wrap__event_submit_Expect((struct event_header *)exp_event);

	/* Stimulus. */
	struct app_module_event *app_module_event = new_app_module_event();

	app_module_event->type = APP_EVT_DATA_GET;
	app_module_event->count = 1;
	app_module_event->data_list[0] = APP_DATA_GNSS;

	bool ret = GPS_MODULE_EVT_HANDLER((struct event_header *)app_module_event);
	TEST_ASSERT_EQUAL(0, ret);

	/* Cleanup */
	free(app_module_event);
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
