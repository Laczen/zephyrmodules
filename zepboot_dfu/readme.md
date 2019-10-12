<!--
  Copyright (c) 2018 Laczen

  SPDX-License-Identifier: Apache-2.0
-->
# ZEPbootDFU - A work in progress - expect rough edges

## Overview

ZEPbootDFU is a library to add loader functionality (at the moment only
bluetooth using Nordic DFU) to an application that is running on a system
equipped with ZEPboot. When a new application is uploaded to the system a reset
triggers the signature checking, decrypting and booting of this new app.

## Image types

ZEPboot supports two types of images:

The first type (classical) is uploaded to slot1, copied to slot0 and run from
slot0. This kind of image will typically contain the functionality to upload new
images to slot1. These classical images are compiled to be run from slot0, and
'CONFIG_FLASH_LOAD_OFFSET' is set to the start of slot0 (offset by the header
size).

A second type (inplace) is uploaded to slot1, (decrypted) in slot1 and run from
slot1. These images cannot upload a new image to slot1. To allow uploads the
inplace images are accompagnied by a loader application that is run from slot0.
This loader app can be smal in size and the functionality is limited to only the
upload functionality. This kind of setup allows the creation of a smaller slot0
(only the loader needs to fit) and a larger slot1 for the inplace image.
For the inplace images the DFU functionality is limited to resetting the device
or to boot into slot0. So a typical upload for a inplace image would be:

a. restart to boot slot0
b. use the DFU to upload image1
c. reboot, (decrypt) and start image1

## Usage

**REMARK** When starting with a blank device it is required that you program the
bootloader and the first image using `openocd/nrfjprog`. You can find how to do
that in the ZEPboot documentation.

### **Classical Image**

#### Create the sample
A sample for classical images can be found in
[app_with_loader](./app_with_loader), build this application using west:

```
west build -s mymodules/zepboot_dfu/app_with_loader -b nrf52_pca10040
```

#### Sign and encrypt the sample

```
./mymodules/zepboot/scripts/imgtool.py sign -io 0x200 -ss 0x3A000 -a 8 -v 0.0.1 -sk ./mymodules/zepboot/root-ec256.pem -ek ./mymodules/zepboot/boot-ec256.pem build/zephyr/zephyr.hex build/signed.hex
```

#### First time load

When the device only contains the bootloader you can use the following commands

```
objcopy --change-addresses 0x3A000 build/signed.hex build/signedoff.hex
```

```
nrfjprog --program build/signedoff.hex --sectorerase
```

#### Loads with nrfconnect/nrftoolbox

a. Copy the generated signed.hex to your smartphone/tablet,

b. Start nrfconnect/nrftoolbox,

c. Connect to the zephyr device (named Zephyr),

d. Select DFU (in top toolbar),

e. Select Application,

e. Select the image (signed.hex),

f. Select No for the init packet,

g. Reset your zephyr device

### Inplace Image

#### Create the sample
A sample for a inplace image can be found in
[app_without_loader](./app_without_loader), build this application using west:

```
west build -s mymodules/zepboot_dfu/app_without_loader -b nrf52_pca10040
```

#### Sign and encrypt the sample

```
./mymodules/zepboot/scripts/imgtool.py sign -io 0x200 -ss 0x3A000 -a 8 -v 0.0.1 -sk ./mymodules/zepboot/root-ec256.pem -ek ./mymodules/zepboot/boot-ec256.pem build/zephyr/zephyr.hex build/signed.hex
```

#### Loads

Remember that inplace images need an accompagnying loader to place the image in slot1. The classical image [app_with_loader](./app_with_loader) is an example of
such a loader so this needs to be programmed into slot0 (using openocd/nrfjprog/nrfconnect...) first.

Once the loader is into place the just generated signed.hex ([app_without_loader](./app_without_loader)) can be programmed to slot 1 using nrfconnect/nrftoolbox. A reset will decrypt the image and start it from slot 1.

One of the BT characteristics `23A07C4E-9E3B-4379-BD0D-2B03381274A4` is of type write only. This can be used to reboot, reboot to slt0, reboot to slt1 by writing resp. 0x00, 0x01 or 0x02 to the characteristic.