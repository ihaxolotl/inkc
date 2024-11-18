// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     +-- VarDecl
// CHECK:     |   +-- Name `x `
// CHECK:     |   `-- NumberLiteral `5`
// CHECK:     +-- TempStmt
// CHECK:     |   +-- Name `y `
// CHECK:     |   `-- NumberLiteral `4`
// CHECK:     `-- ContentStmt
// CHECK:         +-- BraceExpr
// CHECK:         |   `-- Name `x`
// CHECK:         `-- BraceExpr
// CHECK:             `-- Name `y`

VAR x = 5
~ temp y = 4
{x}{y}
