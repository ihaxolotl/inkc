// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File
// CHECK-NEXT: --BlockStmt
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------DivertExpr
// CHECK-NEXT: ----------IdentifierExpr `test`
// CHECK-NEXT: --KnotDecl
// CHECK-NEXT: ----KnotPrototype
// CHECK-NEXT: ------IdentifierExpr `test`
// CHECK-NEXT: ----BlockStmt
// CHECK-NEXT: ------ContentStmt
// CHECK-NEXT: --------ContentExpr
// CHECK-NEXT: ----------StringLiteral `hello`
// CHECK-NEXT: ------ContentStmt
// CHECK-NEXT: --------ContentExpr
// CHECK-NEXT: ----------DivertExpr
// CHECK-NEXT: ------------IdentifierExpr `END`
// CHECK-NEXT: ------ContentStmt
// CHECK-NEXT: --------ContentExpr
// CHECK-NEXT: ----------StringLiteral `world`
// CHECK-NEXT: ------ContentStmt
// CHECK-NEXT: --------ContentExpr
// CHECK-NEXT: ----------DivertExpr
// CHECK-NEXT: ------------IdentifierExpr `END`

-> test
== test ==
hello
-> END
world
-> END
