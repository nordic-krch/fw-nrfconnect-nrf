/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <logging/log_ctrl.h>
#include <spm.h>
#include <init.h>

static void log_dump(void)
{
	LOG_PANIC();
}

int init(const struct device *dev)
{
	ARG_UNUSED(dev);
	spm_ns_set_fatal_error_handler(log_dump);
}

SYS_INIT(log_dump_init, PRE_KERNEL_1, 0);
