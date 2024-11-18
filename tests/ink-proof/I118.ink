// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt
// CHECK-NEXT:    +--VarDecl
// CHECK-NEXT:    |  +--Name `negativeLiteral `
// CHECK-NEXT:    |  `--NegateExpr
// CHECK-NEXT:    |     `--NumberLiteral `1`
// CHECK-NEXT:    +--VarDecl
// CHECK-NEXT:    |  +--Name `negativeLiteral2 `
// CHECK-NEXT:    |  `--NotExpr
// CHECK-NEXT:    |     `--NotExpr
// CHECK-NEXT:    |        `--False
// CHECK-NEXT:    +--VarDecl
// CHECK-NEXT:    |  +--Name `negativeLiteral3 `
// CHECK-NEXT:    |  `--NotExpr
// CHECK-NEXT:    |     `--NumberLiteral `0`
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--AddExpr
// CHECK-NEXT:    |           +--Name `negativeLiteral `
// CHECK-NEXT:    |           `--NumberLiteral `0`
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--AddExpr
// CHECK-NEXT:    |           +--Name `negativeLiteral2 `
// CHECK-NEXT:    |           `--NumberLiteral `0`
// CHECK-NEXT:    `--ContentStmt
// CHECK-NEXT:       `--ContentExpr
// CHECK-NEXT:          `--BraceExpr
// CHECK-NEXT:             `--AddExpr
// CHECK-NEXT:                +--Name `negativeLiteral3 `
// CHECK-NEXT:                `--NumberLiteral `0`

VAR negativeLiteral = -1
VAR negativeLiteral2 = not not false
VAR negativeLiteral3 = !(0)
{negativeLiteral + 0}
{negativeLiteral2 + 0}
{negativeLiteral3 + 0}
