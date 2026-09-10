/*
 * Chain-of-thought (Step 1 — file scope):
 *
 * 1. Single responsibility: leap-year day counts and Mar 1 yday.
 * 2. No syscalls.
 * 3. No heap.
 * 4. Not throughput-sensitive.
 * 5. C11 / assert().
 */

#include "test_civil_leap_year.h"

#include "date_toto.h"

#include <assert.h>
#include <stdio.h>

static int is_leap_year(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

void test_civil_leap_year(void)
{
    struct date_toto_tm tm;
    long long z;

    assert(!is_leap_year(1900));
    assert(is_leap_year(2000));
    assert(is_leap_year(2024));
    assert(!is_leap_year(2026));

    z = days_from_civil(2024, 2u, 29u);
    civil_from_days(z, &tm.year, &tm.month, &tm.day);
    assert(tm.year == 2024);
    assert(tm.month == 2u);
    assert(tm.day == 29u);

    date_toto_break_down(days_from_civil(2024, 3u, 1u) * 86400LL, 0, &tm);
    assert(tm.yday == 61);

    date_toto_break_down(days_from_civil(2026, 3u, 1u) * 86400LL, 0, &tm);
    assert(tm.yday == 60);

    printf("PASS: leap-year rules and Mar 1 yday\n");
}
