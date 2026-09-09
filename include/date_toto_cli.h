#ifndef DATE_TOTO_CLI_H
#define DATE_TOTO_CLI_H

/* ISO-8601 precision for -I / --iso-8601. */
enum date_toto_iso_prec {
    DATE_TOTO_ISO_NONE = -1,
    DATE_TOTO_ISO_DATE = 0,
    DATE_TOTO_ISO_HOURS = 1,
    DATE_TOTO_ISO_MINUTES = 2,
    DATE_TOTO_ISO_SECONDS = 3
};

struct date_toto_options {
    int utc;
    const char *date_string;
    const char *ref_file;
    const char *format; /* points into argv or static implied format */
    int iso_prec;       /* DATE_TOTO_ISO_* */
    int rfc_email;
};

void scan_meta_flags(int argc, char **argv, int *help, int *version);
void print_help(void);
void print_version(void);
int install_sigpipe_ignore(void);

/*
 * parse_options — fill opts from argv after meta-flag handling.
 *
 * Returns: 0 on success, -1 on usage error (message already emitted).
 */
int parse_options(int argc, char **argv, struct date_toto_options *opts);

const char *date_toto_iso_format(int iso_prec);
const char *date_toto_rfc_format(void);

#endif /* DATE_TOTO_CLI_H */
