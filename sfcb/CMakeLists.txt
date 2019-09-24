# SPDX-License-Identifier: Apache-2.0
add_library(sfcb INTERFACE)

zephyr_include_directories(sfcb/include)

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_SFCB
    sfcb/src/sfcb.c
    )
zephyr_library_link_libraries(sfcb)

