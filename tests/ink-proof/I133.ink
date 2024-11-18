// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt
// CHECK-NEXT:    `--ContentStmt
// CHECK-NEXT:       `--ContentExpr
// CHECK-NEXT:          `--BraceExpr
// CHECK-NEXT:             `--DivideExpr
// CHECK-NEXT:                +--NumberLiteral `7 `
// CHECK-NEXT:                `--NumberLiteral `3.0 `

{ 7 / 3.0 }
