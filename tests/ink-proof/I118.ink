// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     +-- VarDecl
// CHECK:     |   +-- Name `negativeLiteral `
// CHECK:     |   `-- NegateExpr
// CHECK:     |       `-- NumberLiteral `1`
// CHECK:     +-- VarDecl
// CHECK:     |   +-- Name `negativeLiteral2 `
// CHECK:     |   `-- NotExpr
// CHECK:     |       `-- NotExpr
// CHECK:     |           `-- False
// CHECK:     +-- VarDecl
// CHECK:     |   +-- Name `negativeLiteral3 `
// CHECK:     |   `-- NotExpr
// CHECK:     |       `-- NumberLiteral `0`
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- AddExpr
// CHECK:     |           +-- Name `negativeLiteral `
// CHECK:     |           `-- NumberLiteral `0`
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- AddExpr
// CHECK:     |           +-- Name `negativeLiteral2 `
// CHECK:     |           `-- NumberLiteral `0`
// CHECK:     `-- ContentStmt
// CHECK:         `-- BraceExpr
// CHECK:             `-- AddExpr
// CHECK:                 +-- Name `negativeLiteral3 `
// CHECK:                 `-- NumberLiteral `0`

VAR negativeLiteral = -1
VAR negativeLiteral2 = not not false
VAR negativeLiteral3 = !(0)
{negativeLiteral + 0}
{negativeLiteral2 + 0}
{negativeLiteral3 + 0}
