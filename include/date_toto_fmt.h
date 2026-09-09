#ifndef DATE_TOTO_FMT_H
#define DATE_TOTO_FMT_H

#include <stddef.h>

/*
 * date_toto_format — expand `fmt` (no leading '+') for the given instant.
 *
 * Writes into *out / *out_len including a trailing '\n'.
 * If the result fits in stack_cap bytes of *stack_buf, *out points there
 * and *heap remains NULL. Otherwise one malloc is used (*heap holds it).
 *
 * Preconditions: fmt, out, out_len, stack_buf non-NULL; tz_abbr non-NULL;
 *                stack_cap >= 1.
 * Returns: 0 on success, -1 on OOM (errno set).
 */
int date_toto_format(const char *fmt, long long epoch, long offset_sec,
                     const char *tz_abbr, char *stack_buf, size_t stack_cap,
                     char **out, size_t *out_len, void **heap);

#endif /* DATE_TOTO_FMT_H */
