/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <arch/cpu.h>
#include <spm.h>
#include <logging/log_ctrl.h>
#include <logging/log.h>
#include <fatal.h>

LOG_MODULE_REGISTER(fatal_error, CONFIG_FATAL_ERROR_LOG_LEVEL);

extern void sys_arch_reboot(int type);

void k_sys_fatal_error_handler(unsigned int reason,
			       const z_arch_esf_t *esf)
{
	ARG_UNUSED(esf);
	ARG_UNUSED(reason);

	LOG_PANIC();

	if (IS_ENABLED(CONFIG_IS_SPM) &&
	    IS_ENABLED(CONFIG_SPM_NS_DEBUG_LOGS_DUMP)) {
		spm_ns_fatal_error_handler();
	}

	LOG_ERR("Resetting system");
	sys_arch_reboot(0);

	CODE_UNREACHABLE;
}
