/*
 * Chain-of-thought (Step 1 — file scope):
 *
 * 1. Single responsibility: days_from_civil / civil_from_days round-trip.
 * 2. No syscalls.
 * 3. No heap.
 * 4. Not throughput-sensitive.
 * 5. C11 / assert().
 */

#include "test_civil_roundtrip.h"

#include "date_toto.h"

#include <assert.h>
#include <stdio.h>

void test_civil_roundtrip(void)
{
    int y;
    unsigned m;
    unsigned d;

    for (y = 1970; y <= 2030; y++) {
        for (m = 1u; m <= 12u; m++) {
            unsigned last = (m == 2u)
                ? (((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) ? 29u : 28u)
                : (m == 4u || m == 6u || m == 9u || m == 11u ? 30u : 31u);
            for (d = 1u; d <= last; d++) {
                long long z = days_from_civil(y, m, d);
                int oy;
                unsigned om;
                unsigned od;
                civil_from_days(z, &oy, &om, &od);
                assert(oy == y);
                assert(om == m);
                assert(od == d);
            }
        }
    }

    printf("PASS: days_from_civil / civil_from_days round-trip 1970-2030\n");
}
