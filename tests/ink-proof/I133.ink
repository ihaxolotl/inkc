// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     `-- ContentStmt
// CHECK:         `-- BraceExpr
// CHECK:             `-- DivideExpr
// CHECK:                 +-- NumberLiteral `7 `
// CHECK:                 `-- NumberLiteral `3.0 `

{ 7 / 3.0 }
