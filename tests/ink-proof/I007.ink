// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File
// CHECK-NEXT: --BlockStmt
// CHECK-NEXT: ----VarDecl
// CHECK-NEXT: ------IdentifierExpr `x `
// CHECK-NEXT: ------StringExpr `"world"`
// CHECK-NEXT: --------StringLiteral `world`
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------StringLiteral `Hello `
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------IdentifierExpr `x`
// CHECK-NEXT: --------StringLiteral `.`

VAR x = "world"
Hello {x}.
