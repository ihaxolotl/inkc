// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: +-- BlockStmt
// CHECK: |   +-- ThreadStmt
// CHECK: |   |   `-- ThreadExpr
// CHECK: |   |       `-- Name `move`
// CHECK: |   `-- ContentStmt
// CHECK: |       `-- StringLiteral `Limes`
// CHECK: `-- KnotDecl
// CHECK:     +-- KnotPrototype
// CHECK:     |   `-- Name `move`
// CHECK:     `-- BlockStmt
// CHECK:         `-- ChoiceStmt
// CHECK:             `-- ChoiceStarBranch
// CHECK:                 +-- ChoiceContentExpr
// CHECK:                 |   +-- StringLiteral `boop`
// CHECK:                 |   +-- NullNode
// CHECK:                 |   `-- NullNode
// CHECK:                 `-- BlockStmt
// CHECK:                     `-- DivertStmt
// CHECK:                         `-- DivertExpr
// CHECK:                             `-- Name `END`

<- move
Limes
=== move
    * boop
        -> END
