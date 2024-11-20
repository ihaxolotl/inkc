// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: +--BlockStmt
// CHECK-NEXT: |  +--ContentStmt
// CHECK-NEXT: |  |  `--ContentExpr
// CHECK-NEXT: |  |     `--StringLiteral `One`
// CHECK-NEXT: |  +--ContentStmt
// CHECK-NEXT: |  |  `--ContentExpr
// CHECK-NEXT: |  |     `--StringLiteral `Two`
// CHECK-NEXT: |  `--ContentStmt
// CHECK-NEXT: |     `--ContentExpr
// CHECK-NEXT: |        `--StringLiteral `Three`
// CHECK-NEXT: +--FunctionDecl
// CHECK-NEXT: |  +--Name `func1 `
// CHECK-NEXT: |  `--BlockStmt
// CHECK-NEXT: |     +--ContentStmt
// CHECK-NEXT: |     |  `--ContentExpr
// CHECK-NEXT: |     |     `--StringLiteral `This is a function`
// CHECK-NEXT: |     `--ReturnStmt
// CHECK-NEXT: |        `--NumberLiteral `5`
// CHECK-NEXT: +--FunctionDecl
// CHECK-NEXT: |  +--Name `func2 `
// CHECK-NEXT: |  `--BlockStmt
// CHECK-NEXT: |     +--ContentStmt
// CHECK-NEXT: |     |  `--ContentExpr
// CHECK-NEXT: |     |     `--StringLiteral `This is a function without a return value`
// CHECK-NEXT: |     `--ReturnStmt
// CHECK-NEXT: `--FunctionDecl
// CHECK-NEXT:    +--Name `add`
// CHECK-NEXT:    +--ParameterList
// CHECK-NEXT:    |  +--ParamDecl `x`
// CHECK-NEXT:    |  `--ParamDecl `y`
// CHECK-NEXT:    `--BlockStmt
// CHECK-NEXT:       +--ContentStmt
// CHECK-NEXT:       |  `--ContentExpr
// CHECK-NEXT:       |     +--StringLiteral `x = `
// CHECK-NEXT:       |     +--BraceExpr
// CHECK-NEXT:       |     |  `--Name `x`
// CHECK-NEXT:       |     +--StringLiteral `, y = `
// CHECK-NEXT:       |     `--BraceExpr
// CHECK-NEXT:       |        `--Name `y`
// CHECK-NEXT:       `--ReturnStmt
// CHECK-NEXT:          `--AddExpr
// CHECK-NEXT:             +--Name `x `
// CHECK-NEXT:             `--Name `y`

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
