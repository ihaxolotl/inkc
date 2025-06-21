.PHONY: all default help tests clean

VERBOSE ?= 0

ifeq ($(VERBOSE),1)
	export Q :=
	export VERBOSE := 1
else
	export Q := @
	export VERBOSE := 0
endif

BUILDDIR ?= build
OPTIONS ?=
SANITIZER ?=
BUILD_TYPE ?=

ifneq ($(SANITIZER),)
	OPTIONS += -DUSE_SANITIZER=$(SANITIZER)
endif

ifneq ($(BUILD_TYPE),)
	OPTIONS += -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
endif

all: default

default:
	$(Q) cmake -B $(BUILDDIR) $(OPTIONS)
	$(Q) make -s -C $(BUILDDIR)

tests:
	$(Q) cmake -B $(BUILDDIR)/coverage -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTING=ON -DENABLE_COVERAGE=ON
	$(Q) make -s -C $(BUILDDIR)/coverage coverage

clean:
	$(Q) rm -rf $(BUILDDIR)

help :
	@echo "usage: make [OPTIONS] <target>"
	@echo "  Options:"
	@echo "    > VERBOSE Show verbose output for Make rules. Default 0. Enable with 1."
	@echo "    > BUILDDIR Directory for build results. Default 'build'."
	@echo "    > SANITIZER Compile with support for a Clang/GCC Sanitizer."
	@echo "        Options are: none (default), address, thread, undefined, memory,"
	@echo "        leak, and 'address,undefined' as a combined option"
	@echo "Targets:"
	@echo "  default: Builds all default targets"
	@echo "  tests: Build and run unit test programs"
	@echo "  clean: cleans build artifacts"
