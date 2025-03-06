.PHONY: all test fuzz clean

Q = @

PROFILE   := debug
DIST_ROOT := build
DIST      := $(DIST_ROOT)/$(PROFILE)
INK_BIN   := $(DIST)/inkc
INK_LIB   := $(DIST)/libink.a
TEST_BIN  := $(DIST)/libink-tests
FUZZ_BIN  := $(DIST)/libink-fuzzer

CC    := clang
AR    := ar
RM    := rm -rf
MKDIR := mkdir -p

CFLAGS.release := -O2 -DNDEBUG
CFLAGS.debug   := -O0 -g3                      \
                  -fsanitize=address,undefined \
                  -fno-omit-frame-pointer      \
                  -fprofile-instr-generate     \
                  -fcoverage-mapping

CFLAGS  := -Wall                               \
           -Wextra                             \
           -Werror                             \
           -Wpedantic                          \
           -Wno-unused-parameter               \
           -Wconversion                        \
           -std=c99                            \
           -Isrc                               \
           ${CFLAGS.${PROFILE}}

LDFLAGS.debug := -fsanitize=address,undefined  \
	             -fno-omit-frame-pointer       \

LDFLAGS.release :=

LDFLAGS := -lm -L/usr/local/lib                \
		   ${LDFLAGS.${PROFILE}}

ARFLAGS  := -crs

lib_srcs := src/logging.c                  \
            src/common.c                   \
            src/memory.c                   \
            src/arena.c                    \
            src/symtab.c                   \
            src/story.c                    \
            src/source.c                   \
            src/token.c                    \
            src/ast.c                      \
            src/scanner.c                  \
            src/parse.c                    \
            src/astgen.c                   \
            src/compile.c

bin_srcs := src/option.c                   \
            src/main.c

test_srcs := tests/test_main.c

fuzz_srcs := tests/fuzz_main.cc

build-prefix = $(addprefix $(DIST)/,$1)
source-to-object = $(call build-prefix, $(subst .c,.o,$(filter %.c,$1)))
msg = @printf '  %-8s %s%s\n' "$(1)" "$(2)" "$(if $(3), $(3))";

all: $(INK_LIB) $(INK_BIN)

$(DIST):
	$(call msg,MKDIR,$@)
	$(Q)$(MKDIR) $@

$(DIST)/%.o: %.c | $(DIST)
	$(call msg,CC,$@)
	$(Q)$(MKDIR) $(dir $@)
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

$(INK_LIB): $(call source-to-object,$(lib_srcs))
	$(call msg,AR,$@)
	$(Q)$(AR) $(ARFLAGS) $@ $^

$(INK_BIN): $(call source-to-object,$(bin_srcs)) $(INK_LIB)
	$(call msg,LD,$@)
	$(Q)$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: $(TEST_BIN)

$(TEST_BIN): $(call source-to-object,$(test_srcs)) $(INK_LIB)
	$(call msg,LD,$@)
	$(Q)$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lcmocka

fuzz: $(FUZZ_BIN)

$(FUZZ_BIN): $(INK_LIB)
	$(call msg,LD,$@)
	$(Q)$(CC) -Isrc -g3 -O0                                \
        -fno-omit-frame-pointer                            \
		-fsanitize=address,undefined,fuzzer                \
		-fprofile-instr-generate                           \
		-fcoverage-mapping                                 \
	   	-o $(FUZZ_BIN) $(fuzz_srcs) $(INK_LIB) $(LDFLAGS)

clean:
	$(Q)$(RM) $(DIST_ROOT)
