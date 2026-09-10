/*
 * Chain-of-thought (Step 1 — file scope):
 *
 * 1. Single responsibility: discover --help/--version, parse options,
 *    print help/version, install SIGPIPE ignore.
 * 2. sigaction: failure returns -1 with errno; emit helper reports it.
 * 3. No heap.
 * 4. Not hot-path.
 * 5. C11 + POSIX.1-2008 (sigaction); _WIN32 no-op for SIGPIPE.
 */

#include "date_toto_cli.h"

#include "date_toto.h"
#include "date_toto_emit.h"

#include <stdio.h>
#include <string.h>

#if !defined(_WIN32)
#include <signal.h>
#endif

#define DATE_TOTO_ARG_HELP         "--help"
#define DATE_TOTO_ARG_HELP_SHORT   "--h"
#define DATE_TOTO_ARG_VERSION      "--version"
#define DATE_TOTO_ARG_VERSION_SHORT "--v"

static const char ISO_DATE[] = "%Y-%m-%d";
static const char ISO_HOURS[] = "%Y-%m-%dT%H%:z";
static const char ISO_MINUTES[] = "%Y-%m-%dT%H:%M%:z";
static const char ISO_SECONDS[] = "%Y-%m-%dT%H:%M:%S%:z";
static const char RFC_EMAIL[] = "%a, %d %b %Y %H:%M:%S %z";

static int argv_has_exact(int argc, char **argv, const char *needle)
{
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], needle) == 0) {
            return 1;
        }
    }
    return 0;
}

void scan_meta_flags(int argc, char **argv, int *help, int *version)
{
    *help = argv_has_exact(argc, argv, DATE_TOTO_ARG_HELP)
         || argv_has_exact(argc, argv, DATE_TOTO_ARG_HELP_SHORT);
    *version = 0;
    if (*help) {
        return;
    }
    *version = argv_has_exact(argc, argv, DATE_TOTO_ARG_VERSION)
            || argv_has_exact(argc, argv, DATE_TOTO_ARG_VERSION_SHORT);
}

void print_help(void)
{
    printf(
        "Usage: date-toto [OPTION]... [+FORMAT]\n"
        "\n"
        "Display the current time in the given FORMAT, or set the format\n"
        "via -I / -R. Does not set the system clock.\n"
        "\n"
        "  -d, --date=STRING        display time described by STRING\n"
        "  -u, --utc, --universal   print Coordinated Universal Time\n"
        "  -r, --reference=FILE     display the last modification time of FILE\n"
        "  -I, --iso-8601[=FMT]     ISO 8601 output (date|hours|minutes|seconds)\n"
        "  -R, --rfc-email          RFC 5322 output\n"
        "  --help, --h              display this help and exit\n"
        "  --version, --v           display version and exit\n"
        "\n"
        "FORMAT controls the output via %% directives (see documentation).\n"
    );
}

void print_version(void)
{
    printf("date-toto %s\n", DATE_TOTO_VERSION_STRING);
}

int install_sigpipe_ignore(void)
{
#if defined(_WIN32)
    return 0;
#else
    {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = SIG_IGN;
        if (sigaction(SIGPIPE, &sa, NULL) != 0) {
            date_toto_emit_error("sigaction(SIGPIPE)");
            return -1;
        }
        return 0;
    }
#endif
}

const char *date_toto_iso_format(int iso_prec)
{
    switch (iso_prec) {
    case DATE_TOTO_ISO_DATE:
        return ISO_DATE;
    case DATE_TOTO_ISO_HOURS:
        return ISO_HOURS;
    case DATE_TOTO_ISO_MINUTES:
        return ISO_MINUTES;
    case DATE_TOTO_ISO_SECONDS:
        return ISO_SECONDS;
    default:
        return NULL;
    }
}

const char *date_toto_rfc_format(void)
{
    return RFC_EMAIL;
}

static int parse_iso_prec(const char *prec, int *out)
{
    if (prec == NULL || prec[0] == '\0' || strcmp(prec, "date") == 0) {
        *out = DATE_TOTO_ISO_DATE;
        return 0;
    }
    if (strcmp(prec, "hours") == 0) {
        *out = DATE_TOTO_ISO_HOURS;
        return 0;
    }
    if (strcmp(prec, "minutes") == 0) {
        *out = DATE_TOTO_ISO_MINUTES;
        return 0;
    }
    if (strcmp(prec, "seconds") == 0) {
        *out = DATE_TOTO_ISO_SECONDS;
        return 0;
    }
    return -1;
}

static int format_already_chosen(const struct date_toto_options *opts)
{
    if (opts->format != NULL) {
        return 1;
    }
    if (opts->iso_prec != DATE_TOTO_ISO_NONE) {
        return 1;
    }
    if (opts->rfc_email) {
        return 1;
    }
    return 0;
}

static void init_options(struct date_toto_options *opts)
{
    opts->utc = 0;
    opts->date_string = NULL;
    opts->ref_file = NULL;
    opts->format = NULL;
    opts->iso_prec = DATE_TOTO_ISO_NONE;
    opts->rfc_email = 0;
}

/*
 * parse_options — fill opts; returns 0 or -1 with message emitted.
 */
