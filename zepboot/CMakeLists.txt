# SPDX-License-Identifier: Apache-2.0
add_library(zepboot INTERFACE)

zephyr_include_directories(zepboot/include)

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_ZEPBOOT
    zepboot/src/zb_aes.c
    zepboot/src/zb_ec256.c
    zepboot/src/zb_flash.c
    zepboot/src/zb_image.c
    zepboot/src/zb_move.c
    zepboot/src/zb_tlv.c
    zepboot/src/zb_slotmap.c
    )
zephyr_library_link_libraries(zepboot)
