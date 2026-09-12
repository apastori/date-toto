/*
 * Chain-of-thought (Step 1 — before code):
 *
 * 1. Single responsibility: argv dispatch (help/version), SIGPIPE setup,
 *    resolve the instant (-d / -r / now), obtain zone info, format once,
 *    write once via date_toto_run().
 *
 * 2. Syscall failure modes: clock_gettime, localtime, stat, write, sigaction
 *    — reported via date_toto_emit_* and exit 1; EPIPE → exit 0.
 *
 * 3. Heap: optional one-shot malloc inside date_toto_format() only.
 *
 * 4. Throughput: single line output; raw write(2).
 *
 * 5. C11 + POSIX.1-2008 feature macros from the Makefile.
 */

#include "date_toto.h"

#include "date_toto_cli.h"
#include "date_toto_emit.h"
#include "date_toto_fmt.h"
#include "date_toto_parse.h"

#include <stdlib.h>

/*
 * main — argv dispatcher for help/version or one formatted date line.
 *
 * Preconditions: argc >= 1; argv[0..argc-1] valid C strings.
 * Postconditions: returns 0/1; may exit from date_toto_run on write/EPIPE.
 */
int main(int argc, char **argv)
{
    int want_help = 0;
    int want_version = 0;
    struct date_toto_options opts;
    long long now_epoch = 0;
    long long epoch = 0;
    long offset_sec = 0;
    char tz_abbr[64];
    char stack_buf[DATE_TOTO_FMT_STACK_CAP];
    char *out = NULL;
    size_t out_len = 0;
    void *heap = NULL;

    scan_meta_flags(argc, argv, &want_help, &want_version);

    if (want_help) {
        print_help();
        return DATE_TOTO_EXIT_OK;
    }

    if (want_version) {
        print_version();
        return DATE_TOTO_EXIT_OK;
    }

    if (install_sigpipe_ignore() != 0) {
        return DATE_TOTO_EXIT_ERR;
    }

    if (parse_options(argc, argv, &opts) != 0) {
        return DATE_TOTO_EXIT_ERR;
    }

    //Save the current epoch time (number of seconds since 1970-01-01 00:00:00 UTC Unix epoch)
    if (date_toto_now_epoch(&now_epoch) != 0) {
        return DATE_TOTO_EXIT_ERR;
    }

    if (opts.date_string != NULL) {
        if (date_toto_parse_date(opts.date_string, now_epoch, opts.utc,
                                 &epoch) != 0) {
            return DATE_TOTO_EXIT_ERR;
        }
    } 
    
    if (opts.date_string == NULL && opts.ref_file != NULL) {
        if (date_toto_parse_reference(opts.ref_file, &epoch) != 0) {
            return DATE_TOTO_EXIT_ERR;
        }
    } 

    if (opts.date_string == NULL && opts.ref_file == NULL) {
        epoch = now_epoch;
    }

    if (date_toto_zone_info(epoch, opts.utc, &offset_sec, tz_abbr,
                            sizeof(tz_abbr)) != 0) {
        return DATE_TOTO_EXIT_ERR;
    }

    if (date_toto_format(opts.format, epoch, offset_sec, tz_abbr, stack_buf,
                         sizeof(stack_buf), &out, &out_len, &heap) != 0) {
        date_toto_emit_error("date_toto_format");
        return DATE_TOTO_EXIT_ERR;
    }

    date_toto_run(out, out_len);

    if (heap != NULL) {
        free(heap);
    }
    
    return DATE_TOTO_EXIT_OK;
}
