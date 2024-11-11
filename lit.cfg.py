import os
import lit.formats

config.name = "Ink Proof"
config.suffixes = [".ink"]
config.test_format = lit.formats.ShTest(True)
config.tools = ["FileCheck"]

config.test_exec_root = os.path.join(os.path.dirname(__file__), "dist")
config.test_times_file = os.path.join(config.test_exec_root, ".lit_test_times.txt")

config.substitutions.append(
    ("%ink-compiler", os.path.join(config.test_exec_root, "inkc")),
)
