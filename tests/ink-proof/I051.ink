// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File
// CHECK-NEXT: --BlockStmt
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------IdentifierExpr `x`
// CHECK-NEXT: ----VarDecl
// CHECK-NEXT: ------IdentifierExpr `x `
// CHECK-NEXT: ------IdentifierExpr `kX`
// CHECK-NEXT: ----ConstDecl
// CHECK-NEXT: ------IdentifierExpr `kX `
// CHECK-NEXT: ------StringExpr `"hi"`
// CHECK-NEXT: --------StringLiteral `hi`

{x}
VAR x = kX
CONST kX = "hi"
