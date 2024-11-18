// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt
// CHECK-NEXT:    +--ConstDecl
// CHECK-NEXT:    |  +--Name `CONST_STR `
// CHECK-NEXT:    |  `--StringExpr `"ConstantString"`
// CHECK-NEXT:    |     `--StringLiteral `ConstantString`
// CHECK-NEXT:    +--VarDecl
// CHECK-NEXT:    |  +--Name `varStr `
// CHECK-NEXT:    |  `--Name `CONST_STR`
// CHECK-NEXT:    `--ContentStmt
// CHECK-NEXT:       `--ContentExpr
// CHECK-NEXT:          `--BraceExpr
// CHECK-NEXT:             `--ConditionalStmt
// CHECK-NEXT:                +--LogicalEqualityExpr
// CHECK-NEXT:                |  +--Name `varStr `
// CHECK-NEXT:                |  `--Name `CONST_STR`
// CHECK-NEXT:                `--ContentExpr
// CHECK-NEXT:                   `--StringLiteral `success`

CONST CONST_STR = "ConstantString"
VAR varStr = CONST_STR
{varStr == CONST_STR:success}
