#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

add_subdirectory_ifdef(CONFIG_CJSON_LIB cjson)

if(CONFIG_SUPL_CLIENT_LIB)
  if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lib/bin/supl)
    assert(0 "Cannot locate SUPL Client library, should be located in your nrf repo ext/lib/bin/supl.")
  else()
    add_subdirectory(lib/bin/supl)
  endif()
endif()
