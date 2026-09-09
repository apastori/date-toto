/*
 * Chain-of-thought (Step 1 — file scope):
 *
 * 1. Single responsibility: write one finished line to stdout with
 *    partial-write and EINTR retry; map broken pipe to exit 0.
 * 2. write: EINTR → retry; EPIPE → fflush+exit(0); other → emit+exit(1).
 * 3. No heap in try_write_all / the write path.
 * 4. One line only — no hot loop throughput concerns beyond one write.
 * 5. C11 + POSIX.1-2008 (write, unistd).
 */

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#endif

#include "date_toto.h"

#include "date_toto_emit.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(_WIN32)
/*
 * maybe_normalize_broken_pipe_errno — map MinGW EINVAL to EPIPE on pipes.
 *
 * Preconditions: fd is a CRT file descriptor; errno reflects failed write().
 * Postconditions: on pipe fds, errno may become EPIPE; otherwise unchanged.
 */
static void maybe_normalize_broken_pipe_errno(int fd, const void *buf)
{
    if (errno != EINVAL || buf == NULL) {
        return;
    }
    {
        HANDLE h = (HANDLE)_get_osfhandle(fd);
        if (h != INVALID_HANDLE_VALUE && GetFileType(h) == FILE_TYPE_PIPE) {
            errno = EPIPE;
        }
    }
}
#else
static void maybe_normalize_broken_pipe_errno(int fd, const void *buf)
{
    (void)fd;
    (void)buf;
}
#endif

/*
 * try_write_all — drain buf[0..count) to STDOUT_FILENO.
 *
 * Preconditions: buf valid for count bytes if count > 0.
 * Returns: 1 on full success; never returns on EPIPE (exits 0); -1 on other
 *          write errors (errno set).
 */
static int try_write_all(const char *buf, size_t count)
{
    size_t sent = 0;

    while (sent < count) {
        ssize_t n = write(STDOUT_FILENO, buf + sent, count - sent);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            maybe_normalize_broken_pipe_errno(STDOUT_FILENO, buf + sent);
            if (errno == EPIPE) {
                (void)fflush(stdout);
                exit(DATE_TOTO_EXIT_OK);
            }
            return -1;
        }
        sent += (size_t)n;
    }
    return 1;
}

/*
 * date_toto_run — see include/date_toto.h for the full contract.
 */
void date_toto_run(const char *line, size_t line_len)
{
    if (try_write_all(line, line_len) < 0) {
        date_toto_emit_error("write");
        exit(DATE_TOTO_EXIT_ERR);
    }
}
