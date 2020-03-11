/*
 * Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zb8/zb8_tlv.h>

#include <errno.h>
#include <string.h>

int zb_step_tlv(const void *data, u32_t *offset, struct tlv_entry *entry)
{
    	u8_t *p = (u8_t *)data;

    	p += *offset;

    	memcpy(&entry->type, p, sizeof(u16_t));

	if (entry->type == 0x0000) {
		return -ENOENT;
	}

	p += sizeof(u16_t);

    	memcpy(&entry->length, p, sizeof(u16_t));
	p += sizeof(u16_t);

	entry->value = p;

	*offset += entry->length + 2 * sizeof(u16_t);

	return 0;
}