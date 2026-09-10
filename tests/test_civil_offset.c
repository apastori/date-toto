/*
 * Chain-of-thought (Step 1 — file scope):
 *
 * 1. Single responsibility: non-zero offsets shift civil fields / day.
 * 2. No syscalls.
 * 3. No heap.
 * 4. Not throughput-sensitive.
 * 5. C11 / assert().
 */

#include "test_civil_offset.h"

#include "date_toto.h"

#include <assert.h>
#include <stdio.h>

void test_civil_offset(void)
{
    struct date_toto_tm tm;

    /* Epoch 0 with -03:00 → 1969-12-31 21:00:00 */
    date_toto_break_down(0, -3 * 3600L, &tm);
    assert(tm.year == 1969);
    assert(tm.month == 12u);
    assert(tm.day == 31u);
    assert(tm.hour == 21);
    assert(tm.min == 0);
    assert(tm.sec == 0);

    /* Epoch 0 with +14:00 → 1970-01-01 14:00:00 */
    date_toto_break_down(0, 14 * 3600L, &tm);
    assert(tm.year == 1970);
    assert(tm.month == 1u);
    assert(tm.day == 1u);
    assert(tm.hour == 14);

    /* Near midnight UTC: 86400-1 with +1h stays same calendar day 23:59+1 */
    date_toto_break_down(86400LL - 1LL, 3600L, &tm);
    assert(tm.year == 1970);
    assert(tm.month == 1u);
    assert(tm.day == 2u);
    assert(tm.hour == 0);
    assert(tm.min == 59);
    assert(tm.sec == 59);

    printf("PASS: offsets shift fields and cross day boundaries\n");
}
