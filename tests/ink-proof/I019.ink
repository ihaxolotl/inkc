// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- KnotDecl
// CHECK:     +-- KnotPrototype
// CHECK:     |   `-- Name `test `
// CHECK:     `-- BlockStmt
// CHECK:         +-- ContentStmt
// CHECK:         |   `-- StringLiteral `Content`
// CHECK:         `-- DivertStmt
// CHECK:             `-- DivertExpr
// CHECK:                 `-- Name `END`

== test ==
Content
-> END
