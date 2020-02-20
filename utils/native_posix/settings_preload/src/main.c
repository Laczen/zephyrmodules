/*
 * Copyright (c) 2020 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CONFIG_ARCH_POSIX
#error("Application only supports native posix architecture")
#endif

#include <zephyr.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <settings/settings.h>

#include "cmdline.h"
#include "soc.h"
#include "posix_board_if.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

#define MAX_BYTE_ARRAY_SIZE 256

static const char *preload_name;
static const char *preload_value;

static struct hex_value {
	u8_t value[MAX_BYTE_ARRAY_SIZE];
	size_t len;
} hex_value;

static void preload_native_posix_options(void)
{
	static struct args_struct_t preload_options[] = {
		{ .manual = false,
		  .is_mandatory = false,
		  .is_switch = false,
		  .option = "pname",
		  .name = "name",
		  .type = 's',
		  .dest = (void *)&preload_name,
		  .call_when_found = NULL,
		  .descript = "Name of the preload parameter" },
		{ .manual = false,
		  .is_mandatory = false,
		  .is_switch = false,
		  .option = "pvalue",
		  .name = "value",
		  .type = 's',
		  .dest = (void *)&preload_value,
		  .call_when_found = NULL,
		  .descript = "Value of the preload parameter" },
		ARG_TABLE_ENDMARKER
	};

	native_add_command_line_opts(preload_options);
}


NATIVE_TASK(preload_native_posix_options, PRE_BOOT_1, 1);

int str2bytearray(const char *string, struct hex_value *hex_value)
{
	unsigned const char *pos = string;
    	char *endptr;
    	size_t count = 0, len;

	len = strlen(string)/2;
	LOG_INF("Data Len: %u", len);

	if ((string[0] == '\0') || (strlen(string) != 2*len) ||
	    (len > MAX_BYTE_ARRAY_SIZE)) {
        	/* string contains no data, has a odd length or is to big */
		LOG_ERR("Malformed Value");
		hex_value->len = 0;
        	return -EINVAL;
   	}

	while (count < len) {
        	char buf[5] = {'0', 'x', pos[0], pos[1], 0};
        	hex_value->value[count] = strtol(buf, &endptr, 0);
        	pos += 2 * sizeof(char);

        	if (endptr[0] != '\0') {
            		/* non-hexadecimal character encountered */
			LOG_ERR("Malformed Value");
	    		hex_value->len = 0;
        		return -EINVAL;
        	}
		count++;
    	}
	hex_value->len = len;
	return 0;
}

void main(void)
{
	int rc;

	if (settings_subsys_init()) {
		LOG_ERR("Failed to initialize settings subsystem");
	}

	if (!strncmp(preload_value, "0x", 2)) {
		rc = str2bytearray(preload_value+2, &hex_value);
		if (rc) {
			posix_exit(rc);
		}
		rc = settings_save_one(preload_name, hex_value.value,
				       hex_value.len);
		if (rc) {
			LOG_ERR("Failed to save");
			posix_exit(rc);
		}
	} else {
		rc = settings_save_one(preload_name, preload_value,
				       strlen(preload_value));
		if (rc) {
			LOG_ERR("Failed to save");
			posix_exit(rc);
		}
	}
	LOG_INF("Stored %s to %s", preload_value, preload_name);
	posix_exit(0);
}
