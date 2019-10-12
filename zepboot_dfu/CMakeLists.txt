# SPDX-License-Identifier: Apache-2.0
add_library(zepboot_dfu INTERFACE)

zephyr_include_directories(zepboot_dfu/include)

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_ZEPBOOT_DFU
    zepboot_dfu/src/nrf_dfu.c
    zepboot_dfu/src/zepboot_dfu.c
    )
zephyr_library_link_libraries(zepboot_dfu)
