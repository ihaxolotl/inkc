// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- Name `x`
// CHECK:     +-- TempStmt
// CHECK:     |   +-- Name `x `
// CHECK:     |   `-- NumberLiteral `5`
// CHECK:     `-- ContentStmt
// CHECK:         `-- StringLiteral `hello`

{x}
~temp x = 5
hello
