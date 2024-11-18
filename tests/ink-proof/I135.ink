// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- AddExpr
// CHECK:     |           +-- LogicalEqualityExpr
// CHECK:     |           |   +-- NumberLiteral `1 `
// CHECK:     |           |   `-- NumberLiteral `1`
// CHECK:     |           `-- NumberLiteral `1 `
// CHECK:     `-- ContentStmt
// CHECK:         `-- BraceExpr
// CHECK:             `-- SubtractExpr
// CHECK:                 +-- LogicalInequalityExpr
// CHECK:                 |   +-- NumberLiteral `1 `
// CHECK:                 |   `-- NumberLiteral `1`
// CHECK:                 `-- NumberLiteral `1 `

{ (1 == 1) + 1 }
{ (1 != 1) - 1 }
