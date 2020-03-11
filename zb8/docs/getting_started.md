<!--
  Copyright (c) 2018 Laczen

  SPDX-License-Identifier: Apache-2.0
-->

# GETTING STARTED

**REMARK 1: in the getting started guide the provided signature and encryption
keys are used, DO NOT use these keys in your real world setup.**

**REMARK 2: the provided overlay files have been made taking into consideration
that serial feedback is good during testing. This makes the generated FSL, swapper and loader larger then required. Further size optimization is adviced.**

ZB-8 is a configurable bootloader that uses dts overlay files for it's
configuration. In this getting started a nrf51_pca10028 board is used in a
single image configuration, the overlay file is
[nrf51_pca10028_si.overlay](../overlays/nrf51_pca10028_si.overlay).

From the overlay file the following adresses can be derived:
```
FSL start address = 0x00000000
SWPR start address = 0x00002000
LDR start address = 0x00007000
RUN start address = 0x0001C000
UPGRADE start address = 0x0001C800
```

The getting started guide will use `nrfjprog` to program the board.

## STEP 1: Clear the board
```
nrfjprog --eraseall
```

## STEP 2: Build and program the First Stage Loader
```
west build --pristine -s mymodules/zb8/fsl/ -b nrf51_pca10028  -t flash \
-- -DDTC_OVERLAY_FILE="../overlays/nrf51_pca10028_si.overlay fsl.overlay"
```

The serial logger will respond with:
```console
E: Starting failed, try reset
```

## STEP 3: Build and program the swapper
Build the swapper:
```
west build --pristine -s mymodules/zb8/swapper/ -b nrf51_pca10028 \
-- -DDTC_OVERLAY_FILE="../overlays/nrf51_pca10028_si.overlay swapper.overlay"
```
Sign the swapper:
```
./mymodules/zb8/scripts/zb8tool.py sign -v 0.0.1 -h 512 -sk \
./mymodules/zb8/root-ec256.pem build/zephyr/zephyr.hex \
build/zephyr/zephyr_signed.hex
```
Program the swapper:
```
nrfjprog --program build/zephyr/zephyr_signed.hex --sectorerase --verify
```

Hit the reset button. The serial logger will respond with:
```console
I: Welcome to ZB-8 first stage loader
I: Welcome to ZB-8 swapper
E: BAD IMAGE in upgrade slot
I: Trying to start image
I: Falling back to loader image
E: Starting failed, try reset
```

## STEP 4: Build and program the loader
Build the loader:

```
west build --pristine -s mymodules/zb8/loader/bledfu_loader/ -b nrf51_pca10028
-- -DDTC_OVERLAY_FILE="../../overlays/nrf51_pca10028_si.overlay ldr.overlay"

```

Sign the loader:

```
./mymodules/zb8/scripts/zb8tool.py sign -v 0.0.1 -h 512 -sk \
./mymodules/zb8/root-ec256.pem build/zephyr/zephyr.hex \
build/zephyr/zephyr_signed.hex
```

Program the loader:
```
nrfjprog --program build/zephyr/zephyr_signed.hex --sectorerase --verify
```

Hit the reset button. The serial logger will respond with:
```console
I: Welcome to ZB-8 first stage loader
I: Welcome to ZB-8 swapper
E: BAD IMAGE in upgrade slot
I: Trying to start image
I: Falling back to loader image
I: Booting image at 7200
I: Welcome to ZB-8 bledfu loader
I: Bluetooth initialized
I: Advertising successfully started
I: Waiting for a image...
```

If you have `nRF Connect` on your smartphone you can also see that a device
named `Zephyr ZB-8` is available with DFU functionality.

## STEP 5 : Build and program a test application (unencrypted)
Build the test application:
```
west build --pristine -s mymodules/zb8/apps/test/ -b nrf51_pca10028 \
-- -DDTC_OVERLAY_FILE="../../overlays/nrf51_pca10028_si.overlay"
```
Sign the application and modify the start address to the upgrade slot:
```
./mymodules/zb8/scripts/zb8tool.py sign -v 0.0.1 -h 512 -la 0x1B800 \
-sk ./mymodules/zb8/root-ec256.pem \
build/zephyr/zephyr.hex build/zephyr/zephyr_signed.hex
```
Program the test application:
```
nrfjprog --program build/zephyr/zephyr_signed.hex --sectorerase --verify
```
Hit reset, this will move the image to the run location and boot the image.

Hit reset again, this time the image will not start. This is because the image
was not confirmed so it can only be run once. By adding a `-c` to the signing
line a image can be "pre"-confirmed.

## STEP 6 : Build and program a test application (encrypted)
Build the test application:
```
west build --pristine -s mymodules/zb8/apps/test/ -b nrf51_pca10028 \
-- -DDTC_OVERLAY_FILE="../../overlays/nrf51_pca10028_si.overlay"
```
Sign+encrypt the application and modify the start address to the upgrade slot:
```
./mymodules/zb8/scripts/zb8tool.py sign -v 0.0.1 -h 512 -la 0x1B800 \
-sk ./mymodules/zb8/root-ec256.pem -ek ./mymodules/zb8/boot-ec256.pem \
build/zephyr/zephyr.hex build/zephyr/zephyr_signed.hex
```
Program the test application:
```
nrfjprog --program build/zephyr/zephyr_signed.hex --sectorerase --verify
```
Hit reset, this will decrypt the image to the run location (**SLOWER THAN IN
STEP 5**) and start the image.

## STEP 7: Build and program a updated swapper (encrypted)
Build the swapper:
```
west build --pristine -s mymodules/zb8/swapper/ -b nrf51_pca10028 \
-- -DDTC_OVERLAY_FILE="../overlays/nrf51_pca10028_si.overlay swapper.overlay"
```
Sign+encrypt the swapper and modify the start address to the upgrade slot:
```
./mymodules/zb8/scripts/zb8tool.py sign -v 0.0.1 -h 512 -la 0x1A800 \
-sk ./mymodules/zb8/root-ec256.pem -ek ./mymodules/zb8/boot-ec256.pem \
build/zephyr/zephyr.hex build/zephyr/zephyr_signed.hex
```
Program the swapper:
```
nrfjprog --program build/zephyr/zephyr_signed.hex --sectorerase --verify
```
Hit reset, this will decrypt the swapper to the run location, call the FSL to update the swapper and restart the swapper and loader (there will be no valid
image to boot).




