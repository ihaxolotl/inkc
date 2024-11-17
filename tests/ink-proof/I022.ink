// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     `-- ContentStmt
// CHECK:         +-- StringLiteral `My name is "`
// CHECK:         +-- BraceExpr
// CHECK:         |   `-- StringExpr `"J{"o"}e"`
// CHECK:         |       +-- StringLiteral `J`
// CHECK:         |       +-- BraceExpr
// CHECK:         |       |   `-- StringExpr `"o"`
// CHECK:         |       |       `-- StringLiteral `o`
// CHECK:         |       `-- StringLiteral `e`
// CHECK:         `-- StringLiteral `"`

My name is "{"J{"o"}e"}"
