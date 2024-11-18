// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt
// CHECK-NEXT:    +--VarDecl
// CHECK-NEXT:    |  +--Name `x `
// CHECK-NEXT:    |  `--Name `c`
// CHECK-NEXT:    +--ConstDecl
// CHECK-NEXT:    |  +--Name `c `
// CHECK-NEXT:    |  `--NumberLiteral `5`
// CHECK-NEXT:    `--ContentStmt
// CHECK-NEXT:       `--ContentExpr
// CHECK-NEXT:          `--BraceExpr
// CHECK-NEXT:             `--Name `x`

VAR x = c
CONST c = 5
{x}
