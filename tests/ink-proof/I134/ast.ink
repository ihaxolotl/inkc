// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--LogicalEqualityExpr
// CHECK-NEXT:    |           +--NumberLiteral `1 `
// CHECK-NEXT:    |           `--NumberLiteral `1 `
// CHECK-NEXT:    `--ContentStmt
// CHECK-NEXT:       `--ContentExpr
// CHECK-NEXT:          `--BraceExpr
// CHECK-NEXT:             `--LogicalInequalityExpr
// CHECK-NEXT:                +--NumberLiteral `1 `
// CHECK-NEXT:                `--NumberLiteral `1 `

{ 1 == 1 }
{ 1 != 1 }
