// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt
// CHECK-NEXT:    +--VarDecl
// CHECK-NEXT:    |  +--Name `x `
// CHECK-NEXT:    |  `--NumberLiteral `5`
// CHECK-NEXT:    +--TempStmt
// CHECK-NEXT:    |  +--Name `y `
// CHECK-NEXT:    |  `--NumberLiteral `4`
// CHECK-NEXT:    `--ContentStmt
// CHECK-NEXT:       `--ContentExpr
// CHECK-NEXT:          +--BraceExpr
// CHECK-NEXT:          |  `--Name `x`
// CHECK-NEXT:          `--BraceExpr
// CHECK-NEXT:             `--Name `y`

VAR x = 5
~ temp y = 4
{x}{y}
