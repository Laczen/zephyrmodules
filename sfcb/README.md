## sfcb

A small/fail-safe flash circular buffer for zephyr.

Sfcb stores variables as a identifier(id) - value pair. The value stored can be
binary blobs, strings, integers, longs, or any combination of these.

Sfcb provides a high level and low level API for reading and writing variables.
The low level API allows better control over how the data is read or written,
this can be handy when tighter control is needed.

### High level API for reading and writing variables

a. ```sfcb_write(&fs, id, &data, len)``` where `fs` is the filesystem, `id` is
the identifier, `data` is the data to write and `len` is the length of data.

b. ```sfcb_read(&fs, id, &data, len)``` where `fs` is the filesystem, `id` is
the identifier, `data` is the data to write and `len` is the length of data. As
there might be several stored items with the same id, the read returns the data
of the last written.

### Low level API for reading and writing variables

When storing variables sfcb can write the value in one go, but it can also write
the value in steps. The writing of the values is very similar to a classical
file write operation and is performed in three steps:

a. Opening a location for writing: ```sfcb_open_loc(&fs, &loc, id, len)```

b. Writing the value: ```sfcb_write_loc(&loc, &data1, len1)```

(c. Continue writing: ```sfcb_write_loc(&loc, &data2, len2)```)

d. Close the location when finished: ```sfcb_close_loc(&loc)```

Reading stored variables is done by walking over the locations and when the
required location is found reading the data using
```sfcb_read_loc(&loc, &data, len)```. Similar to the write, the read operation
can also be executed in steps.

**Power-loss resilience** - sfcb is designed to handle random power
failures. If power is lost the flash circular buffer will fall back to the last
known good state.

**Wear leveling** - sfcb uses flash to store data in a buffer. This buffer
erases pages as needed for storage. Whenever a erase is performed a (optional)
compress algorithm is called automatically to allow stored data to remain
available.

**Bounded RAM/ROM** - sfcb is designed to work with a small amount of
memory.

## Example

Here's a simple example using the low level API that updates a location with
id 0 with a variable `boot_count` every time main runs. The program can be
interrupted at any time without losing track of how many times it has been
booted and without corrupting the filesystem:

``` c
/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <sfcb.h>
#include <string.h>

// variables used by the filesystem
sfcb_fs sfcb;

// configuration of the filesystem is provided by this struct
const sfcb_fs_cfg cfg = {
	.offset = DT_FLASH_AREA_STORAGE_OFFSET,
	.sector_size = DT_FLASH_ERASE_BLOCK_SIZE,
	.sector_cnt = DT_FLASH_AREA_STORAGE_SIZE /
		      DT_FLASH_ERASE_BLOCK_SIZE,
	.dev_name = DT_FLASH_AREA_STORAGE_DEV,
};

// entry point
int main(void) {
    // mount the filesystem
    int rc;
    u32_t boot_count = 0;
    sfcb_loc loc;
    sfcb_ate *ate;

    sfcb.cfg = &cfg;
    rc = sfcb_mount(&sfcb);

    // reformat if we can't mount the filesystem
    if (rc) {
        sfcb_format(&sfcb);
        sfcb_mount(&sfcb);
    }

    // read current count
    if (!sfcb_first_loc(&sfcb, &loc)) {
        do {
            ate = sfcb_get_ate(&loc);
            if (ate->id == 0) {
                sfcb_read_loc(&loc, &boot_count, sizeof(boot_count));
            }
        } while (!sfcb_next_loc(&loc));
    }

    // update boot count
    boot_count += 1;
    if (!sfcb_open_loc(&sfcb, &loc, 0, sizeof(boot_count))) {
        (void)sfcb_write_loc(&loc, &boot_count, sizeof(boot_count));
        // remember the storage is not updated until the location is closed
        (void)sfcb_close_loc(&loc);
    }

    // release any resources we were using
    sfcb_unmount(&sfcb);

    // print the boot count
    printk("bootcount: %d\n", boot_count);
    return 0;
}
```

Using the high level API the example becomes:
``` c
#include <sfcb.h>
#include <printk.h>

// variables used by the filesystem
sfcb_fs sfcb;

// configuration of the filesystem is provided by this struct
const sfcb_fs_cfg cfg = {
	.offset = DT_FLASH_AREA_STORAGE_OFFSET,
	.sector_size = DT_FLASH_ERASE_BLOCK_SIZE,
	.sector_cnt = DT_FLASH_AREA_STORAGE_SIZE /
		      DT_FLASH_ERASE_BLOCK_SIZE,
	.dev_name = DT_FLASH_AREA_STORAGE_DEV,
};

// entry point
int main(void) {
    // mount the filesystem
    int rc;
    u32_t boot_count = 0;

    sfcb.cfg = &cfg;
    rc = sfcb_mount(&sfcb);

    // reformat if we can't mount the filesystem
    if (rc) {
        sfcb_format(&sfcb);
        sfcb_mount(&sfcb);
    }

    // read current count
    (void)sfcb_read(&sfcb, 0, &boot_count, sizeof(boot_count));

    // update boot count
    boot_count += 1;
    (void)sfcb_write(&sfcb, 0, &boot_count, sizeof(boot_count));

    // release any resources we were using
    sfcb_unmount(&sfcb);

    // print the boot count
    printk("boot_count: %d\n", boot_count);
}
```

