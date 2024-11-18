// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--Name `x`
// CHECK-NEXT:    +--VarDecl
// CHECK-NEXT:    |  +--Name `x `
// CHECK-NEXT:    |  `--Name `kX`
// CHECK-NEXT:    `--ConstDecl
// CHECK-NEXT:       +--Name `kX `
// CHECK-NEXT:       `--StringExpr `"hi"`
// CHECK-NEXT:          `--StringLiteral `hi`

{x}
VAR x = kX
CONST kX = "hi"
