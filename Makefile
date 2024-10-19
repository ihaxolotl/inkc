.PHONY: all clean

BUILD_ROOT := dist
BUILD_TARGET := $(BUILD_ROOT)/inkc

Q       := @
CC      := clang
RM      := rm -rf
MKDIR   := mkdir -p
CFLAGS  := -Wall -Wextra -Wpedantic -Wno-unused-parameter -Werror -std=c99 \
           -g -O0
LDFLAGS := -fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined

SRCS := src/main.c

all: $(BUILD_ROOT) $(BUILD_TARGET)

clean:
	$(Q)$(RM) $(BUILD_ROOT)

$(BUILD_ROOT):
	$(Q)$(MKDIR) $@

$(BUILD_TARGET): $(SRCS)
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
