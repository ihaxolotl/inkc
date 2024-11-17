// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File
// CHECK-NEXT: --BlockStmt
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------CallExpr
// CHECK-NEXT: ------------IdentifierExpr `FLOOR`
// CHECK-NEXT: ------------ArgumentList
// CHECK-NEXT: --------------NumberExpr `1.2`
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------CallExpr
// CHECK-NEXT: ------------IdentifierExpr `INT`
// CHECK-NEXT: ------------ArgumentList
// CHECK-NEXT: --------------NumberExpr `1.2`
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------CallExpr
// CHECK-NEXT: ------------IdentifierExpr `CEILING`
// CHECK-NEXT: ------------ArgumentList
// CHECK-NEXT: --------------NumberExpr `1.2`
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------DivideExpr
// CHECK-NEXT: ------------CallExpr
// CHECK-NEXT: --------------IdentifierExpr `CEILING`
// CHECK-NEXT: --------------ArgumentList
// CHECK-NEXT: ----------------NumberExpr `1.2`
// CHECK-NEXT: ------------NumberExpr `3`
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------DivideExpr
// CHECK-NEXT: ------------CallExpr
// CHECK-NEXT: --------------IdentifierExpr `INT`
// CHECK-NEXT: --------------ArgumentList
// CHECK-NEXT: ----------------CallExpr
// CHECK-NEXT: ------------------IdentifierExpr `CEILING`
// CHECK-NEXT: ------------------ArgumentList
// CHECK-NEXT: --------------------NumberExpr `1.2`
// CHECK-NEXT: ------------NumberExpr `3`
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------CallExpr
// CHECK-NEXT: ------------IdentifierExpr `FLOOR`
// CHECK-NEXT: ------------ArgumentList
// CHECK-NEXT: --------------NumberExpr `1`

{FLOOR(1.2)}
{INT(1.2)}
{CEILING(1.2)}
{CEILING(1.2) / 3}
{INT(CEILING(1.2)) / 3}
{FLOOR(1)}
