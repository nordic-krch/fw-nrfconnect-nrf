cmake_minimum_required(VERSION 3.13.1)
include($ENV{ZEPHYR_BASE}/../nrf/tests/unity/unity_test.cmake)
project(crc)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE .)

test_runner_generate(src/nus_test.c)
cmock_handle(${ZEPHYR_BASE}/include/bluetooth/gatt.h bluetooth)

#set_property(SOURCE src/nus_test.c PROPERTY COMPILE_FLAGS "-include ${PROJECT_SOURCE_DIR}/src/test.h")
set_property(SOURCE build/unity/mocks/bluetooth/mock_gatt.c PROPERTY COMPILE_FLAGS "-include src/test.h")
