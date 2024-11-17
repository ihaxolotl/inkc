// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     +-- VarDecl
// CHECK:     |   +-- Name `x `
// CHECK:     |   `-- Name `c`
// CHECK:     +-- ConstDecl
// CHECK:     |   +-- Name `c `
// CHECK:     |   `-- NumberLiteral `5`
// CHECK:     `-- ContentStmt
// CHECK:         `-- BraceExpr
// CHECK:             `-- Name `x`

VAR x = c
CONST c = 5
{x}
