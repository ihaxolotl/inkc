// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File
// CHECK-NEXT: --BlockStmt
// CHECK-NEXT: ----VarDecl
// CHECK-NEXT: ------IdentifierExpr `x `
// CHECK-NEXT: ------IdentifierExpr `c`
// CHECK-NEXT: ----ConstDecl
// CHECK-NEXT: ------IdentifierExpr `c `
// CHECK-NEXT: ------NumberExpr `5`
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------IdentifierExpr `x`

VAR x = c
CONST c = 5
{x}
