// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- Name `x`
// CHECK:     +-- VarDecl
// CHECK:     |   +-- Name `x `
// CHECK:     |   `-- Name `kX`
// CHECK:     `-- ConstDecl
// CHECK:         +-- Name `kX `
// CHECK:         `-- StringExpr `"hi"`
// CHECK:             `-- StringLiteral `hi`

{x}
VAR x = kX
CONST kX = "hi"
