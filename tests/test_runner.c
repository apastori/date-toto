/*
 * Chain-of-thought (Step 1 — before code):
 *
 * 1. Single responsibility: invoke each civil/epoch unit test in order.
 * 2. Syscalls: none.
 * 3. Heap: none in runner.
 * 4. N/A.
 * 5. C11 — matches the main program; assert() lives in individual tests.
 */

#include "test_civil_epoch_zero.h"
#include "test_civil_leap_year.h"
#include "test_civil_offset.h"
#include "test_civil_roundtrip.h"
#include "test_civil_ub_note.h"

int main(void)
{
    test_civil_epoch_zero();
    test_civil_roundtrip();
    test_civil_leap_year();
    test_civil_offset();
    test_civil_ub_note();
    return 0;
}
