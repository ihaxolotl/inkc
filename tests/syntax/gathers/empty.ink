// RUN: %ink-compiler --stdin --compile-only --dump-ast < %s | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:7, line:7>
// CHECk-NEXT:    `--GatherPoint <line:7, col:1:3>

-
