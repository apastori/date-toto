/*
 * Chain-of-thought (Step 1 — file scope):
 *
 * 1. Single responsibility: format error lines on stderr.
 * 2. No syscalls here; strerror reads errno set by the caller.
 * 3. No heap beyond fprintf’s stdio internals.
 * 4. Not throughput-sensitive.
 * 5. C11 via the project Makefile (-std=c11).
 */

#include "date_toto_emit.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

/*
 * date_toto_emit_error — print `date-toto: context: strerror(errno)`.
 *
 * Preconditions: context != NULL; errno reflects the failed syscall/setup.
 * Postconditions: exactly one line on stderr.
 */
void date_toto_emit_error(const char *context)
{
    fprintf(stderr, "date-toto: %s: %s\n", context, strerror(errno));
}

/*
 * date_toto_emit_invalid_date — print `date-toto: invalid date '...'`.
 *
 * Preconditions: date_string != NULL.
 * Postconditions: exactly one line on stderr.
 */
void date_toto_emit_invalid_date(const char *date_string)
{
    fprintf(stderr, "date-toto: invalid date '%s'\n", date_string);
}

/*
 * date_toto_emit_msg — print `date-toto: <message>` (no errno).
 *
 * Preconditions: message != NULL.
 * Postconditions: exactly one line on stderr.
 */
void date_toto_emit_msg(const char *message)
{
    fprintf(stderr, "date-toto: %s\n", message);
}
