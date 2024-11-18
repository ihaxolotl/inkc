// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

CHECK: File "STDIN"
CHECK: +-- BlockStmt
CHECK: |   `-- DivertStmt
CHECK: |       `-- DivertExpr
CHECK: |           `-- Name `test`
CHECK: `-- KnotDecl
CHECK:     +-- KnotPrototype
CHECK:     |   `-- Name `test `
CHECK:     `-- BlockStmt
CHECK:         +-- ContentStmt
CHECK:         |   `-- StringLiteral `hello`
CHECK:         +-- DivertStmt
CHECK:         |   `-- DivertExpr
CHECK:         |       `-- Name `END`
CHECK:         +-- ContentStmt
CHECK:         |   `-- StringLiteral `world`
CHECK:         `-- DivertStmt
CHECK:             `-- DivertExpr
CHECK:                 `-- Name `END`

-> test
== test ==
hello
-> END
world
-> END
