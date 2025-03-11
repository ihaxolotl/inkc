// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:12, line:13>
// CHECK-NEXT:    `--ContentStmt <line:12, col:1:4>
// CHECK-NEXT:       `--Content <col:1, col:4>
// CHECK-NEXT:          `--MultiIfStmt <col:1, col:1>
// CHECK-NEXT: <STDIN>:13:1: error: condition block with no conditions
// CHECK-NEXT:   13 | }
// CHECK-NEXT:      | ^

{
}
