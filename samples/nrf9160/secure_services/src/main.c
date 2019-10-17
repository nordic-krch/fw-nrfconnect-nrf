/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <misc/printk.h>
#include <misc/reboot.h>
#include <secure_services.h>
#include <kernel.h>
#include <pm_config.h>
#include <shell/shell.h>
#include <logging/log_link.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(app);

void print_hex_number(const struct shell *shell, u8_t *num, size_t len)
{
	shell_fprintf(shell, SHELL_NORMAL, "0x");
	for (int i = 0; i < len; i++) {
		shell_fprintf(shell, SHELL_NORMAL, "%02x", num[i]);
	}
	shell_fprintf(shell, SHELL_NORMAL, "\n");
}

void print_random_number(const struct shell *shell, u8_t *num, size_t len)
{
	shell_fprintf(shell, SHELL_NORMAL, "Random number len %d: ", len);
	print_hex_number(shell, num, len);
}

static int cmd_random_numbers(const struct shell *shell,
			      size_t argc, char **argv)
{
	const int len = 144;
	u8_t random_number[len];
	size_t olen = len;
	int ret;

	LOG_INF("Requesting %d random numbers", len);
	ret = spm_request_random_number(random_number, len, &olen);
	if (ret != 0) {
		shell_error(shell, "Could not get random number (err: %d)\n",
				ret);
		return 0;
	}

	print_random_number(shell, random_number, olen);

	return 0;
}

static int cmd_reboot(const struct shell *shell,
			      size_t argc, char **argv)
{
	sys_reboot(0); /* Argument is ignored. */

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_log_secure_service,
	SHELL_CMD_ARG(random_numbers, NULL, "Get random numbers.",
			cmd_random_numbers, 1, 0),
	SHELL_CMD_ARG(reboot, NULL, "Reboot", cmd_reboot, 1, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(secure_service, &sub_log_secure_service,
		"Secure services commands", NULL);

