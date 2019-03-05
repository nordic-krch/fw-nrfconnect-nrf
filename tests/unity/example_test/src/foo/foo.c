/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <foo/foo.h>

int foo_init(void *handle)
{
	/* This implementation will be wrapped and mocked. */
	return 0;
}
