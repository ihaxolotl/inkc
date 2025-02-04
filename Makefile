.PHONY: all test clean

Q = @

CC    := clang
AR    := ar
RM    := rm -rf
MKDIR := mkdir -p

DIST     := dist
INK_BIN  := $(DIST)/inkc
INK_LIB  := $(DIST)/libink.a
TEST_BIN := $(DIST)/libink-tests

CFLAGS  := -Wall                       \
           -Wextra                     \
           -Werror                     \
           -Wpedantic                  \
           -Wno-unused-parameter       \
           -Wconversion                \
           -std=c99 -g3 -ggdb -O0      \
           -Isrc

LDFLAGS := -fno-omit-frame-pointer     \
           -fsanitize=address          \
           -fsanitize=undefined        \
           -lm -L/usr/local/lib

ARFLAGS  := -crs

lib_srcs := src/logging.c                  \
            src/common.c                   \
            src/memory.c                   \
            src/arena.c                    \
            src/object.c                   \
            src/story.c                    \
            src/source.c                   \
            src/token.c                    \
            src/tree.c                     \
            src/ir.c                       \
            src/scanner.c                  \
            src/parse.c                    \
            src/astgen.c

bin_srcs := src/option.c                   \
            src/main.c

test_srcs := tests/main.c

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

$(TEST_BIN): $(call source-to-object,$(test_srcs)) $(INK_LIB)
	$(call msg,LD,$@)
	$(Q)$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lcmocka

test: $(TEST_BIN)
	$(Q)$(TEST_BIN)

clean:
	$(Q)$(RM) $(DIST)
