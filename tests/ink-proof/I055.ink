// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File
// CHECK-NEXT: --BlockStmt
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------DivertExpr
// CHECK-NEXT: ----------IdentifierExpr `hurry_home`
// CHECK-NEXT: --KnotDecl
// CHECK-NEXT: ----KnotPrototype
// CHECK-NEXT: ------IdentifierExpr `hurry_home`
// CHECK-NEXT: ----BlockStmt
// CHECK-NEXT: ------ContentStmt
// CHECK-NEXT: --------ContentExpr
// CHECK-NEXT: ----------StringLiteral `We hurried home to Savile Row `
// CHECK-NEXT: ----------DivertExpr
// CHECK-NEXT: ------------IdentifierExpr `as_fast_as_we_could`
// CHECK-NEXT: --KnotDecl
// CHECK-NEXT: ----KnotPrototype
// CHECK-NEXT: ------IdentifierExpr `as_fast_as_we_could`
// CHECK-NEXT: ----BlockStmt
// CHECK-NEXT: ------ContentStmt
// CHECK-NEXT: --------ContentExpr
// CHECK-NEXT: ----------StringLiteral `as fast as we could.`
// CHECK-NEXT: ------ContentStmt
// CHECK-NEXT: --------ContentExpr
// CHECK-NEXT: ----------DivertExpr
// CHECK-NEXT: ------------IdentifierExpr `DONE`

-> hurry_home
=== hurry_home ===
We hurried home to Savile Row -> as_fast_as_we_could
=== as_fast_as_we_could ===
as fast as we could.
-> DONE
