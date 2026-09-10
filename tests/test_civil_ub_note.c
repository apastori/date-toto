/*
 * Chain-of-thought (Step 1 — file scope):
 *
 * 1. Single responsibility: document out-of-range civil as UB; do not call.
 * 2. No syscalls.
 * 3. No heap.
 * 4. Not throughput-sensitive.
 * 5. C11.
 */

#include "test_civil_ub_note.h"

#include <stdio.h>

void test_civil_ub_note(void)
{
    /*
     * API contract: month outside 1..12 or an invalid day for that month
     * is a precondition violation for days_from_civil / callers that claim
     * a civil triple — undefined behaviour. We do not invoke the helpers.
     */
    printf("PASS: out-of-range month/day is precondition violation (UB); "
           "not exercised\n");
}
