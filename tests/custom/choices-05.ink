// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     `-- ChoiceStmt
// CHECK:         `-- ChoiceStarBranch
// CHECK:             `-- ChoiceContentExpr
// CHECK:                 +-- StringLiteral `A`
// CHECK:                 +-- StringLiteral ``
// CHECK:                 `-- StringLiteral `B`

* A[]B
