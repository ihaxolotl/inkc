.PHONY: all default help debug release docs tests clean

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
DOXYGEN_CONFIG ?= docs/Doxyfile

ifneq ($(SANITIZER),)
	OPTIONS += -DUSE_SANITIZER=$(SANITIZER)
endif

ifneq ($(BUILD_TYPE),)
	OPTIONS += -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
endif

all: default
default: release

debug:
	$(Q) cmake -B $(BUILDDIR)/debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED=OFF
	$(Q) make -s -C $(BUILDDIR)/debug

release:
	$(Q) cmake -B $(BUILDDIR)/release -DCMAKE_BUILD_TYPE=RelWithDebInfo
	$(Q) make -s -C $(BUILDDIR)/release

tests:
	$(Q) cmake -B $(BUILDDIR)/coverage -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTING=ON -DENABLE_COVERAGE=ON
	$(Q) make -s -C $(BUILDDIR)/coverage coverage

docs:
	$(Q) mkdir -p $(BUILDDIR)
	$(Q) doxygen $(DOXYGEN_CONFIG)

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
