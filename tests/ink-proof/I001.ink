// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:    `-- ContentStmt
// CHECK:        `-- StringLiteral `Hello, world!`

Hello, world!
