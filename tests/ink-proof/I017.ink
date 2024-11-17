// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- StringLiteral `hello`
// CHECK:     +-- DivertStmt
// CHECK:     |   `-- Name `END`
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- StringLiteral `world`
// CHECK:     `-- DivertStmt
// CHECK:         `-- Name `END`

hello
-> END
world
-> END
