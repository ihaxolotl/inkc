// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: +-- BlockStmt
// CHECK: |   +-- ContentStmt
// CHECK: |   |   `-- StringLiteral `One`
// CHECK: |   +-- ContentStmt
// CHECK: |   |   `-- StringLiteral `Two`
// CHECK: |   `-- ContentStmt
// CHECK: |       `-- StringLiteral `Three`
// CHECK: +-- KnotDecl
// CHECK: |   +-- KnotPrototype
// CHECK: |   |   `-- Name `func1 `
// CHECK: |   `-- BlockStmt
// CHECK: |       +-- ContentStmt
// CHECK: |       |   `-- StringLiteral `This is a function`
// CHECK: |       `-- ReturnStmt
// CHECK: |           `-- NumberLiteral `5`
// CHECK: +-- KnotDecl
// CHECK: |   +-- KnotPrototype
// CHECK: |   |   `-- Name `func2 `
// CHECK: |   `-- BlockStmt
// CHECK: |       +-- ContentStmt
// CHECK: |       |   `-- StringLiteral `This is a function without a return value`
// CHECK: |       `-- ReturnStmt
// CHECK: `-- KnotDecl
// CHECK:     +-- KnotPrototype
// CHECK:     |   +-- Name `add`
// CHECK:     |   `-- ParameterList
// CHECK:     |       +-- ParamDecl `x`
// CHECK:     |       `-- ParamDecl `y`
// CHECK:     `-- BlockStmt
// CHECK:         +-- ContentStmt
// CHECK:         |   +-- StringLiteral `x = `
// CHECK:         |   +-- BraceExpr
// CHECK:         |   |   `-- Name `x`
// CHECK:         |   +-- StringLiteral `, y = `
// CHECK:         |   `-- BraceExpr
// CHECK:         |       `-- Name `y`
// CHECK:         `-- ReturnStmt
// CHECK:             `-- AddExpr
// CHECK:                 +-- Name `x `
// CHECK:                 `-- Name `y`

One
Two
Three
== function func1 ==
This is a function
~ return 5
== function func2 ==
This is a function without a return value
~ return
== function add(x,y) ==
x = {x}, y = {y}
~ return x + y
