// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File
// CHECK-NEXT: --BlockStmt
// CHECK-NEXT: ----VarDecl
// CHECK-NEXT: ------IdentifierExpr `negativeLiteral `
// CHECK-NEXT: ------NegateExpr
// CHECK-NEXT: --------NumberExpr `1`
// CHECK-NEXT: ----VarDecl
// CHECK-NEXT: ------IdentifierExpr `negativeLiteral2 `
// CHECK-NEXT: ------NotExpr
// CHECK-NEXT: --------NotExpr
// CHECK-NEXT: ----------FalseExpr
// CHECK-NEXT: ----VarDecl
// CHECK-NEXT: ------IdentifierExpr `negativeLiteral3 `
// CHECK-NEXT: ------NotExpr
// CHECK-NEXT: --------NumberExpr `0`
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------AddExpr
// CHECK-NEXT: ------------IdentifierExpr `negativeLiteral `
// CHECK-NEXT: ------------NumberExpr `0`
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------AddExpr
// CHECK-NEXT: ------------IdentifierExpr `negativeLiteral2 `
// CHECK-NEXT: ------------NumberExpr `0`
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------AddExpr
// CHECK-NEXT: ------------IdentifierExpr `negativeLiteral3 `
// CHECK-NEXT: ------------NumberExpr `0`

VAR negativeLiteral = -1
VAR negativeLiteral2 = not not false
VAR negativeLiteral3 = !(0)
{negativeLiteral + 0}
{negativeLiteral2 + 0}
{negativeLiteral3 + 0}
