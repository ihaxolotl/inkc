// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     +-- VarDecl
// CHECK:     |   +-- Name `x `
// CHECK:     |   `-- StringExpr `"world"`
// CHECK:     |       `-- StringLiteral `world`
// CHECK:     `-- ContentStmt
// CHECK:         +-- StringLiteral `Hello `
// CHECK:         +-- BraceExpr
// CHECK:         |   `-- Name `x`
// CHECK:         `-- StringLiteral `.`

VAR x = "world"
Hello {x}.
