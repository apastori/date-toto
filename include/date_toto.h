/*
 * Chain-of-thought (Step 1 — before code):
 *
 * 1. Single responsibility: public contract for date-toto — buffer sizes,
 *    version, default format, exit codes, civil/epoch helpers, and
 *    date_toto_run() declaration.
 *
 * 2. Syscalls: none in this header. Civil helpers are pure arithmetic.
 *
 * 3. Heap: none. All helpers are allocation-free static inline.
 *
 * 4. Throughput: one formatted line; 4 KiB stack slab is ample.
 *
 * 5. Standard: C11 — static inline, long long for day counts.
 */

#ifndef DATE_TOTO_H
#define DATE_TOTO_H

#include <stddef.h>

/* Output / format stack slab (4 KiB). */
#define DATE_TOTO_OUT_SIZE 4096u
#define DATE_TOTO_FMT_STACK_CAP 4096u

#define DATE_TOTO_VERSION_STRING "1.0.0"

/* GNU-like default (C locale English names via our engine). */
#define DATE_TOTO_DEFAULT_FORMAT "%a %b %e %H:%M:%S %Z %Y"

enum {
    DATE_TOTO_EXIT_OK = 0,
    DATE_TOTO_EXIT_ERR = 1
};

/*
 * Broken-down civil time in the active zone (UTC when offset_sec is 0).
 * month 1..12; day 1..31; wday 0..6 (Sunday = 0); yday 1..366.
 */
struct date_toto_tm {
    int year;
    unsigned month;
    unsigned day;
    int hour;
    int min;
    int sec;
    int wday;
    int yday;
};

/*
 * date_toto_run — write `line` (length `line_len`, includes trailing '\n')
 * to stdout once.
 *
 * Preconditions: line != NULL; line_len > 0.
 * Postconditions: returns after a successful full write; on EPIPE exits 0;
 *                 on other write errors emits stderr and exits 1.
 */
void date_toto_run(const char *line, size_t line_len);

/*
 * days_from_civil — serial day number for civil (y, m, d).
 * Epoch day 0 is 1970-01-01. Algorithm: Howard Hinnant (public domain).
 *
 * Preconditions: m in 1..12; d valid for that month/year.
 * Postconditions: returns days since 1970-01-01 (may be negative).
 */
static inline long long days_from_civil(int y, unsigned m, unsigned d)
{
    y -= (m <= 2u);
    {
        const int era = (y >= 0 ? y : y - 399) / 400;
        const unsigned yoe = (unsigned)(y - era * 400);
        const unsigned doy =
            (153u * (m + (m > 2u ? -3 : 9)) + 2u) / 5u + d - 1u;
        const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
        return (long long)era * 146097LL + (long long)doe - 719468LL;
    }
}

/*
 * civil_from_days — inverse of days_from_civil.
 *
 * Preconditions: y, m, d non-NULL.
 * Postconditions: *y, *m (1..12), *d (1..31) set for serial day z.
 */
static inline void civil_from_days(long long z, int *y, unsigned *m,
                                   unsigned *d)
{
    z += 719468LL;
    {
        const long long era = (z >= 0 ? z : z - 146096LL) / 146097LL;
        const unsigned doe = (unsigned)(z - era * 146097LL);
        const unsigned yoe =
            (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
        const int y_ = (int)yoe + (int)era * 400;
        const unsigned doy =
            doe - (365u * yoe + yoe / 4u - yoe / 100u);
        const unsigned mp = (5u * doy + 2u) / 153u;
        *d = doy - (153u * mp + 2u) / 5u + 1u;
        *m = mp < 10u ? mp + 3u : mp - 9u;
        *y = *m <= 2u ? y_ + 1 : y_;
    }
}

/*
 * date_toto_break_down — convert Unix epoch + UTC offset to civil fields.
 *
 * Preconditions: out != NULL.
 * Postconditions: all fields normalised; wday in 0..6; yday in 1..366.
 */
static inline void date_toto_break_down(long long epoch, long offset_sec,
                                        struct date_toto_tm *out)
{
    long long local = epoch + (long long)offset_sec;
    long long days = local / 86400LL;
    long long rem = local % 86400LL;

    if (rem < 0) {
        rem += 86400LL;
        days -= 1;
    }

    civil_from_days(days, &out->year, &out->month, &out->day);
    out->hour = (int)(rem / 3600LL);
    out->min = (int)((rem % 3600LL) / 60LL);
    out->sec = (int)(rem % 60LL);

    {
        long long wd = (days + 4LL) % 7LL;
        if (wd < 0) {
            wd += 7LL;
        }
        out->wday = (int)wd;
    }

    out->yday =
        (int)(days - days_from_civil(out->year, 1u, 1u)) + 1;
}

#endif /* DATE_TOTO_H */
