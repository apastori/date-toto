# date-toto — POSIX-oriented Makefile (gcc or clang on Unix, MSYS2, etc.)
#
# Compiler flag rationale (release CFLAGS):
#   -std=c11          ISO C11 baseline requested by the project spec.
#   -O2               Strong optimisation without -O3’s aggressive trade-offs.
#   -Wall -Wextra     Enable most warnings.
#   -Wpedantic        Reject common extensions and dubious constructs.
#   -Werror           Treat warnings as build-breaking (zero-warning policy).
#   -D_POSIX_C_SOURCE=200809L  Expose POSIX.1-2008 (sigaction, write, etc.).
#
# Artefacts live under build/ (objects + binaries). Tests under build/tests/.
# Debug: Linux uses ASan/UBSan; Windows/MinGW uses -g -O1 only (no libasan).

CC := $(shell command -v gcc >/dev/null 2>&1 && echo gcc || echo clang)

CFLAGS_COMMON := -std=c11 -Wall -Wextra -Wpedantic -Werror \
	-D_POSIX_C_SOURCE=200809L -Iinclude

CFLAGS := $(CFLAGS_COMMON) -O2

# Sanitizers need matching compile+link flags. On MinGW/UCRT they require
# extra packages and are often unavailable; keep -g -O1 there.
ifeq ($(OS),Windows_NT)
CFLAGS_DEBUG := $(CFLAGS_COMMON) -g -O1 -fno-omit-frame-pointer
else
CFLAGS_DEBUG := $(CFLAGS_COMMON) -g -O1 -fsanitize=address,undefined \
	-fno-omit-frame-pointer
endif

BUILD_DIR := build
TEST_BUILD_DIR := $(BUILD_DIR)/tests

MAIN_SRCS := src/main.c \
	src/date_toto_emit.c \
	src/date_toto_write.c \
	src/date_toto_fmt.c \
	src/date_toto_parse.c \
	src/date_toto_cli.c

HDRS := $(wildcard include/*.h)

TEST_SRCS := tests/test_runner.c \
	tests/test_civil_epoch_zero.c \
	tests/test_civil_roundtrip.c \
	tests/test_civil_leap_year.c \
	tests/test_civil_offset.c \
	tests/test_civil_ub_note.c

TEST_HDRS := $(wildcard tests/*.h)

MAIN_OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(MAIN_SRCS))
TEST_OBJS := $(patsubst tests/%.c,$(TEST_BUILD_DIR)/%.o,$(TEST_SRCS))

DATE_TOTO := $(BUILD_DIR)/date-toto
DATE_TOTO_DEBUG := $(BUILD_DIR)/date-toto-debug
TEST_CORE := $(TEST_BUILD_DIR)/test_core

.PHONY: all debug test clean install

all: $(DATE_TOTO)

$(DATE_TOTO): $(MAIN_OBJS) $(HDRS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(MAIN_OBJS)

debug: $(DATE_TOTO_DEBUG)

# Debug/sanitizer binary: compile+link from sources with CFLAGS_DEBUG
# (must not reuse release .o files built with -O2).
$(DATE_TOTO_DEBUG): $(MAIN_SRCS) $(HDRS) | $(BUILD_DIR)
	$(CC) $(CFLAGS_DEBUG) -o $@ $(MAIN_SRCS)

$(TEST_CORE): $(TEST_OBJS) $(HDRS) $(TEST_HDRS) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS)

test: $(TEST_CORE)
	./$(TEST_CORE)

$(BUILD_DIR)/%.o: src/%.c $(HDRS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TEST_BUILD_DIR)/%.o: tests/%.c $(HDRS) $(TEST_HDRS) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR) $(TEST_BUILD_DIR):
	mkdir -p $@

clean:
	rm -f $(MAIN_OBJS) $(TEST_OBJS) $(DATE_TOTO) $(DATE_TOTO_DEBUG) $(TEST_CORE)
	rm -f $(DATE_TOTO).exe $(DATE_TOTO_DEBUG).exe $(TEST_CORE).exe

install: $(DATE_TOTO)
	install -m 755 $(DATE_TOTO) /usr/local/bin
