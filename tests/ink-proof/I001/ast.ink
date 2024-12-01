// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--CompoundStmt <line:9, line:9>
// CHECK-NEXT:    `--ContentStmt <col:1, col:14>
// CHECK-NEXT:       `--ContentExpr <col:1>
// CHECK-NEXT:          `--StringLiteral `Hello, world!` <col:1, col:14>

Hello, world!
