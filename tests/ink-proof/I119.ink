// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     +-- VarDecl
// CHECK:     |   +-- Name `x `
// CHECK:     |   `-- StringExpr `"Hello world 1"`
// CHECK:     |       `-- StringLiteral `Hello world 1`
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- Name `x`
// CHECK:     `-- ContentStmt
// CHECK:         +-- StringLiteral `Hello `
// CHECK:         +-- BraceExpr
// CHECK:         |   `-- StringExpr `"world"`
// CHECK:         |       `-- StringLiteral `world`
// CHECK:         `-- StringLiteral ` 2.`

VAR x = "Hello world 1"
{x}
Hello {"world"} 2.
