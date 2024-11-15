// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File
// CHECK-NEXT: --BlockStmt
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------DivideExpr
// CHECK-NEXT: ------------NumberExpr `7 `
// CHECK-NEXT: ------------NumberExpr `3.0 `

{ 7 / 3.0 }
