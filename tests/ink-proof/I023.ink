// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: +-- BlockStmt
// CHECK: |   `-- DivertStmt
// CHECK: |       `-- DivertExpr
// CHECK: |           `-- Name `firstKnot`
// CHECK: +-- KnotDecl
// CHECK: |   +-- KnotPrototype
// CHECK: |   |   `-- Name `firstKnot`
// CHECK: |   `-- BlockStmt
// CHECK: |       +-- ContentStmt
// CHECK: |       |   `-- StringLiteral `Hello!`
// CHECK: |       `-- DivertStmt
// CHECK: |           `-- DivertExpr
// CHECK: |               `-- Name `anotherKnot`
// CHECK: `-- KnotDecl
// CHECK:     +-- KnotPrototype
// CHECK:     |   `-- Name `anotherKnot`
// CHECK:     `-- BlockStmt
// CHECK:         +-- ContentStmt
// CHECK:         |   `-- StringLiteral `World.`
// CHECK:         `-- DivertStmt
// CHECK:             `-- DivertExpr
// CHECK:                 `-- Name `END`

-> firstKnot
=== firstKnot
    Hello!
    -> anotherKnot
=== anotherKnot
    World.
    -> END
