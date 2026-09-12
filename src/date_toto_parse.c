/*
 * Chain-of-thought (Step 1 — file scope):
 *
 * 1. Single responsibility: resolve “now”, -d date strings, -r file mtimes,
 *    and local UTC offset / timezone abbreviation (no tm_gmtoff).
 * 2. Syscalls: clock_gettime, localtime_r/localtime_s, stat.
 * 3. Heap: none.
 * 4. Not throughput-sensitive.
 * 5. C11 + POSIX.1-2008; _WIN32 guards for reentrant localtime.
 */

#include "date_toto_parse.h"

#include "date_toto.h"
#include "date_toto_emit.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static int is_leap(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int days_in_month(int y, unsigned m)
{
    static const int dim[] = {
        0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    if (m == 2u && is_leap(y)) {
        return 29;
    }
    if (m < 1u || m > 12u) {
        return 0;
    }
    return dim[m];
}

static long long civil_to_epoch(int y, unsigned m, unsigned d, int hh, int mm,
                                int ss, long offset_sec)
{
    long long days = days_from_civil(y, m, d);
    return days * 86400LL + (long long)hh * 3600LL + (long long)mm * 60LL
         + (long long)ss - (long long)offset_sec;
}

static void skip_ws(const char **p)
{
    while (**p != '\0' && isspace((unsigned char)**p)) {
        (*p)++;
    }
}

static int parse_int_n(const char **p, int n, int *out)
{
    int v = 0;
    int i;
    for (i = 0; i < n; i++) {
        if (!isdigit((unsigned char)(*p)[i])) {
            return -1;
        }
        v = v * 10 + ((*p)[i] - '0');
    }
    *p += n;
    *out = v;
    return 0;
}

static int parse_int_var(const char **p, int *out)
{
    int v = 0;
    int digits = 0;
    if (**p == '+' || **p == '-') {
        return -1;
    }
    while (isdigit((unsigned char)**p)) {
        v = v * 10 + (**p - '0');
        (*p)++;
        digits++;
        if (digits > 9) {
            return -1;
        }
    }
    if (digits == 0) {
        return -1;
    }
    *out = v;
    return 0;
}

static int parse_signed_ll(const char **p, long long *out)
{
    int neg = 0;
    long long v = 0;
    int digits = 0;

    if (**p == '-') {
        neg = 1;
        (*p)++;
    } 
    
    if (!neg && **p == '+') {
        (*p)++;
    }

    while (isdigit((unsigned char)**p)) {
        //Convert the character to an integer
        int dig = **p - '0';
        //Check if the number is too large to fit in a long long integer
        if (v > (LLONG_MAX - dig) / 10) {
            return -1;
        }
        v = v * 10 + dig;
        (*p)++;
        digits++;
    }
    //Check if the number is empty
    if (digits == 0) {
        return -1;
    }
    //If the number is negative, multiply it by -1
    if (neg) { 
        *out = v * -1LL;
    }
    //If the number is positive, set it to the value of v
    if (!neg) {
        *out = v;
    }
    return 0;
}

static int eq_ci(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int starts_ci(const char *s, const char *prefix)
{
    while (*prefix) {
        if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) {
            return 0;
        }
        s++;
        prefix++;
    }
    return 1;
}

static int localtime_fields(time_t t, struct tm *out)
{
#if defined(_WIN32)
    return localtime_s(out, &t) == 0 ? 0 : -1;
#else
    return localtime_r(&t, out) != NULL ? 0 : -1;
#endif
}

/*
 * date_toto_zone_info — see include/date_toto_parse.h.
 */
int date_toto_zone_info(long long epoch, int use_utc, long *offset_sec,
                        char *tz_abbr, size_t tz_cap)
{
    struct tm lt;
    time_t t;
    long long local_epoch;

    if (tz_cap < 2u) {
        errno = EINVAL;
        return -1;
    }

    if (use_utc) {
        *offset_sec = 0;
        (void)snprintf(tz_abbr, tz_cap, "%s", "UTC");
        return 0;
    }

    t = (time_t)epoch;
    if ((long long)t != epoch) {
        errno = EOVERFLOW;
        date_toto_emit_error("localtime");
        return -1;
    }

    if (localtime_fields(t, &lt) != 0) {
        date_toto_emit_error("localtime");
        return -1;
    }

    local_epoch = days_from_civil(lt.tm_year + 1900,
                                  (unsigned)(lt.tm_mon + 1),
                                  (unsigned)lt.tm_mday)
                * 86400LL
                + (long long)lt.tm_hour * 3600LL
                + (long long)lt.tm_min * 60LL
                + (long long)lt.tm_sec;

    *offset_sec = (long)(local_epoch - epoch);

    /*
     * Timezone abbreviation from libc: tzname on POSIX; _tzname on UCRT
     * (plain tzname is deprecated under MinGW UCRT and trips -Werror).
     */
    {
        const char *abbr = NULL;
#if defined(_WIN32)
        tzset();
        if (lt.tm_isdst > 0 && _tzname[1] != NULL && _tzname[1][0] != '\0') {
            abbr = _tzname[1];
        } else if (_tzname[0] != NULL && _tzname[0][0] != '\0') {
            abbr = _tzname[0];
        }
#else
        tzset();
        if (lt.tm_isdst > 0 && tzname[1] != NULL && tzname[1][0] != '\0') {
            abbr = tzname[1];
        } else if (tzname[0] != NULL && tzname[0][0] != '\0') {
            abbr = tzname[0];
        }
#endif
        if (abbr != NULL) {
            (void)snprintf(tz_abbr, tz_cap, "%s", abbr);
        } else {
            (void)snprintf(tz_abbr, tz_cap, "%s", "LOCAL");
        }
    }
    return 0;
}

int date_toto_now_epoch(long long *out_epoch)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        date_toto_emit_error("clock_gettime");
        return -1;
    }
    *out_epoch = (long long)ts.tv_sec;
    return 0;
}

