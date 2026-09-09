# date-toto

A small C11 utility that behaves like GNU `date` for the supported subset: it
displays, formats, and converts date/time to a single line on stdout. It uses
raw `write(2)` for that line (no stdio formatting on the output path).

Formatting uses a self-implemented `%` directive engine and civil-date/epoch
arithmetic (C locale English names). libc supplies the current instant
(`clock_gettime`), the local UTC offset, and the timezone abbreviation. No
`strftime`.

## Build

```sh
make            # release binary: ./build/date-toto
make debug      # sanitizers + debug symbols → ./build/date-toto-debug
make test       # unit tests for civil helpers → ./build/tests/test_core
make clean      # removes objects/binaries; keeps build/.gitkeep scaffolding
sudo make install   # installs to /usr/local/bin (optional)
```

Object files and binaries are written under `build/` (tests under `build/tests/`).
The directories are tracked via `.gitkeep`; artefacts are gitignored.

Requires `gcc` or `clang`. On Linux, `sigaction` is used for `SIGPIPE`; on
Windows (MSYS2 UCRT64 MinGW) build natively — see below.

### Linux

```sh
make clean && make
```

### Windows (MSYS2 UCRT64)

Build from the **MSYS2 UCRT64** terminal (recommended):

```sh
pacman -S make mingw-w64-ucrt-x86_64-gcc   # if not already installed
cd /path/to/date-toto
make clean && make
```

The resulting `build/date-toto` is a native Windows executable and runs in
UCRT64, Git Bash, cmd, and PowerShell.

Git Bash can build if `/c/msys64/ucrt64/bin` is early on `PATH` (before
Anaconda). Prefer the UCRT64 shell, or:

```sh
PATH="/c/msys64/ucrt64/bin:$PATH" make
```

## Usage

- `./build/date-toto` — current local date/time (default format).
- `./build/date-toto +FORMAT` — custom format (leading `+`).
- `./build/date-toto -u` / `--utc` / `--universal` — UTC.
- `./build/date-toto -d STRING` / `--date=STRING` — date described by STRING.
- `./build/date-toto -r FILE` / `--reference=FILE` — file modification time.
- `./build/date-toto -I` / `--iso-8601[=FMT]` — ISO 8601-style output.
- `./build/date-toto -R` / `--rfc-email` — RFC 5322-style output.
- `./build/date-toto --help` / `--h` — help and exit.
- `./build/date-toto --version` / `--v` — version and exit.

Help is detected anywhere in the argument list and takes precedence over
version. Unknown options are errors. Clock setting is not supported.

Examples:

```sh
./build/date-toto '+%Y-%m-%dT%H:%M:%S'
./build/date-toto -u '+%Y-%m-%d %H:%M:%S'
./build/date-toto -d '7 days ago' +%F
./build/date-toto -Iseconds
./build/date-toto -R
```

### Supported directives

| Directive | Meaning |
|-----------|---------|
| `%Y` `%y` `%C` | Year / two-digit year / century |
| `%m` `%d` `%e` | Month; day zero- or space-padded |
| `%H` `%I` `%M` `%S` | Hour 24/12, minute, second |
| `%j` `%s` | Day of year; Unix timestamp |
| `%u` `%w` | Weekday Mon=1..7 / Sun=0..6 |
| `%a` `%A` | Abbreviated / full weekday |
| `%b` `%h` `%B` | Abbreviated / full month |
| `%p` `%P` | AM/PM / am/pm |
| `%Z` `%z` `%:z` | Zone name; `±HHMM`; `±HH:MM` |
| `%F` `%T` `%D` `%R` | Shortcuts (`%Y-%m-%d`, etc.) |
| `%c` `%x` `%X` | Preferred date+time / date / time |
| `%n` `%t` `%%` | Newline; tab; literal `%` |

### Accepted `-d` strings

| Form | Example |
|------|---------|
| `@EPOCH` | `@1788725922` |
| ISO-ish datetime | `2026-12-25`, `2026-12-25T15:30:00Z` |
| Time only | `15:30`, `15:30:00` |
| Keywords | `now`, `today`, `yesterday`, `tomorrow` |
| Relative | `7 days ago`, `+3 weeks`, `next month` |

On Windows (MinGW), a broken pipe may surface as `EINVAL` instead of `EPIPE`;
the write path normalises that to `EPIPE` so piped use can exit `0` quietly
(no stderr error).

## Exit codes

- `0` — success, `--help`, `--version`, or broken pipe (`EPIPE`; on Linux after
  ignoring `SIGPIPE`).
- `1` — invalid option/date, `-r` failure, write/setup error (stderr:
  `date-toto: <context>: <reason>` or `date-toto: invalid date '<s>'`).

## Layout

- `LICENSE.txt` — GNU General Public License, version 2.
- `c_version.txt` — C11 standard, compiler flags, and toolchain notes.
- `include/date_toto.h` — constants, exit enum, civil helpers, `date_toto_run()`.
- `include/date_toto_*.h` — declarations for emit, fmt, parse, CLI helpers.
- `src/main.c` — entry point only.
- `src/date_toto_{emit,write,fmt,parse,cli}.c` — implementation units.
- `tests/test_runner.c` plus `tests/test_civil_*.c` / `.h` — `assert()`-based
  civil/epoch tests → `build/tests/test_core`.
- `build/` — object/binary output (`build/date-toto`, `build/tests/test_core`);
  dirs kept via `.gitkeep`, artefacts gitignored.
