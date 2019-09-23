/*  SFCB: simple flash circular buffer
 *
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_MODULE_SFCB_H_
#define ZEPHYR_INCLUDE_MODULE_SFCB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <kernel.h>
#include <device.h>
#include <flash.h>
#include <errno.h>
#include <crc.h>

#define SFCB_BLOCK_SIZE 32

/* Check that CONFIG_SFCB_WBS is power of 2 */
BUILD_ASSERT_MSG(((CONFIG_SFCB_WBS != 0) &&
                 (CONFIG_SFCB_WBS & (CONFIG_SFCB_WBS - 1))) == 0,
		 "CONFIG_SFCB_WBS must be power of 2");

/* forward declaration of sfcb_fs */
typedef struct sfcb_fs sfcb_fs;

/**
 * @brief simple flash circular buffer
 * @defgroup sfcb simple flash circular buffer
 * @ingroup file_system_storage
 * @{
 * @}
 */

/**
 * @brief simple flash circular buffer Data Structures
 * @defgroup sfcb_data_structures simple flash circular buffer Data Structures
 * @ingroup sfcb
 * @{
 */

/**
 * @brief SFCB File system configuration structure
 *
 * The SFCB file system is divided into sectors. As each sector is erased in one
 * operation the sector size should be a multiple of the flash page erase.
 *
 * @param offset: file system offset
 * @param sector_size: size of a sector
 * @param sector_cnt: number of sectors in file system
 * @param dev_name: name of the flash device
 */
typedef struct {
	off_t offset;
	u16_t sector_size;
	u16_t sector_cnt;
	char *dev_name;
} sfcb_fs_cfg;

#define SFCB_ATE_SIZE MAX(CONFIG_SFCB_WBS, 8)

BUILD_ASSERT_MSG(SFCB_ATE_SIZE % CONFIG_SFCB_WBS == 0,
		 "SFCB_ATE_SIZE must be multiple of CONFIG_SFCB_WBS");

/**
 * @brief SFCB Allocation Table Entry
 * @param id: data id
 * @param offset: data offset within sector
 * @param len: data length
 * @param pad8: pads to fill up - unused
 * @param crc8: CRC8 check of the Allocation TAble Entry
 */
typedef struct {
	u16_t id;
	u16_t offset;
	u16_t len;
	u8_t pad8[SFCB_ATE_SIZE - 7];
	u8_t crc8;
} __packed sfcb_ate;

#define SFCB_SEC_START_SIZE MAX(CONFIG_SFCB_WBS, 8)

BUILD_ASSERT_MSG(SFCB_SEC_START_SIZE % CONFIG_SFCB_WBS == 0,
		 "SFCB_SEC_START_SIZE must be multiple of CONFIG_SFCB_WBS");

#define SFCB_MAGIC 0x73666362
#define SFCB_VERSION 0x00
/**
 * @brief SFCB Sector start
 * @param sfcb magic
 * @param sec_id: sector number
 * @param version: sfcb version
 * @param crc8: CRC8 check of the Sector start
 */
typedef struct {
	u32_t magic;
	u16_t sec_id;
	u8_t version;
	u8_t crc8;
} __packed sfcb_sec_start;

#define SFCB_ATE_CACHE_BYTES MIN(128, SFCB_ATE_SIZE*CONFIG_SFCB_ATE_CACHE_SIZE)

/**
 * @brief SFCB location
 *
 */
typedef struct {
	u16_t sector;
	u16_t sector_id;
	u16_t data_offset;
	u16_t ate_offset;
#if (CONFIG_SFCB_ATE_CACHE_SIZE != 1)
	u16_t ate_cache_offset;
#endif
	u8_t  ate_cache[SFCB_ATE_CACHE_BYTES];
#if IS_ENABLED(CONFIG_SFCB_FLASH_SUPPORTS_UNALIGNED_WRITE)
#else
	u8_t  dcache[CONFIG_SFCB_WBS];
#endif
	sfcb_fs *fs;
} sfcb_loc;

/**
 * @brief SFCB File system structure
 *
 * @param wr_sector: write sector
 * @param wr_sector_id: write sector id
 * @param data_wr_offset: data write offset in sector
 * @param ate_wr_offset: ATE write offset in sector
 * @param flash_device: flash device, set to NULL in case of cfg error
 * @param wr_lock: mutex locked during write
 * @param compress: pointer to garbage collection routine supplied by user
 * @param cfg: file system configuration
 */
struct sfcb_fs {
	u16_t wr_sector;
	u16_t wr_sector_id;
	u16_t wr_data_offset;
	u16_t wr_ate_offset;
	struct device *flash_device;
	struct k_mutex mutex;
	int (*compress)(sfcb_fs *fs);
	const sfcb_fs_cfg *cfg;
};

/**
 * @}
 */

/**
 * @brief SFCB APIs
 * @defgroup sfcb_api simple flash circular buffer APIs
 * @ingroup sfcb
 * @{
 */