int parse_options(int argc, char **argv, struct date_toto_options *opts)
{
    int i;

    init_options(opts);

    for (i = 1; i < argc; i++) {
        const char *single_arg = argv[i];

        // --help OR -h, --version OR -v handled earlier.
        if (strcmp(single_arg, DATE_TOTO_ARG_HELP) == 0
            || strcmp(single_arg, DATE_TOTO_ARG_HELP_SHORT) == 0
            || strcmp(single_arg, DATE_TOTO_ARG_VERSION) == 0
            || strcmp(single_arg, DATE_TOTO_ARG_VERSION_SHORT) == 0) {
            continue;
        }

        // -u, --utc, --universal
        if (strcmp(single_arg, "-u") == 0 || strcmp(single_arg, "--utc") == 0
            || strcmp(single_arg, "--universal") == 0) {
            opts->utc = 1;
            continue;
        }

        // -R, --rfc-email
        if (strcmp(single_arg, "-R") == 0 || strcmp(single_arg, "--rfc-email") == 0) {
            if (format_already_chosen(opts)) {
                date_toto_emit_msg("multiple output formats specified");
                return -1;
            }
            opts->rfc_email = 1;
            continue;
        }

        // -d, --date
        if (strcmp(single_arg, "-d") == 0 || strcmp(single_arg, "--date") == 0) {

            if (opts->date_string != NULL || opts->ref_file != NULL) {
                date_toto_emit_msg(
                    "the options may not be used together: -d --date / -r --reference");
                return -1;
            }

            if (i + 1 >= argc) {
                date_toto_emit_msg("option requires an argument -- 'd'");
                return -1;
            }

            //captures next argument as date string
            opts->date_string = argv[++i];
            continue;
        }

        // --date=STRING
        if (strncmp(single_arg, "--date=", 7) == 0) {
            if (opts->date_string != NULL || opts->ref_file != NULL) {
                date_toto_emit_msg(
                    "the options may not be used together: -d --date / -r --reference");
                return -1;
            }
            opts->date_string = single_arg + 7;
            continue;
        }

        // -r, --reference
        if (strcmp(single_arg, "-r") == 0 || strcmp(single_arg, "--reference") == 0) {
            if (opts->date_string != NULL || opts->ref_file != NULL) {
                date_toto_emit_msg(
                    "the options may not be used together: -d --date / -r --reference");
                return -1;
            }
            if (i + 1 >= argc) {
                date_toto_emit_msg("option requires an argument -- 'r'");
                return -1;
            }
            opts->ref_file = argv[++i];
            continue;
        }

        // --reference=FILE
        if (strncmp(single_arg, "--reference=", 12) == 0) {
            if (opts->date_string != NULL || opts->ref_file != NULL) {
                date_toto_emit_msg(
                    "the options may not be used together: -d --date / -r --reference");
                return -1;
            }
            opts->ref_file = single_arg + 12;
            continue;
        }

        // -I, --iso-8601
        if (strcmp(single_arg, "-I") == 0 || strcmp(single_arg, "--iso-8601") == 0) {
            if (format_already_chosen(opts)) {
                date_toto_emit_msg("multiple output formats specified");
                return -1;
            }
            opts->iso_prec = DATE_TOTO_ISO_DATE;
            continue;
        }

        // -I=FMT, --iso-8601=FMT
        if (strncmp(single_arg, "-I", 2) == 0 && single_arg[2] != '\0') {
            int prec;
            if (format_already_chosen(opts)) {
                date_toto_emit_msg("multiple output formats specified");
                return -1;
            }
            if (parse_iso_prec(single_arg + 2, &prec) != 0) {
                date_toto_emit_msg("invalid --iso-8601 argument");
                return -1;
            }
            opts->iso_prec = prec;
            continue;
        }

        // --iso-8601=FMT
        if (strncmp(single_arg, "--iso-8601=", 11) == 0) {
            int prec;
            if (format_already_chosen(opts)) {
                date_toto_emit_msg("multiple output formats specified");
                return -1;
            }
            if (parse_iso_prec(single_arg + 11, &prec) != 0) {
                date_toto_emit_msg("invalid --iso-8601 argument");
                return -1;
            }
            opts->iso_prec = prec;
            continue;
        }

        // +FORMAT
        if (single_arg[0] == '+' && opts->format == NULL
            && opts->iso_prec == DATE_TOTO_ISO_NONE && !opts->rfc_email) {
            if (format_already_chosen(opts)) {
                date_toto_emit_msg("multiple output formats specified");
                return -1;
            }
            opts->format = single_arg + 1;
            continue;
        }

        // invalid option
        if (single_arg[0] == '-') {
            date_toto_emit_msg("invalid option");
            return -1;
        }

        date_toto_emit_msg("extra operand");
        return -1;
    }

    if (opts->rfc_email) {
        opts->format = date_toto_rfc_format();
    } 
    
    if (!opts->rfc_email && opts->iso_prec != DATE_TOTO_ISO_NONE) {
        opts->format = date_toto_iso_format(opts->iso_prec);
    } 
    
    if (opts->format == NULL) {
        opts->format = DATE_TOTO_DEFAULT_FORMAT;
    }
    
    return 0;
}
