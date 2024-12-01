.PHONY: all clean

BUILD_ROOT := dist
BUILD_TARGET := $(BUILD_ROOT)/inkc

Q       := @
CC      := clang
RM      := rm -rf
MKDIR   := mkdir -p

CFLAGS  := -Wall                       \
           -Wextra                     \
           -Werror                     \
           -Wpedantic                  \
           -Wno-unused-parameter       \
           -Wconversion                \
           -std=c99 -g3 -ggdb -O0

LDFLAGS := -fno-omit-frame-pointer     \
           -fsanitize=address          \
           -fsanitize=undefined        \

SRCS := src/main.c                     \
        src/logging.c                  \
        src/unix.c                     \
        src/platform.c                 \
        src/arena.c                    \
        src/source.c                   \
        src/token.c                    \
        src/tree.c                     \
        src/scanner.c                  \
        src/parse.c

all: $(BUILD_ROOT) $(BUILD_TARGET)

clean:
	$(Q)$(RM) $(BUILD_ROOT)

$(BUILD_ROOT):
	$(Q)$(MKDIR) $@

$(BUILD_TARGET): $(SRCS)
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