inline sfcb_ate *sfcb_get_ate(sfcb_loc *loc)
{
#if (CONFIG_SFCB_ATE_CACHE_SIZE !=1)
	return (sfcb_ate *) (&loc->ate_cache[loc->ate_cache_offset]);
#else
	return (sfcb_ate *) (&loc->ate_cache[0]);
#endif
}

/**
 * @brief sfcb_mount
 *
 * Mounts a SFCB file system in flash.
 *
 * @param fs: Pointer to file system
 * @retval 0 Success
 * @retval -ERRNO errno code if error
 */
int sfcb_mount(sfcb_fs *fs);

/**
 * @brief sfcb_unmount
 *
 * Unmounts a SFCB file system.
 *
 * @param fs: Pointer to file system
 * @retval 0 Success
 * @retval -ERRNO errno code if error
 */
int sfcb_unmount(sfcb_fs *fs);

/**
 * @brief sfcb_format
 *
 * Formats a SFCB file system.
 *
 * @param fs: Pointer to file system
 * @retval 0 Success
 * @retval -ERRNO errno code if error
 */
int sfcb_format(sfcb_fs *fs);

/**
 * @brief sfcb_write(sfcb_fs *fs, u16_t id, const void *data, size_t len)
 *
 * Write data to sfcb filesystem.
 * @param id: identifier
 * @param data: pointer to data
 * @param len: bytes to write
 * @retval bytes written
 * @retval -ERRNO errno code if error
 */
ssize_t sfcb_write(sfcb_fs *fs, u16_t id, const void *data, size_t len);

/**
 * @brief sfcb_read(sfcb_fs *fs, u16_t id, void *data, size_t len)
 *
 * Read data from sfcb filesystem.
 * @param id: identifier
 * @param data: pointer to data
 * @param len: bytes to write
 * @retval bytes written
 * @retval -ERRNO errno code if error
 */
ssize_t sfcb_read(sfcb_fs *fs, u16_t id, void *data, size_t len);

/**
 * @brief sfcb_open_loc(sfcb_fs *fs, sfcb_loc *loc)
 *
 * Open a location in filesystem for writing.
 * @param fs: pointer to file system
 * @param loc: pointer to location
 * @param id: identifier
 * @param len: required storage length
 * @retval 0 Success
 * @retval -ERRNO errno code if error
 */
int sfcb_open_loc(sfcb_fs *fs, sfcb_loc *loc, u16_t id, u16_t len);

/**
 * @brief sfcb_close_loc(sfcb_loc *loc)
 *
 * Close a location in filesystem (writes last data and ate).
 * @param loc: pointer to location
 * @retval 0 Success
 * @retval -ERRNO errno code if error
 */
int sfcb_close_loc(sfcb_loc *loc);

/**
 * @brief sfcb_write_loc(sfcb_loc *loc, const void *data, size_t len)
 *
 * Write data to a location.
 * @param loc: pointer to location
 * @param data: pointer to data
 * @param len: bytes to write
 * @retval bytes written
 * @retval -ERRNO errno code if error
 */
ssize_t sfcb_write_loc(sfcb_loc *loc, const void *data, size_t len);

/**
 * @brief sfcb_read_loc(sfcb_loc *loc, void *data, size_t len)
 *
 * Read data from a location.
 * @param loc: pointer to location
 * @param data: pointer to data
 * @param len: bytes to read
 * @retval bytes read
 * @retval -ERRNO errno code if error
 */
ssize_t sfcb_read_loc(sfcb_loc *loc, void *data, size_t len);

/**
 * @brief sfcb_copy_loc(sfcb_loc *loc)
 *
 * Copies the data at loc to the current write location.
 * @param loc: pointer to location
 * @retval 0 Succes
 * @retval -ERRNO errno code if error
 */
int sfcb_copy_loc(sfcb_loc *loc);

/**
 * @brief sfcb_next_loc(sfcb_loc *loc)
 *
 * Get next location in fs (from oldest to newest).
 * @param loc: pointer to location
 * @retval 0 Succes
 * @retval -ERRNO errno code if error
 */
int sfcb_next_loc(sfcb_loc *loc);

/**
 * @brief sfcb_first_loc(sfcb_fs *fs, sfcb_loc *loc)
 *
 * Get first location in fs.
 * @param fs: pointer to file system
 * @param loc: pointer to location
 * @retval 0 Succes
 * @retval -ERRNO errno code if error
 */
int sfcb_first_loc(sfcb_fs *fs, sfcb_loc *loc);

/**
 * @brief sfcb_compress_sector(sfcb_fs *fs, u16_t *sectors)
 *
 * Fills the compression sector in sector.
 * @param fs: pointer to file system
 * @param sector: pointer to sector
 * @retval 0 Succes
 * @retval -ERRNO errno code if error
 */
int sfcb_compress_sector(sfcb_fs *fs, u16_t *sector);

int sfcb_rewind_loc(sfcb_loc *loc);

int sfcb_setpos_loc(sfcb_loc *loc, u16_t pos);

/**
 * @}
 */



#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_SFCB_SFCB_H_ */
