#ifndef DATE_TOTO_PARSE_H
#define DATE_TOTO_PARSE_H

#include <stddef.h>

/*
 * date_toto_zone_info — local UTC offset and abbreviation for `epoch`.
 *
 * With use_utc non-zero: *offset_sec = 0 and tz_abbr = "UTC".
 * Otherwise uses reentrant localtime and days_from_civil round-trip.
 *
 * Preconditions: tz_abbr != NULL; tz_cap >= 1.
 * Returns: 0 on success, -1 on failure (errno set where applicable).
 */
int date_toto_zone_info(long long epoch, int use_utc, long *offset_sec,
                        char *tz_abbr, size_t tz_cap);

/*
 * date_toto_parse_date — parse GNU-ish -d STRING relative to now_epoch.
 *
 * On success stores *out_epoch. On failure emits invalid-date and returns -1.
 *
 * Preconditions: date_string != NULL; out_epoch != NULL.
 */
int date_toto_parse_date(const char *date_string, long long now_epoch,
                         int use_utc, long long *out_epoch);

/*
 * date_toto_parse_reference — read FILE mtime into *out_epoch.
 *
 * Preconditions: path != NULL; out_epoch != NULL.
 * Returns: 0 on success, -1 on failure (emits error).
 */
int date_toto_parse_reference(const char *path, long long *out_epoch);

/*
 * date_toto_now_epoch — CLOCK_REALTIME seconds since Unix epoch.
 *
 * Returns: 0 on success, -1 on failure (emits error).
 */
int date_toto_now_epoch(long long *out_epoch);

#endif /* DATE_TOTO_PARSE_H */
