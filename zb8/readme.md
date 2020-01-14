<!--
  Copyright (c) 2018 Laczen

  SPDX-License-Identifier: Apache-2.0
-->
# ZB-eight - A work in progress - expect rough edges

## Overview

ZB-eight (ZB-8) is a secure bootloader for 32-bit MCUs. A typical ZB-8 for uploading and starting images consists of:

	a. First stage loader (FSL) responsible for starting an image or the bootloader, or responsible for upgrading the bootloader,

	b. Bootloader or image swapper responsible for copying/decrypting new firmware images,

	c. Firmware image.

ZB-eight is developed on top of the zephyr RTOS. Some of the idea's used in the implementation are coming from MCUboot, a similar bootloader for 32-bit MCUs.

The configuration of ZB-8 is mainly done using a dts overlay file. An example of such
an overlay file:

```
/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/delete-node/ &boot_partition;
/delete-node/ &scratch_partition;
/delete-node/ &slot0_partition;
/delete-node/ &slot1_partition;
/delete-node/ &storage_partition;

/ {
	chosen {
		zephyr,code-partition = &fsl_partition;
	};
};

&flash0 {
	/*
	 * For more information, see:
	 * http://docs.zephyrproject.org/latest/guides/dts/index.html#flash-partitions
	 */
	partitions {
		compatible = "fixed-partitions";
		#address-cells = <1>;
		#size-cells = <1>;

		fsl_partition: partition@0 {
			label = "fsl";
			reg = <0x00000000 0x00002000>;
		};
		boot_partition: partition@2000 {
			label = "boot";
			reg = <0x00002000 0x00004000>;
		};
		swpstat0_partition: partition@6000 {
			label = "swpstat-0";
			reg = <0x00006000 0x0000800>;
		};
		run0_partition: partition@6800 {
			label = "run-0";
			reg = <0x00006800 0x00010000>;
		};
		move0_partition: partition@6C00_0 {
			label = "move-0";
			reg = <0x00006C00 0x00010000>;
		};
		upgrade0_partition: partition@6C00_1 {
			label = "upgrade-0";
			reg = <0x00006C00 0x00010000>;
		};
	};
};

```

An firmware image area is defined as a combination of 4 slots: a run slot, a move slot, an upgrade slot and a swap status (swpstat) slot.

In a classical upgrade method new firmware will be placed in the upgrade slot
and a upgrade is started by clearing the swpstat slot and starting the
bootloader (or image swapper). The bootloader will start by moving the run image
to the move slot (as can be seen above the move slot overlaps with the run
slot), so this move in reality is just a move up in flash. The next step is then
to start copying/decrypting the image in the upgrade slot to the run slot and
simultaneously copying/encrypting the image from the move slot to the upgrade
slot.

A closer look at the overlay file shows that the move slot and the upgrade slot
are equal. When this is the case ZB-8 will do an "in place" decryption: it will
just decrypt the image in the upgrade slot to the run slot.

Multiple image areas (up to 4 at the moment) can be added to the dts overlay
file, this allows for a very flexible setup where you could have 2 image areas
and a third area to contain a flash data area that can be securely updated.

It is also possible to create a solution where the bootloader includes
communication (e.g. over ble) to update the image in a "single image" setup.
Even in this case the images remain protected and encrypted during upload.

## Security

In ZB-8 images are protected by an ec256 signature and aes encryption. All
information about the image, encryption, dependencies, ... is added to the image
header that is prepended to the image. A ec256 signature is added to the header to tamperproof the firmware.

## Downgrade protection

ZB-8 protects against downgrades by including a dependency in each image that
designates what images it can replace. An image that has a lower version than
the image in the run slot is not allowed to replace this image.

## Where can I find what ?

[./zb8](./zb8) contains the library of routines that make up ZB-8

[./fsl](./fsl) contains the First Stage Loader App

[./bootloader](./bootloader) contains a classical swapping bootloader

[./bledfubootloader](./bledfubootloader) contains a bootloader with ble dfu upload functionality

[./appbledfuloader](./appbledfuloader) contains a sample app with ble dfu upload functionality


## Documentation

~~Project documentation is located in the [docs](./docs/index.md) folder.~~
needs updating
