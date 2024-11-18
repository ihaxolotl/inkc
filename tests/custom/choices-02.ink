// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     `-- ChoiceStmt
// CHECK:         +-- ChoiceStarBranch
// CHECK:         |   +-- ChoiceContentExpr
// CHECK:         |   |   +-- StringLiteral `A`
// CHECK:         |   |   +-- NullNode
// CHECK:         |   |   `-- NullNode
// CHECK:         |   `-- BlockStmt
// CHECK:         |       `-- ChoiceStmt
// CHECK:         |           +-- ChoiceStarBranch
// CHECK:         |           |   +-- ChoiceContentExpr
// CHECK:         |           |   |   +-- StringLiteral `Nested inside A`
// CHECK:         |           |   |   +-- NullNode
// CHECK:         |           |   |   `-- NullNode
// CHECK:         |           |   `-- BlockStmt
// CHECK:         |           `-- ChoiceStarBranch
// CHECK:         |               +-- ChoiceContentExpr
// CHECK:         |               |   +-- StringLiteral `Also nested inside A`
// CHECK:         |               |   +-- NullNode
// CHECK:         |               |   `-- NullNode
// CHECK:         |               `-- BlockStmt
// CHECK:         `-- ChoiceStarBranch
// CHECK:             +-- ChoiceContentExpr
// CHECK:             |   +-- StringLiteral `B`
// CHECK:             |   +-- NullNode
// CHECK:             |   `-- NullNode
// CHECK:             `-- BlockStmt
// CHECK:                 +-- ChoiceStmt
// CHECK:                 |   `-- ChoiceStarBranch
// CHECK:                 |       +-- ChoiceContentExpr
// CHECK:                 |       |   +-- StringLiteral `Nested inside B`
// CHECK:                 |       |   +-- NullNode
// CHECK:                 |       |   `-- NullNode
// CHECK:                 |       `-- BlockStmt
// CHECK:                 `-- ChoiceStmt
// CHECK:                     `-- ChoiceStarBranch
// CHECK:                         `-- ChoiceContentExpr
// CHECK:                             +-- StringLiteral `Also nested inside B`
// CHECK:                             +-- NullNode
// CHECK:                             `-- NullNode

* A
** Nested inside A
** Also nested inside A
* B
*** Nested inside B
** Also nested inside B