## Kconfig options

sfcb has several configuration options that allows finetuning for specific
applications.

```
config SFCB_WBS
	int "SFCB write block size"
	range 1 128
	default 8
	help
	  SFCB will align writes to this size, it should be a power of 2 and a
	  multiple of the flash device write block size. Set it as low as
	  possible to reduce stack usage and to maximize flash utilization.

config SFCB_ATE_CACHE_SIZE
	int "SFCB ATE cache size (ATE count)"
	range 1 32
	default 1
	help
	  SFCB can use a cache mechanism to reduce the reads in the ATE region.
	  When a cache size bigger than 1 is selected a read in the ATE region
	  results in multiple ATE's being read in the ate_cache, this cache is
	  then searched for a next ate. If the cache is exhausted a next read
	  from flash is performed. The cache size is internally limited to 128
	  bytes.

config SFCB_ENABLE_CFG_CHECK
	bool "SFCB enable configuration check"
	depends on FLASH_PAGE_LAYOUT
	default y
	help
	  When SFCB mounts a file system the supplied configuration is checked.
	  This is especially good during development but takes code space. Once
	  you are sure that the configuration is good you can disable the
	  configuration check. Leave enabled if unsure.

config SFCB_FLASH_SUPPORTS_UNALIGNED_READ
	bool "SFCB flash backend supports unaligned read"
	default n
	help
	  When the flash used supports unaligned reads SFCB can use this,
	  enabling this option will reduce code. Leave disabled if unsure.

config SFCB_FLASH_SUPPORTS_UNALIGNED_WRITE
	bool "SFCB flash backend supports unaligned write"
	default n
	help
	  When the flash used supports unaligned writes SFCB can use this,
	  enabling this option will reduce code. Leave disabled if unsure.
```

## Design

Sfcb stored data in a flash area. The flash area is divided into sectors, in
each of the sectors data is stored as an allocation entry (ATE) that contains
the data id, data length and a data offset (where the data starts in a sector).

A sfcb sector starts with a sector start block that contains the sector id (a
automatically increased number) and a sector magic that identifies the type.

A sfcb file system looks like:

```
 Sector 1                    Sector 2
-------------------------   -------------------------
|START0|DATA0   ...     |   |START1|DATA2 ...       |
|                       |   |                       |
|            |DATA1     |   |                       |   ...
|  |XXXXXXXXXXXXXXXXXXXX|   | |DATA3|DATA4|DATA5|XXX|
|XXXXXXXXXXXXXXXXXXXXXXX|   |XXXXXXXXXXXXXXXXXXXXXXX|
|XXXXXXXXXXXXX|ATE1|ATE0|   |XXX|ATE5|ATE4|ATE3|ATE2|
-------------------------   -------------------------

X: free (unwritten) flash memory
```

In a sfcb sector ate's are written from the end of the sector, while data is
written from the start of a sector.

Whenever a request for a write is made sfcb checks if there is enough space to
put the data and the ate between the end of the previous data and the start of
the previous ate. If there is insufficient space a new sector is started. A new
sector is started by erasing the flash in the sector and writing a sector start.
After this the new data and ATE can be written. To make sure the data is valid
the ATE is only written after the data write. To make sure the ATE is valid the
ATE includes a crc8 calculated over the ATE.

When writing data eventually all sectors will be used. Requesting a new sector
will then result in erasing older data. In cases were it is required to maintain
old information a compression routine can be defined that is started just after
erasing a sector. The compression routine can then be used to copy the old data
that is still needed.

## Compress

Whenever a new sector is started a compression routine is called to copy data
from the start of the filesystem to the end. A typical application might be to
keep at least one id-value pair (the newest), this is similar to nvs. This
behavior can be achieved as:

```
int compress(sfcb_fs *fs)
{
	int rc;
	sfcb_loc loc_compress, loc_walk;
	sfcb_ate *ate_compress, *ate_walk;
	bool copy;
	u16_t compress_sector;

	if (sfcb_first_loc(fs, &loc_compress)) {
        /* if there is no data do nothing */
		return 0;
	}

    (void)sfcb_compress_sector(fs, &compress_sector);

	while (loc_compress.sector == compress_sector) {
		copy = true;
		ate_compress = sfcb_get_ate(&loc_compress);
		loc_walk = loc_compress;

    	while (!sfcb_next_loc(&loc_walk)) {
			ate_walk = sfcb_get_ate(&loc_walk);
			if (ate_compress->id == ate_walk->id) {
                /* found something with the same id later in the fs */
				copy = false;
				break;
			}
		}

		if ((copy) && (ate_compress->len != 0)) {
			rc = sfcb_copy_loc(&loc_compress);
			if (rc) {
				return rc;
			}
		}

		if (sfcb_next_loc(&loc_compress)) {
			break;
		}
	}
	return 0;
}
```

and setting the sfcb compress routine:

```
sfcb.compress = &compress;
```

Other, more advanced compression techniques where filtering is done based upon
id and value are easily implemented by changing the compression routine.

## Testing

Sfcb comes with a test suite that can run on emulated (qemu_x86) or real
hardware. The test is found under the directory `test`.
