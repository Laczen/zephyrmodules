# SPDX-License-Identifier: Apache-2.0
#if(CONFIG_SFCB)
add_library(sfcb INTERFACE)

zephyr_include_directories(sfcb/include)

zephyr_library()
zephyr_library_sources(
    sfcb/src/sfcb.c
    )
zephyr_library_link_libraries(sfcb)
target_link_libraries(sfcb INTERFACE zephyr_interface)
#endif()
