#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(gps_module_test)

# generate runner for the test
test_runner_generate(src/gps_module_test.c)

# create mock
cmock_handle(${ZEPHYR_BASE}/../nrf/include/event_manager.h )
cmock_handle(${ZEPHYR_BASE}/../nrf/include/date_time.h )
cmock_handle(${ZEPHYR_BASE}/../nrf/applications/asset_tracker_v2/src/modules/modules_common.h )


# add gps_module (the UUT)
target_sources(app PRIVATE ../../src/modules/gps_module.c)
target_include_directories(app PRIVATE ../../src/modules/)
target_include_directories(app PRIVATE ${ZEPHYR_BASE}/../nrf/subsys/event_manager)
target_include_directories(app PRIVATE ../../src/)
target_include_directories(app PRIVATE ${ZEPHYR_BASE}/../nrf/modules/cjson/include)
target_include_directories(app PRIVATE ${ZEPHYR_BASE}/../nrf/applications/asset_tracker_v2/src/events/)
target_include_directories(app PRIVATE )

# add test file
target_sources(app PRIVATE src/gps_module_test.c)
target_include_directories(app PRIVATE .)

target_compile_options(app PRIVATE
	-DCONFIG_GPS_DEV_NAME="test"
  	-DCONFIG_ASSET_TRACKER_V2_APP_VERSION_MAX_LEN=20
	-DCONFIG_GPS_MODULE_LOG_LEVEL=4
	-DCONFIG_GPS_DEV_NAME="GPS_SIM")

# Preinclude file to the module under test to redefine IS_ENABLED() macro
# which is used in the module.
#set_property(SOURCE src/uut/uut.c PROPERTY COMPILE_FLAGS
#	"-include src/example_test.h")
