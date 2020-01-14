# SPDX-License-Identifier: Apache-2.0
add_library(zb8 INTERFACE)

zephyr_include_directories(zb8/include)

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_ZB_EIGHT
    zb8/src/zb8_flash.c
    zb8/src/zb8_slot.c
    zb8/src/zb8_dfu.c
    zb8/src/zb8_fsl.c
)

zephyr_library_sources_ifdef(CONFIG_ZB_EIGHT_IS_BOOTLOADER
    zb8/src/zb8_crypto.c
    zb8/src/zb8_tlv.c
    zb8/src/zb8_image.c
    zb8/src/zb8_move.c
)

zephyr_library_link_libraries(zb8)
