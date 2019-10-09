#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

zephyr_sources_ifdef(CONFIG_LOG_BACKEND_TZ_S log_backend_tz_s.c)
zephyr_sources_ifdef(CONFIG_LOG_LINK_TZ_S log_link_tz_s.c)