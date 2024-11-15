// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File
// CHECK-NEXT: --BlockStmt
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------AddExpr
// CHECK-NEXT: ------------MultiplyExpr
// CHECK-NEXT: --------------NumberExpr `2 `
// CHECK-NEXT: --------------NumberExpr `3 `
// CHECK-NEXT: ------------MultiplyExpr
// CHECK-NEXT: --------------NumberExpr `5 `
// CHECK-NEXT: --------------NumberExpr `6 `
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------ModExpr
// CHECK-NEXT: ------------NumberExpr `8 `
// CHECK-NEXT: ------------NumberExpr `3`
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------ModExpr
// CHECK-NEXT: ------------NumberExpr `13 `
// CHECK-NEXT: ------------NumberExpr `5`
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------DivideExpr
// CHECK-NEXT: ------------NumberExpr `7 `
// CHECK-NEXT: ------------NumberExpr `3 `
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------DivideExpr
// CHECK-NEXT: ------------NumberExpr `5 `
// CHECK-NEXT: ------------NumberExpr `2.0 `
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------SubtractExpr
// CHECK-NEXT: ------------NumberExpr `10 `
// CHECK-NEXT: ------------NumberExpr `2 `
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------MultiplyExpr
// CHECK-NEXT: ------------NumberExpr `2 `
// CHECK-NEXT: ------------SubtractExpr
// CHECK-NEXT: --------------NumberExpr `5`
// CHECK-NEXT: --------------NumberExpr `1`

{ 2 * 3 + 5 * 6 }
{8 mod 3}
{13 % 5}
{ 7 / 3 }
{ 5 / 2.0 }
{ 10 - 2 }
{ 2 * (5-1) }
