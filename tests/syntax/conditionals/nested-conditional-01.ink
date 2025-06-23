// RUN: %ink-compiler < %s --stdin --compile-only --dump-ast | FileCheck %s

VAR x = 10

{
  - x >= 5:
    {
      - x == 10: Ten
      - else: Unreachable
    }
  - else: Unreachable
}
