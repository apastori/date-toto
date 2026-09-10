/*
 * Chain-of-thought (Step 1 — file scope):
 *
 * 1. Single responsibility: epoch 0 + offset 0 → 1970-01-01 Thursday.
 * 2. No syscalls.
 * 3. No heap.
 * 4. Not throughput-sensitive.
 * 5. C11 / assert().
 */

#include "test_civil_epoch_zero.h"

#include "date_toto.h"

#include <assert.h>
#include <stdio.h>

void test_civil_epoch_zero(void)
{
    struct date_toto_tm tm;

    date_toto_break_down(0, 0, &tm);

    assert(tm.year == 1970);
    assert(tm.month == 1u);
    assert(tm.day == 1u);
    assert(tm.hour == 0);
    assert(tm.min == 0);
    assert(tm.sec == 0);
    assert(tm.wday == 4);
    assert(tm.yday == 1);

    printf("PASS: epoch zero is 1970-01-01 00:00:00 wday=4 yday=1\n");
}
