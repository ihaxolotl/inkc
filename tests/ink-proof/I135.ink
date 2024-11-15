// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File
// CHECK-NEXT: --BlockStmt
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------AddExpr
// CHECK-NEXT: ------------LogicalEqualityExpr
// CHECK-NEXT: --------------NumberExpr `1 `
// CHECK-NEXT: --------------NumberExpr `1`
// CHECK-NEXT: ------------NumberExpr `1 `
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------SubtractExpr
// CHECK-NEXT: ------------LogicalInequalityExpr
// CHECK-NEXT: --------------NumberExpr `1 `
// CHECK-NEXT: --------------NumberExpr `1`
// CHECK-NEXT: ------------NumberExpr `1 `

{ (1 == 1) + 1 }
{ (1 != 1) - 1 }
