#!/usr/bin/env python3

import os
import lit.formats

config.name = "Ink Proof"
config.suffixes = [".ink"]
config.test_format = lit.formats.ShTest(True)
config.tools = ["FileCheck"]

config.test_source_root = os.path.dirname(__file__)
config.test_exec_root = os.path.join(config.test_source_root, "build", "bin")
config.test_times_file = os.path.join(config.test_exec_root, ".lit_test_times.txt")

config.substitutions.append(
    ("%ink-compiler", os.path.join(config.test_exec_root, "inkc")),
)
