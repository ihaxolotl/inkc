.PHONY: all driver library test fuzzer coverage clean

Q = @
PROFILE := debug

DIST_ROOT         := build
DIST              := $(DIST_ROOT)/$(PROFILE)
BIN               := $(DIST)/inkc
LIB               := $(DIST)/libink.a
TEST_BIN          := $(DIST_ROOT)/libink-testing
FUZZ_BIN          := $(DIST_ROOT)/libink-fuzzer
FUZZ_COVERAGE_BIN := $(DIST_ROOT)/libink-coverage

CC    := clang
AR    := ar
RM    := rm -rf
MKDIR := mkdir -p
MAKE  := make

CFLAGS.release := -O2 -DNDEBUG -march=native

CFLAGS.debug   := -O0 -g3                 \
                  -fsanitize=address      \
                  -fsanitize=undefined    \
                  -fno-omit-frame-pointer

CFLAGS.coverage := -O2 -g -march=native      \
                   -fprofile-instr-generate  \
                   -fcoverage-mapping        \
                   -fsanitize=address        \
                   -fsanitize=undefined      \
                   -fsanitize=fuzzer-no-link

CFLAGS  := -Wall                            \
           -Wextra                          \
           -Werror                          \
           -Wpedantic                       \
           -Wno-unused-parameter            \
           -Wconversion                     \
           -std=c99                         \
           -Isrc                            \
           -Iinclude                        \
           ${CFLAGS.${PROFILE}}

LDFLAGS.debug := -fsanitize=address      \
                 -fsanitize=undefined    \
	             -fno-omit-frame-pointer

LDFLAGS.release := -flto

LDFLAGS.coverage := -fprofile-instr-generate  \
                    -fcoverage-mapping        \
                    -fsanitize=address        \
                    -fsanitize=undefined      \
                    -fsanitize=fuzzer-no-link

LDFLAGS := -lm -L/usr/local/lib \
		   ${LDFLAGS.${PROFILE}}

ARFLAGS  := -crs

MAKEFLAGS := --no-print-directory

lib_srcs := src/logging.c \
            src/common.c  \
            src/memory.c  \
            src/arena.c   \
            src/symtab.c  \
            src/stream.c  \
            src/story.c   \
            src/source.c  \
            src/token.c   \
            src/ast.c     \
            src/scanner.c \
            src/parse.c   \
            src/astgen.c  \
            src/compile.c

bin_srcs := src/cli/option.c \
            src/cli/main.c

test_bin_srcs := testing/test_main.c

fuzz_bin_srcs := fuzzing/fuzz_harness.c

fuzz_coverage_bin_srcs := fuzzing/fuzz_main_standalone.c \
                          fuzzing/fuzz_harness.c

build-prefix = $(addprefix $(DIST)/,$1)
source-to-object = $(call build-prefix, $(subst .c,.o,$(filter %.c,$1)))
msg = @printf '  %-8s %s%s\n' "$(1)" "$(2)" "$(if $(3), $(3))";

all: driver library

$(DIST):
	$(call msg,MKDIR,$@)
	$(Q)$(MKDIR) $@

$(DIST)/%.o: %.c | $(DIST)
	$(call msg,CC,$@)
	$(Q)$(MKDIR) $(dir $@)
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

$(LIB): $(call source-to-object,$(lib_srcs))
	$(call msg,AR,$@)
	$(Q)$(AR) $(ARFLAGS) $@ $^

$(BIN): $(call source-to-object,$(bin_srcs)) $(LIB)
	$(call msg,LD,$@)
	$(Q)$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TEST_BIN): $(call source-to-object,$(test_bin_srcs)) $(LIB)
	$(call msg,LD,$@)
	$(Q)$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lcmocka

$(FUZZ_BIN): $(call source-to-object,$(fuzz_bin_srcs)) $(LIB)
	$(call msg,LD,$@)
	$(Q)$(CC) -Isrc -O2 -g -o $@ $^ \
        -fprofile-instr-generate \
        -fcoverage-mapping \
        -fsanitize=address \
        -fsanitize=undefined \
        -fsanitize=fuzzer

$(FUZZ_COVERAGE_BIN): $(call source-to-object,$(fuzz_coverage_bin_srcs)) $(LIB)
	$(call msg,LD,$@)
	$(Q)$(CC) -Isrc -O2 -g -o $@ $^ $(LDFLAGS)

driver: $(BIN)

library: $(LIB)

fuzzer:
	$(Q)$(MAKE) $(MAKEFLAGS) $(FUZZ_BIN) PROFILE=coverage

coverage:
	$(Q)$(MAKE) $(MAKEFLAGS) library PROFILE=coverage
	$(Q)$(MAKE) $(MAKEFLAGS) driver PROFILE=coverage
	$(Q)$(MAKE) $(MAKEFLAGS) $(TEST_BIN) PROFILE=coverage
	$(Q)$(MAKE) $(MAKEFLAGS) $(FUZZ_COVERAGE_BIN) PROFILE=coverage

clean:
	$(Q)$(RM) $(DIST_ROOT)
