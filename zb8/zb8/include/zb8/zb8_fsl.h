/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef H_ZB_EIGHT_FSL_
#define H_ZB_EIGHT_FSL_

#include <zephyr.h>
#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FSL_MAGIC	0x46534c48 /* FSLH in hex */
#define FSL_VER_MAGIC 	0x56455249 /* VERI in hex */
#define FSL_EMPTY_U32	0xffffffff

struct zb_fsl_hdr_info {
	u16_t size;	/* Header size */
	u8_t sigtype;	/* Header signature type */
	u8_t siglen;	/* Header signature length */
};

struct zb_fsl_ver {
	u8_t major;
	u8_t minor;
	u16_t rev;
};

struct zb_fsl_hdr {
	u32_t magic;		/* Image magic */
	u32_t upload_offset;	/* Where should the image be loaded */
	__packed struct zb_fsl_hdr_info hdr_info;
	u32_t size; 		/* Image size excluding header */
	u32_t run_offset;	/* Where will the image be "run" from */
	__packed struct zb_fsl_ver version;
	u32_t build;
	u32_t pad0;
};

struct zb_fsl_verify_hdr {
	u32_t crc32;
	u32_t pad0;
	u32_t pad1;
	u32_t pad2;
	u32_t pad3;
	u32_t pad4;
	u32_t pad5;
	u32_t pad6;
};

void zb_fsl_jump_fsl(void);
void zb_fsl_jump_swpr(void);
void zb_fsl_jump_ldr(void);
void zb_fsl_jump_run(void);
void zb_fsl_boot(void);

#ifdef __cplusplus
}
#endif

#endif