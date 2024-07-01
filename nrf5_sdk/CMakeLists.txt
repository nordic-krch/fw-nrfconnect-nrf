if (CONFIG_NRF5_SDK)
  zephyr_include_directories(include)

  add_subdirectory(softdevice)
  add_subdirectory(ble)
  add_subdirectory(libraries)
endif()
