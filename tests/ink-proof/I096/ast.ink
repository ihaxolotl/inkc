// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: +--BlockStmt
// CHECK-NEXT: |  +--VarDecl
// CHECK-NEXT: |  |  +--Name `globalVal `
// CHECK-NEXT: |  |  `--NumberLiteral `5`
// CHECK-NEXT: |  +--ContentStmt
// CHECK-NEXT: |  |  `--ContentExpr
// CHECK-NEXT: |  |     `--BraceExpr
// CHECK-NEXT: |  |        `--Name `globalVal`
// CHECK-NEXT: |  +--ExprStmt
// CHECK-NEXT: |  |  `--CallExpr
// CHECK-NEXT: |  |     +--Name `squaresquare`
// CHECK-NEXT: |  |     `--ArgumentList
// CHECK-NEXT: |  |        `--Name `globalVal`
// CHECK-NEXT: |  `--ContentStmt
// CHECK-NEXT: |     `--ContentExpr
// CHECK-NEXT: |        `--BraceExpr
// CHECK-NEXT: |           `--Name `globalVal`
// CHECK-NEXT: +--FunctionDecl
// CHECK-NEXT: |  +--Name `squaresquare`
// CHECK-NEXT: |  +--ParameterList
// CHECK-NEXT: |  |  `--RefParamDecl `x`
// CHECK-NEXT: |  `--BlockStmt
// CHECK-NEXT: |     +--ContentStmt
// CHECK-NEXT: |     |  `--ContentExpr
// CHECK-NEXT: |     |     +--BraceExpr
// CHECK-NEXT: |     |     |  `--CallExpr
// CHECK-NEXT: |     |     |     +--Name `square`
// CHECK-NEXT: |     |     |     `--ArgumentList
// CHECK-NEXT: |     |     |        `--Name `x`
// CHECK-NEXT: |     |     +--StringLiteral ` `
// CHECK-NEXT: |     |     `--BraceExpr
// CHECK-NEXT: |     |        `--CallExpr
// CHECK-NEXT: |     |           +--Name `square`
// CHECK-NEXT: |     |           `--ArgumentList
// CHECK-NEXT: |     |              `--Name `x`
// CHECK-NEXT: |     `--ReturnStmt
// CHECK-NEXT: `--FunctionDecl
// CHECK-NEXT:    +--Name `square`
// CHECK-NEXT:    +--ParameterList
// CHECK-NEXT:    |  `--RefParamDecl `x`
// CHECK-NEXT:    `--BlockStmt
// CHECK-NEXT:       +--ExprStmt
// CHECK-NEXT:       |  `--AssignExpr
// CHECK-NEXT:       |     +--Name `x `
// CHECK-NEXT:       |     `--MultiplyExpr
// CHECK-NEXT:       |        +--Name `x `
// CHECK-NEXT:       |        `--Name `x`
// CHECK-NEXT:       `--ReturnStmt

VAR globalVal = 5
{globalVal}
~ squaresquare(globalVal)
{globalVal}
== function squaresquare(ref x) ==
 {square(x)} {square(x)}
 ~ return
== function square(ref x) ==
 ~ x = x * x
 ~ return
