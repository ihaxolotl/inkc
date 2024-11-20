// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: +--BlockStmt
// CHECK-NEXT: |  +--ExprStmt
// CHECK-NEXT: |  |  `--CallExpr
// CHECK-NEXT: |  |     +--Name `func `
// CHECK-NEXT: |  |     `--ArgumentList
// CHECK-NEXT: |  +--ContentStmt
// CHECK-NEXT: |  |  `--ContentExpr
// CHECK-NEXT: |  |     `--StringLiteral `text 2`
// CHECK-NEXT: |  +--TempStmt
// CHECK-NEXT: |  |  +--Name `tempVar `
// CHECK-NEXT: |  |  `--CallExpr
// CHECK-NEXT: |  |     +--Name `func `
// CHECK-NEXT: |  |     `--ArgumentList
// CHECK-NEXT: |  `--ContentStmt
// CHECK-NEXT: |     `--ContentExpr
// CHECK-NEXT: |        `--StringLiteral `text 2`
// CHECK-NEXT: `--FunctionDecl
// CHECK-NEXT:    +--Name `func `
// CHECK-NEXT:    +--ParameterList
// CHECK-NEXT:    `--BlockStmt
// CHECK-NEXT:       +--ContentStmt
// CHECK-NEXT:       |  `--ContentExpr
// CHECK-NEXT:       |     `--StringLiteral `text1`
// CHECK-NEXT:       `--ReturnStmt
// CHECK-NEXT:          `--True

~ func ()
text 2
~ temp tempVar = func ()
text 2
== function func ()
    text1
    ~ return true
