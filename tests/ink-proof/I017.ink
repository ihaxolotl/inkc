// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- StringLiteral `hello`
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- DivertExpr
// CHECK:     |       `-- Name `END`
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- StringLiteral `world`
// CHECK:     `-- ContentStmt
// CHECK:         `-- DivertExpr
// CHECK:             `-- Name `END`

hello
-> END
world
-> END
