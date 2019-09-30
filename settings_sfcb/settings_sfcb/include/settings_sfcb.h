/*
 * Copyright (c) 2019 Laczen
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SETTINGS_SFCB_H_
#define __SETTINGS_SFCB_H_

#include <sfcb.h>
#include <settings/settings.h>


#ifdef __cplusplus
extern "C" {
#endif

#define SETTINGS_SFCB_ID 0xffff

struct settings_sfcb {
	struct settings_store cf_store;
	sfcb_fs *cf_sfcb;
};

/* register sfcb to be a source of settings */
int settings_sfcb_src(struct settings_sfcb *cf);

/* register sfcb to be the destination of settings */
int settings_sfcb_dst(struct settings_sfcb *cf);

/* Initialize a sfcb backend. */
int settings_sfcb_backend_init(struct settings_sfcb *cf);


#if defined(CONFIG_SETTINGS_SFCB_DEFAULT)
int settings_backend_init(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SETTINGS_SFCB_H_ */