int date_toto_parse_reference(const char *path, long long *out_epoch)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        date_toto_emit_error(path);
        return -1;
    }
    *out_epoch = (long long)st.st_mtime;
    return 0;
}

static int add_months(int *y, unsigned *m, unsigned *d, long long months)
{
    long long total = (long long)(*y) * 12LL + (long long)(*m - 1u) + months;
    int ny;
    unsigned nm;
    int dim;

    if (total < 0) {
        return -1;
    }
    ny = (int)(total / 12LL);
    nm = (unsigned)(total % 12LL) + 1u;
    dim = days_in_month(ny, nm);
    if ((int)*d > dim) {
        *d = (unsigned)dim;
    }
    *y = ny;
    *m = nm;
    return 0;
}

static int match_unit(const char **p, int *unit_kind)
{
    /* unit_kind: 0=sec 1=min 2=hour 3=day 4=week 5=month 6=year */
    const char *s = *p;

    if (starts_ci(s, "seconds")) {
        *unit_kind = 0;
        *p = s + 7;
        return 0;
    }
    if (starts_ci(s, "second")) {
        *unit_kind = 0;
        *p = s + 6;
        return 0;
    }
    if (starts_ci(s, "minutes")) {
        *unit_kind = 1;
        *p = s + 7;
        return 0;
    }
    if (starts_ci(s, "minute")) {
        *unit_kind = 1;
        *p = s + 6;
        return 0;
    }
    if (starts_ci(s, "hours")) {
        *unit_kind = 2;
        *p = s + 5;
        return 0;
    }
    if (starts_ci(s, "hour")) {
        *unit_kind = 2;
        *p = s + 4;
        return 0;
    }
    if (starts_ci(s, "days")) {
        *unit_kind = 3;
        *p = s + 4;
        return 0;
    }
    if (starts_ci(s, "day")) {
        *unit_kind = 3;
        *p = s + 3;
        return 0;
    }
    if (starts_ci(s, "weeks")) {
        *unit_kind = 4;
        *p = s + 5;
        return 0;
    }
    if (starts_ci(s, "week")) {
        *unit_kind = 4;
        *p = s + 4;
        return 0;
    }
    if (starts_ci(s, "months")) {
        *unit_kind = 5;
        *p = s + 6;
        return 0;
    }
    if (starts_ci(s, "month")) {
        *unit_kind = 5;
        *p = s + 5;
        return 0;
    }
    if (starts_ci(s, "years")) {
        *unit_kind = 6;
        *p = s + 5;
        return 0;
    }
    if (starts_ci(s, "year")) {
        *unit_kind = 6;
        *p = s + 4;
        return 0;
    }
    return -1;
}

