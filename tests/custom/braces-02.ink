// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     `-- ContentStmt
// CHECK:         `-- BraceExpr
// CHECK:             `-- NumberLiteral `1`

{1}
