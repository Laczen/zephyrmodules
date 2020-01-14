/*
 * Copyright (c) 2019 Laczen
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>

#include <zb8/zb8_tlv.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(test_zb_tlv);

extern u8_t test_msg[];
extern u8_t test_signature[];

#define HAYSTACK_BYTES 32
uint8_t test_haystack[HAYSTACK_BYTES] = {
	1,0,2,0, 1,2, 	/* type = 1, length = 2, value = 1 2 */
	2,0,3,0, 3,4,5, /* type = 2, length = 3, value = 3 4 5 */
	3,0,1,0, 6,	/* type = 3, length = 1, value = 6 */
	0,1,2,0, 7,8,	/* type = 0x10, length = 1, value = 7 8 */
	0,0		/* end the tlv area with a type zero */
};

/**
 * @brief Test tlv_step method to step through a haystack in ram
 */
void test_zb_tlv_step(void)
{
	int err;
	struct tlv_entry entry;
	u32_t offset;

	offset = 0;
	entry.type = 0;
	while ((!zb_step_tlv(test_haystack, &offset, &entry)) &&
	       (entry.type != 1));
	zassert_true(entry.type == 1, "Entry type 1 not found");
	err = memcmp(&test_haystack[4], entry.value, entry.length);
	zassert_true(err == 0, "Entry type 1 wrong value");

	offset = 0;
	entry.type = 0;
	while ((!zb_step_tlv(test_haystack, &offset, &entry)) &&
	       (entry.type != 2));
	zassert_true(entry.type == 2, "Entry type 2 not found");
	err = memcmp(&test_haystack[10], entry.value, entry.length);
	zassert_true(err == 0, "Entry type 2 wrong value");

	offset = 0;
	entry.type = 0;
	while ((!zb_step_tlv(test_haystack, &offset, &entry)) &&
	       (entry.type != 3));
	zassert_true(entry.type == 3, "Entry type 3 not found");
	err = memcmp(&test_haystack[17], entry.value, entry.length);
	zassert_true(err == 0, "Entry type 3 wrong value");

	offset = 0;
	entry.type = 0;
	while ((!zb_step_tlv(test_haystack, &offset, &entry)) &&
	       (entry.type != 0x100));
	zassert_true(entry.type == 0x100, "Entry type 0x100 not found");
	err = memcmp(&test_haystack[22], entry.value, entry.length);
	zassert_true(err == 0, "Entry type 0x100 wrong value");

	/* Look for non-existing entry */
	offset = 0;
	entry.type = 0;
	while ((!zb_step_tlv(test_haystack, &offset, &entry)) &&
	       (entry.type != 5));
	zassert_false(entry.type == 5, "Found non-existing entry");

}

void test_zb_tlv(void)
{
	ztest_test_suite(test_zb_tlv,
			 ztest_unit_test(test_zb_tlv_step)
			);

	ztest_run_test_suite(test_zb_tlv);
}