static int apply_delta(long long *epoch, long offset_sec, long long n,
                       int unit_kind)
{
    struct date_toto_tm tm;

    if (unit_kind <= 4) {
        long long mul = 1;
        if (unit_kind == 0) {
            mul = 1;
        } else if (unit_kind == 1) {
            mul = 60;
        } else if (unit_kind == 2) {
            mul = 3600;
        } else if (unit_kind == 3) {
            mul = 86400;
        } else {
            mul = 7 * 86400;
        }
        *epoch += n * mul;
        return 0;
    }

    date_toto_break_down(*epoch, offset_sec, &tm);
    if (unit_kind == 5) {
        if (add_months(&tm.year, &tm.month, &tm.day, n) != 0) {
            return -1;
        }
    } else {
        if (add_months(&tm.year, &tm.month, &tm.day, n * 12LL) != 0) {
            return -1;
        }
    }
    *epoch = civil_to_epoch(tm.year, tm.month, tm.day, tm.hour, tm.min, tm.sec,
                            offset_sec);
    return 0;
}

static int parse_tz_suffix(const char **p, long *out_off, int *have_off)
{
    const char *s = *p;
    int sign = 1;
    int hh = 0;
    int mm = 0;

    *have_off = 0;
    if (*s == 'Z' || *s == 'z') {
        *out_off = 0;
        *have_off = 1;
        *p = s + 1;
        return 0;
    }
    if (*s != '+' && *s != '-') {
        return 0;
    }
    sign = (*s == '-') ? -1 : 1;
    s++;
    if (parse_int_n(&s, 2, &hh) != 0) {
        return -1;
    }
    if (*s == ':') {
        s++;
        if (parse_int_n(&s, 2, &mm) != 0) {
            return -1;
        }
    } else if (isdigit((unsigned char)*s)) {
        if (parse_int_n(&s, 2, &mm) != 0) {
            return -1;
        }
    }
    *out_off = (long)sign * ((long)hh * 3600L + (long)mm * 60L);
    *have_off = 1;
    *p = s;
    return 0;
}

static int parse_time_hms(const char **p, int *hh, int *mm, int *ss)
{
    if (parse_int_n(p, 2, hh) != 0) {
        return -1;
    }
    if (**p != ':') {
        return -1;
    }
    (*p)++;
    if (parse_int_n(p, 2, mm) != 0) {
        return -1;
    }
    *ss = 0;
    if (**p == ':') {
        (*p)++;
        if (parse_int_n(p, 2, ss) != 0) {
            return -1;
        }
    }
    if (*hh > 23 || *mm > 59 || *ss > 60) {
        return -1;
    }
    return 0;
}

static int try_parse_iso(const char *s, long long now_epoch, int use_utc,
                         long long *out_epoch)
{
    const char *p = s;
    int y = 0;
    int mo = 0;
    int d = 0;
    int hh = 0;
    int mi = 0;
    int ss = 0;
    int have_date = 0;
    int have_time = 0;
    long off = 0;
    int have_off = 0;
    long zone_off = 0;
    char tz[64];

    if (parse_int_n(&p, 4, &y) == 0 && *p == '-') {
        p++;
        if (parse_int_n(&p, 2, &mo) != 0) {
            return -1;
        }
        if (*p != '-') {
            return -1;
        }
        p++;
        if (parse_int_n(&p, 2, &d) != 0) {
            return -1;
        }
        have_date = 1;
        if (*p == 'T' || *p == 't' || *p == ' ') {
            p++;
            if (parse_time_hms(&p, &hh, &mi, &ss) != 0) {
                return -1;
            }
            have_time = 1;
        }
        if (parse_tz_suffix(&p, &off, &have_off) != 0) {
            return -1;
        }
        skip_ws(&p);
        if (*p != '\0') {
            return -1;
        }
    } else {
        p = s;
        if (parse_time_hms(&p, &hh, &mi, &ss) != 0) {
            return -1;
        }
        have_time = 1;
        if (parse_tz_suffix(&p, &off, &have_off) != 0) {
            return -1;
        }
        skip_ws(&p);
        if (*p != '\0') {
            return -1;
        }
    }

    if (date_toto_zone_info(now_epoch, use_utc, &zone_off, tz, sizeof(tz))
        != 0) {
        return -1;
    }

    if (!have_date) {
        struct date_toto_tm tm;
        date_toto_break_down(now_epoch, zone_off, &tm);
        y = tm.year;
        mo = (int)tm.month;
        d = (int)tm.day;
    }
    if (!have_time) {
        hh = mi = ss = 0;
    }
    if (mo < 1 || mo > 12 || d < 1 || d > days_in_month(y, (unsigned)mo)) {
        return -1;
    }

    {
        long use_off = have_off ? off : zone_off;
        *out_epoch = civil_to_epoch(y, (unsigned)mo, (unsigned)d, hh, mi, ss,
                                    use_off);
    }
    return 0;
}

