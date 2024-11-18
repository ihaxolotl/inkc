// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: +-- BlockStmt
// CHECK: |   +-- ContentStmt
// CHECK: |   |   `-- StringLiteral `This is a thread example`
// CHECK: |   +-- ThreadStmt
// CHECK: |   |   `-- ThreadExpr
// CHECK: |   |       `-- Name `example_thread`
// CHECK: |   `-- ContentStmt
// CHECK: |       `-- StringLiteral `The example is now complete.`
// CHECK: `-- KnotDecl
// CHECK:     +-- KnotPrototype
// CHECK:     |   `-- Name `example_thread `
// CHECK:     `-- BlockStmt
// CHECK:         +-- ContentStmt
// CHECK:         |   `-- StringLiteral `Hello.`
// CHECK:         +-- DivertStmt
// CHECK:         |   `-- DivertExpr
// CHECK:         |       `-- Name `DONE`
// CHECK:         +-- ContentStmt
// CHECK:         |   `-- StringLiteral `World.`
// CHECK:         `-- DivertStmt
// CHECK:             `-- DivertExpr
// CHECK:                 `-- Name `DONE`

This is a thread example
<- example_thread
The example is now complete.
== example_thread ==
Hello.
-> DONE
World.
-> DONE
