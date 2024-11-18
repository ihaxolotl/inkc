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
// CHECK:         +-- ChoiceStarBranch
// CHECK:         |   +-- ChoiceContentExpr
// CHECK:         |   |   +-- StringLiteral `B`
// CHECK:         |   |   +-- NullNode
// CHECK:         |   |   `-- NullNode
// CHECK:         |   `-- BlockStmt
// CHECK:         `-- ChoiceStarBranch
// CHECK:             `-- ChoiceContentExpr
// CHECK:                 +-- StringLiteral `C`
// CHECK:                 +-- NullNode
// CHECK:                 `-- NullNode

* A
* B
* C