static int try_parse_keyword_or_relative(const char *s, long long now_epoch,
                                         int use_utc, long long *out_epoch)
{
    const char *p = s;
    long zone_off = 0;
    char tz[64];
    long long epoch = now_epoch;
    int unit_kind;
    long long n;
    int ago = 0;
    int sign = 1;

    if (date_toto_zone_info(now_epoch, use_utc, &zone_off, tz, sizeof(tz))
        != 0) {
        return -1;
    }

    skip_ws(&p);
    if (*p == '\0') {
        return -1;
    }

    if (eq_ci(p, "now")) {
        *out_epoch = now_epoch;
        return 0;
    }
    if (eq_ci(p, "today")) {
        struct date_toto_tm tm;
        date_toto_break_down(now_epoch, zone_off, &tm);
        *out_epoch = civil_to_epoch(tm.year, tm.month, tm.day, 0, 0, 0,
                                    zone_off);
        return 0;
    }
    if (eq_ci(p, "yesterday")) {
        epoch = now_epoch - 86400LL;
        {
            struct date_toto_tm tm;
            date_toto_break_down(epoch, zone_off, &tm);
            *out_epoch = civil_to_epoch(tm.year, tm.month, tm.day, 0, 0, 0,
                                        zone_off);
        }
        return 0;
    }
    if (eq_ci(p, "tomorrow")) {
        epoch = now_epoch + 86400LL;
        {
            struct date_toto_tm tm;
            date_toto_break_down(epoch, zone_off, &tm);
            *out_epoch = civil_to_epoch(tm.year, tm.month, tm.day, 0, 0, 0,
                                        zone_off);
        }
        return 0;
    }

    /* next/last UNIT */
    if (starts_ci(p, "next ") || starts_ci(p, "last ")) {
        if (starts_ci(p, "next ")) {
            sign = 1;
            p += 5;
        } else {
            sign = -1;
            p += 5;
        }
        skip_ws(&p);
        if (match_unit(&p, &unit_kind) != 0) {
            return -1;
        }
        skip_ws(&p);
        if (*p != '\0') {
            return -1;
        }
        if (apply_delta(&epoch, zone_off, (long long)sign, unit_kind) != 0) {
            return -1;
        }
        *out_epoch = epoch;
        return 0;
    }

    /* [+|-]N unit[s] [ago] */
    p = s;
    skip_ws(&p);
    sign = 1;
    if (*p == '+') {
        p++;
    } else if (*p == '-') {
        sign = -1;
        p++;
    }
    {
        int ni = 0;
        if (parse_int_var(&p, &ni) != 0) {
            return -1;
        }
        n = (long long)ni * (long long)sign;
    }
    skip_ws(&p);
    if (match_unit(&p, &unit_kind) != 0) {
        return -1;
    }
    skip_ws(&p);
    if (starts_ci(p, "ago")) {
        ago = 1;
        p += 3;
        skip_ws(&p);
    }
    if (*p != '\0') {
        return -1;
    }
    if (ago) {
        n = -n;
    }
    if (apply_delta(&epoch, zone_off, n, unit_kind) != 0) {
        return -1;
    }
    *out_epoch = epoch;
    return 0;
}

/*
 * date_toto_parse_date — see include/date_toto_parse.h.
 */
int date_toto_parse_date(const char *date_string, long long now_epoch,
                         int use_utc, long long *out_epoch)
{
    const char *p = date_string;
    long long v;

    skip_ws(&p);
    if (*p == '@') {
        p++;
        if (parse_signed_ll(&p, &v) != 0) {
            date_toto_emit_invalid_date(date_string);
            return -1;
        }
        skip_ws(&p);
        if (*p != '\0') {
            date_toto_emit_invalid_date(date_string);
            return -1;
        }
        *out_epoch = v;
        return 0;
    }

    if (try_parse_iso(date_string, now_epoch, use_utc, out_epoch) == 0) {
        return 0;
    }

    if (try_parse_keyword_or_relative(date_string, now_epoch, use_utc,
                                      out_epoch) == 0) {
        return 0;
    }

    date_toto_emit_invalid_date(date_string);
    return -1;
}
