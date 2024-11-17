// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File
// CHECK-NEXT: --BlockStmt
// CHECK-NEXT: ----KnotDecl
// CHECK-NEXT: ------KnotPrototype
// CHECK-NEXT: --------IdentifierExpr `test`
// CHECK-NEXT: ------BlockStmt
// CHECK-NEXT: --------ContentStmt
// CHECK-NEXT: ----------ContentExpr
// CHECK-NEXT: ------------StringLiteral `Content`
// CHECK-NEXT: --------ContentStmt
// CHECK-NEXT: ----------ContentExpr
// CHECK-NEXT: ------------DivertExpr
// CHECK-NEXT: --------------IdentifierExpr `END`

== test ==
Content
-> END
