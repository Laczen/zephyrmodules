<!--
  Copyright (c) 2018 Laczen

  SPDX-License-Identifier: Apache-2.0
-->

# GETTING STARTED

**REMARK 1: in the getting started guide the provided signature and encryption
keys are used, DO NOT use these keys in your real world setup.**

**REMARK 2: the provided overlay files have been made taking into consideration
that serial feedback is good during testing. This makes the generated FSL and
bootloader larger then required. Further size optimization is adviced.**

ZB-8 is a configurable bootloader that uses dts overlay files for it's
configuration. In this getting started a nrf51_pca10028 board is used in a
single image configuration, the overlay file is
[nrf51_pca10028_si.overlay](../overlays/nrf51_pca10028_si.overlay). From this
overlay the following adresses can be derived:
```
FSL start address = 0x00000000
BOOT start address = 0x00002000
RUN start address = 0x0001A000
UPGRADE start address = 0x0001A800
SWPSTAT start address = 0x00019C00 (size = 1 flash page)
```

The getting started guide will use `nrfjprog` to program the board.

## STEP 1: Clear the board
```
nrfjprog --eraseall
```

## STEP 2: Build and program the First Stage Loader
```
west build --pristine -s mymodules/zb8/fsl -b nrf51_pca10028  -t flash -- \
-DDTC_OVERLAY_FILE="../overlays/nrf51_pca10028_si.overlay fsl.overlay"
```

## STEP 3: Build and program the bootloader
Build the bootloader:
```
west build --pristine -s mymodules/zb8/bootloaders/bledfu_bootloader/ \
-b nrf51_pca10028  -- \
-DDTC_OVERLAY_FILE="../../overlays/nrf51_pca10028_si.overlay boot.overlay"
```
Sign the bootloader:
```
./mymodules/zb8/scripts/zb8tool.py sign -v 0.0.1 -h 512 -sk \
./mymodules/zb8/root-ec256.pem build/zephyr/zephyr.hex \
build/zephyr/zephyr_signed.hex
```
Program the bootloader:
```
nrfjprog --program build/zephyr/zephyr_signed.hex --sectorerase --verify
```
Hit the reset button and you are welcomed with a bootloader greeting message.
If you have `nRF Connect` on your smartphone you can also see that a device
named `Zephyr ZB-8` is available with DFU functionality.

## STEP 4 : Build and program a test application (unencrypted)
Build the test application:
```
west build --pristine -s mymodules/zb8/apps/test/ -b nrf51_pca10028  -- \
-DDTC_OVERLAY_FILE="../../overlays/nrf51_pca10028_si.overlay"
```
Sign the application and modify the start address to the upgrade slot:
```
./mymodules/zb8/scripts/zb8tool.py sign -v 0.0.1 -h 512 -la 0x1A800 -sk \
./mymodules/zb8/root-ec256.pem \
build/zephyr/zephyr.hex build/zephyr/zephyr_signed.hex
```
Program the test application:
```
nrfjprog --program build/zephyr/zephyr_signed.hex --sectorerase --verify
```
Clear the swap status area:
```
nrfjprog --erasepage 0x19C00
```
Hit reset, this will move the image to the run location.

Hit reset again, you will be welcomed by the test application.

## STEP 5 : Build and program a test application (encrypted)
Build the test application:
```
west build --pristine -s mymodules/zb8/apps/test/ -b nrf51_pca10028  -- \
-DDTC_OVERLAY_FILE="../../overlays/nrf51_pca10028_si.overlay"
```
Sign+encrypt the application and modify the start address to the upgrade slot:
```
./mymodules/zb8/scripts/zb8tool.py sign -v 0.0.1 -h 512 -la 0x1A800 \
-sk ./mymodules/zb8/root-ec256.pem -ek ./mymodules/zb8/boot-ec256.pem \
build/zephyr/zephyr.hex build/zephyr/zephyr_signed.hex
```
Program the test application:
```
nrfjprog --program build/zephyr/zephyr_signed.hex --sectorerase --verify
```
Clear the swap status area:
```
nrfjprog --erasepage 0x19C00
```
Hit reset, this will decrypt the image to the run location (**SLOWER THAN IN
STEP 4**).

Hit reset again, you will be welcomed by the test application again.

## STEP 6: Build and program a updated bootloader (encrypted)
Build the bootloader:
```
west build --pristine -s mymodules/zb8/bootloaders/bledfu_bootloader/ \
-b nrf51_pca10028  -- \
-DDTC_OVERLAY_FILE="../../overlays/nrf51_pca10028_si.overlay boot.overlay"
```
Sign+encrypt the bootloader and modify the start address to the upgrade slot:
```
./mymodules/zb8/scripts/zb8tool.py sign -v 0.0.1 -h 512 -la 0x1A800 \
-sk ./mymodules/zb8/root-ec256.pem -ek ./mymodules/zb8/boot-ec256.pem \
build/zephyr/zephyr.hex build/zephyr/zephyr_signed.hex
```
Program the bootloader:
```
nrfjprog --program build/zephyr/zephyr_signed.hex --sectorerase --verify
```
Clear the swap status area:
```
nrfjprog --erasepage 0x19C00
```
Hit reset, this will decrypt the bootloader to the run location.

Hit reset again, the FSL copies the bootloader and starts it.




