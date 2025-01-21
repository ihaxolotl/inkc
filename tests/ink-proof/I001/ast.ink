// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:9, line:9>
// CHECK-NEXT:   `--ContentStmt <line:9, col:1:14>
// CHECK-NEXT:      `--Content <col:1, col:14>
// CHECK-NEXT:          `--StringLiteral `Hello, world!` <col:1, col:14>

Hello, world!
