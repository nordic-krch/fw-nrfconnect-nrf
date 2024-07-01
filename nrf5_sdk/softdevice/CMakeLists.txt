zephyr_library()
zephyr_library_sources_ifdef(CONFIG_SOFTDEVICE nrf_sdh.c)
zephyr_library_sources_ifdef(CONFIG_SOFTDEVICE nrf_sdh_soc.c)
zephyr_library_sources_ifdef(CONFIG_S140 nrf_sdh_ble.c)
zephyr_linker_sources(SECTIONS sdh.ld)

if (CONFIG_S140)
zephyr_include_directories(../include/s140)
endif()
