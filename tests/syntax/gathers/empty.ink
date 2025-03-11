// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:7, line:7>
// CHECk-NEXT:    `--GatherPoint <line:7, col:1:3>

-
