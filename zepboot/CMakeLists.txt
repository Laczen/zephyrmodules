# SPDX-License-Identifier: Apache-2.0
#if(CONFIG_ZEPBOOT)
add_library(zepboot INTERFACE)

zephyr_include_directories(zepboot/include)

zephyr_library()
zephyr_library_sources(
    zepboot/src/zb_aes.c
    zepboot/src/zb_ec256.c
    zepboot/src/zb_flash.c
    zepboot/src/zb_image.c
    zepboot/src/zb_move.c
    zepboot/src/zb_tlv.c
    )
zephyr_library_link_libraries(zepboot)
target_link_libraries(zepboot INTERFACE zephyr_interface)
#endif()
