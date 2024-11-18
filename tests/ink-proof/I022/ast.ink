// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt
// CHECK-NEXT:    `--ContentStmt
// CHECK-NEXT:       `--ContentExpr
// CHECK-NEXT:          +--StringLiteral `My name is "`
// CHECK-NEXT:          +--BraceExpr
// CHECK-NEXT:          |  `--StringExpr `"J{"o"}e"`
// CHECK-NEXT:          |     +--StringLiteral `J`
// CHECK-NEXT:          |     +--BraceExpr
// CHECK-NEXT:          |     |  `--StringExpr `"o"`
// CHECK-NEXT:          |     |     `--StringLiteral `o`
// CHECK-NEXT:          |     `--StringLiteral `e`
// CHECK-NEXT:          `--StringLiteral `"`

My name is "{"J{"o"}e"}"
