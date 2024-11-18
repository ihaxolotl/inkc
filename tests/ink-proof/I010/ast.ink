// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--Name `x`
// CHECK-NEXT:    +--TempStmt
// CHECK-NEXT:    |  +--Name `x `
// CHECK-NEXT:    |  `--NumberLiteral `5`
// CHECK-NEXT:    `--ContentStmt
// CHECK-NEXT:       `--ContentExpr
// CHECK-NEXT:          `--StringLiteral `hello`

{x}
~temp x = 5
hello
