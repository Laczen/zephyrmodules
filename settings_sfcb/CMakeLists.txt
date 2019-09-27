# SPDX-License-Identifier: Apache-2.0
add_library(settings_sfcb INTERFACE)

zephyr_include_directories(settings_sfcb/include)

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_SETTINGS_SFCB
    settings_sfcb/src/settings_sfcb.c
    )
zephyr_library_link_libraries(settings_sfcb)

