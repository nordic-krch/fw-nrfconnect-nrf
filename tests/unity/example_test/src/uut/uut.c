/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <uut.h>
#include <foo/foo.h>
#include <misc/util.h>
#include <stddef.h>

int uut_init(void *handle)
{
	if (IS_ENABLED(CONFIG_UUT_PARAM_CHECK)) {
		if (handle == NULL) {
			return -1;
		}
	}

	return foo_init(handle);
}
