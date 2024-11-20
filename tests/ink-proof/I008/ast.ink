// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: +--BlockStmt
// CHECK-NEXT: |  `--DivertStmt
// CHECK-NEXT: |     `--DivertExpr
// CHECK-NEXT: |        `--Name `outer`
// CHECK-NEXT: +--KnotDecl
// CHECK-NEXT: |  +--Name `outer`
// CHECK-NEXT: |  `--BlockStmt
// CHECK-NEXT: |     +--TempStmt
// CHECK-NEXT: |     |  +--Name `x `
// CHECK-NEXT: |     |  `--NumberLiteral `0`
// CHECK-NEXT: |     +--ExprStmt
// CHECK-NEXT: |     |  `--CallExpr
// CHECK-NEXT: |     |     +--Name `f`
// CHECK-NEXT: |     |     `--ArgumentList
// CHECK-NEXT: |     |        `--Name `x`
// CHECK-NEXT: |     +--ContentStmt
// CHECK-NEXT: |     |  `--ContentExpr
// CHECK-NEXT: |     |     `--BraceExpr
// CHECK-NEXT: |     |        `--Name `x`
// CHECK-NEXT: |     `--DivertStmt
// CHECK-NEXT: |        `--DivertExpr
// CHECK-NEXT: |           `--Name `DONE`
// CHECK-NEXT: +--FunctionDecl
// CHECK-NEXT: |  +--Name `f`
// CHECK-NEXT: |  +--ParameterList
// CHECK-NEXT: |  |  `--RefParamDecl `x`
// CHECK-NEXT: |  `--BlockStmt
// CHECK-NEXT: |     +--TempStmt
// CHECK-NEXT: |     |  +--Name `local `
// CHECK-NEXT: |     |  `--NumberLiteral `0`
// CHECK-NEXT: |     +--ExprStmt
// CHECK-NEXT: |     |  `--AssignExpr
// CHECK-NEXT: |     |     +--Name `x`
// CHECK-NEXT: |     |     `--Name `x`
// CHECK-NEXT: |     `--ContentStmt
// CHECK-NEXT: |        `--ContentExpr
// CHECK-NEXT: |           `--BraceExpr
// CHECK-NEXT: |              `--CallExpr
// CHECK-NEXT: |                 +--Name `setTo3`
// CHECK-NEXT: |                 `--ArgumentList
// CHECK-NEXT: |                    `--Name `local`
// CHECK-NEXT: `--FunctionDecl
// CHECK-NEXT:    +--Name `setTo3`
// CHECK-NEXT:    +--ParameterList
// CHECK-NEXT:    |  `--RefParamDecl `x`
// CHECK-NEXT:    `--BlockStmt
// CHECK-NEXT:       `--ExprStmt
// CHECK-NEXT:          `--AssignExpr
// CHECK-NEXT:             +--Name `x `
// CHECK-NEXT:             `--NumberLiteral `3`

-> outer
=== outer
~ temp x = 0
~ f(x)
{x}
-> DONE
=== function f(ref x)
~temp local = 0
~x=x
{setTo3(local)}
=== function setTo3(ref x)
~x = 3
