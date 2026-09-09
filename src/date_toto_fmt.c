/*
 * Chain-of-thought (Step 1 — file scope):
 *
 * 1. Single responsibility: expand +FORMAT (or implied ISO/RFC format)
 *    into a single line with trailing newline — own % engine, no strftime.
 * 2. Syscalls: none. Pure formatting from epoch + offset + tz abbr.
 * 3. Heap: one optional malloc when expansion exceeds DATE_TOTO_FMT_STACK_CAP.
 * 4. Output is one short line; stack slab of 4 KiB is the common path.
 * 5. C11.
 */

#include "date_toto_fmt.h"

#include "date_toto.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* One-shot heap ceiling if the stack slab is too small (no realloc). */
#define DATE_TOTO_FMT_HEAP_CAP 65536u

static const char *const WDAY_ABBR[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char *const WDAY_FULL[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};
static const char *const MON_ABBR[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static const char *const MON_FULL[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

struct fmt_buf {
    char *data;
    size_t len;
    size_t cap;
    int heap;
};

static int fmt_ensure(struct fmt_buf *b, size_t need)
{
    char *nbuf;

    if (b->len + need <= b->cap) {
        return 0;
    }
    if (b->heap) {
        errno = ENOMEM;
        return -1;
    }
    /* Promote once from stack to a fixed heap slab — no realloc. */
    if (b->len + need > DATE_TOTO_FMT_HEAP_CAP) {
        errno = ENOMEM;
        return -1;
    }
    nbuf = (char *)malloc(DATE_TOTO_FMT_HEAP_CAP);
    if (nbuf == NULL) {
        return -1;
    }
    if (b->len > 0u) {
        memcpy(nbuf, b->data, b->len);
    }
    b->data = nbuf;
    b->cap = DATE_TOTO_FMT_HEAP_CAP;
    b->heap = 1;
    return 0;
}

static int fmt_putc(struct fmt_buf *b, char c)
{
    if (fmt_ensure(b, 1u) != 0) {
        return -1;
    }
    b->data[b->len++] = c;
    return 0;
}

static int fmt_puts(struct fmt_buf *b, const char *s)
{
    size_t n = strlen(s);
    if (fmt_ensure(b, n) != 0) {
        return -1;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    return 0;
}

static int fmt_u_pad(struct fmt_buf *b, unsigned long long v, int width,
                     char pad)
{
    char tmp[32];
    int i = 0;
    int j;

    if (v == 0ull) {
        tmp[i++] = '0';
    } else {
        while (v > 0ull && i < (int)sizeof(tmp)) {
            tmp[i++] = (char)('0' + (v % 10ull));
            v /= 10ull;
        }
    }
    while (i < width) {
        if (fmt_putc(b, pad) != 0) {
            return -1;
        }
        width--;
    }
    for (j = i - 1; j >= 0; j--) {
        if (fmt_putc(b, tmp[j]) != 0) {
            return -1;
        }
    }
    return 0;
}

static int fmt_i_pad(struct fmt_buf *b, long long v, int width, char pad)
{
    if (v < 0) {
        if (fmt_putc(b, '-') != 0) {
            return -1;
        }
        /* width counts digits only for GNU %Y-like; keep simple. */
        return fmt_u_pad(b, (unsigned long long)(-v), width > 0 ? width - 1 : 0,
                         pad);
    }
    return fmt_u_pad(b, (unsigned long long)v, width, pad);
}

static int fmt_offset(struct fmt_buf *b, long offset_sec, int with_colon)
{
    long abs_off = offset_sec < 0 ? -offset_sec : offset_sec;
    long hh = abs_off / 3600L;
    long mm = (abs_off % 3600L) / 60L;

    if (fmt_putc(b, offset_sec < 0 ? '-' : '+') != 0) {
        return -1;
    }
    if (fmt_u_pad(b, (unsigned long long)hh, 2, '0') != 0) {
        return -1;
    }
    if (with_colon && fmt_putc(b, ':') != 0) {
        return -1;
    }
    return fmt_u_pad(b, (unsigned long long)mm, 2, '0');
}

static int fmt_expand(struct fmt_buf *b, const char *fmt, long long epoch,
                      long offset_sec, const char *tz_abbr)
{
    struct date_toto_tm tm;
    size_t i;

    date_toto_break_down(epoch, offset_sec, &tm);

    for (i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] != '%') {
            if (fmt_putc(b, fmt[i]) != 0) {
                return -1;
            }
            continue;
        }
        i++;
        if (fmt[i] == '\0') {
            if (fmt_putc(b, '%') != 0) {
                return -1;
            }
            break;
        }

        switch (fmt[i]) {
        case '%':
            if (fmt_putc(b, '%') != 0) {
                return -1;
            }
            break;
        case 'n':
            if (fmt_putc(b, '\n') != 0) {
                return -1;
            }
            break;
        case 't':
            if (fmt_putc(b, '\t') != 0) {
                return -1;
            }
            break;
        case 'Y':
            if (fmt_i_pad(b, tm.year, 4, '0') != 0) {
                return -1;
            }
            break;
        case 'y':
            if (fmt_u_pad(b, (unsigned long long)((tm.year % 100 + 100) % 100),
                          2, '0') != 0) {
                return -1;
            }
            break;
        case 'C':
            if (fmt_i_pad(b, tm.year / 100, 2, '0') != 0) {
                return -1;
            }
            break;
        case 'm':
            if (fmt_u_pad(b, tm.month, 2, '0') != 0) {
                return -1;
            }
            break;
        case 'd':
            if (fmt_u_pad(b, tm.day, 2, '0') != 0) {
                return -1;
            }
            break;
        case 'e':
            if (fmt_u_pad(b, tm.day, 2, ' ') != 0) {
                return -1;
            }
            break;
        case 'H':
            if (fmt_u_pad(b, (unsigned long long)tm.hour, 2, '0') != 0) {
                return -1;
            }
            break;
        case 'I': {
            int h12 = tm.hour % 12;
            if (h12 == 0) {
                h12 = 12;
            }
            if (fmt_u_pad(b, (unsigned long long)h12, 2, '0') != 0) {
                return -1;
            }
            break;
        }
        case 'M':
            if (fmt_u_pad(b, (unsigned long long)tm.min, 2, '0') != 0) {
                return -1;
            }
            break;
        case 'S':
            if (fmt_u_pad(b, (unsigned long long)tm.sec, 2, '0') != 0) {
                return -1;
            }
            break;
        case 'j':
            if (fmt_u_pad(b, (unsigned long long)tm.yday, 3, '0') != 0) {
                return -1;
            }
            break;
        case 's':
            if (fmt_i_pad(b, epoch, 0, '0') != 0) {
                return -1;
            }
            break;
        case 'u': {
            int u = tm.wday == 0 ? 7 : tm.wday;
            if (fmt_u_pad(b, (unsigned long long)u, 1, '0') != 0) {
                return -1;
            }
            break;
        }
        case 'w':
            if (fmt_u_pad(b, (unsigned long long)tm.wday, 1, '0') != 0) {
                return -1;
            }
            break;
        case 'a':
            if (fmt_puts(b, WDAY_ABBR[tm.wday]) != 0) {
                return -1;
            }
            break;
        case 'A':
            if (fmt_puts(b, WDAY_FULL[tm.wday]) != 0) {
                return -1;
            }
            break;
        case 'b':
        case 'h':
            if (fmt_puts(b, MON_ABBR[tm.month - 1u]) != 0) {
                return -1;
            }
            break;
        case 'B':
            if (fmt_puts(b, MON_FULL[tm.month - 1u]) != 0) {
                return -1;
            }
            break;
        case 'p':
            if (fmt_puts(b, tm.hour >= 12 ? "PM" : "AM") != 0) {
                return -1;
            }
            break;
        case 'P':
            if (fmt_puts(b, tm.hour >= 12 ? "pm" : "am") != 0) {
                return -1;
            }
            break;
        case 'Z':
            if (fmt_puts(b, tz_abbr) != 0) {
                return -1;
            }
            break;
        case 'z':
            if (fmt_offset(b, offset_sec, 0) != 0) {
                return -1;
            }
            break;
        case ':':
            if (fmt[i + 1] == 'z') {
                i++;
                if (fmt_offset(b, offset_sec, 1) != 0) {
                    return -1;
                }
            } else {
                /* Unknown %:X — emit verbatim. */
                if (fmt_putc(b, '%') != 0 || fmt_putc(b, ':') != 0) {
                    return -1;
                }
            }
            break;
        case 'F':
            if (fmt_expand(b, "%Y-%m-%d", epoch, offset_sec, tz_abbr) != 0) {
                return -1;
            }
            break;
        case 'T':
            if (fmt_expand(b, "%H:%M:%S", epoch, offset_sec, tz_abbr) != 0) {
                return -1;
            }
            break;
        case 'D':
            if (fmt_expand(b, "%m/%d/%y", epoch, offset_sec, tz_abbr) != 0) {
                return -1;
            }
            break;
        case 'R':
            if (fmt_expand(b, "%H:%M", epoch, offset_sec, tz_abbr) != 0) {
                return -1;
            }
            break;
        case 'c':
            if (fmt_expand(b, "%a %b %e %H:%M:%S %Y", epoch, offset_sec,
                           tz_abbr) != 0) {
                return -1;
            }
            break;
        case 'x':
            if (fmt_expand(b, "%m/%d/%y", epoch, offset_sec, tz_abbr) != 0) {
                return -1;
            }
            break;
        case 'X':
            if (fmt_expand(b, "%H:%M:%S", epoch, offset_sec, tz_abbr) != 0) {
                return -1;
            }
            break;
        default:
            /* Unknown directive: emit '%' and the character verbatim. */
            if (fmt_putc(b, '%') != 0 || fmt_putc(b, fmt[i]) != 0) {
                return -1;
            }
            break;
        }
    }
    return 0;
}

/*
 * date_toto_format — see include/date_toto_fmt.h.
 */
int date_toto_format(const char *fmt, long long epoch, long offset_sec,
                     const char *tz_abbr, char *stack_buf, size_t stack_cap,
                     char **out, size_t *out_len, void **heap)
{
    struct fmt_buf b;

    b.data = stack_buf;
    b.len = 0;
    b.cap = stack_cap;
    b.heap = 0;

    if (fmt_expand(&b, fmt, epoch, offset_sec, tz_abbr) != 0) {
        if (b.heap) {
            free(b.data);
        }
        return -1;
    }
    if (fmt_putc(&b, '\n') != 0) {
        if (b.heap) {
            free(b.data);
        }
        return -1;
    }

    *out = b.data;
    *out_len = b.len;
    *heap = b.heap ? b.data : NULL;
    return 0;
}
