// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: +-- BlockStmt
// CHECK: |   `-- ContentStmt
// CHECK: |       +-- StringLiteral `hello `
// CHECK: |       `-- DivertExpr
// CHECK: |           `-- Name `world`
// CHECK: `-- KnotDecl
// CHECK:     +-- KnotPrototype
// CHECK:     |   `-- Name `world`
// CHECK:     `-- BlockStmt
// CHECK:         +-- ContentStmt
// CHECK:         |   `-- StringLiteral `world`
// CHECK:         `-- DivertStmt
// CHECK:             `-- DivertExpr
// CHECK:                 `-- Name `END`

hello -> world
== world
world
-> END
