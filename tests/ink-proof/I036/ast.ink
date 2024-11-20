// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: +--BlockStmt
// CHECK-NEXT: |  +--ContentStmt
// CHECK-NEXT: |  |  `--ContentExpr
// CHECK-NEXT: |  |     `--StringLiteral `A`
// CHECK-NEXT: |  +--TempStmt
// CHECK-NEXT: |  |  +--Name `someTemp `
// CHECK-NEXT: |  |  `--CallExpr
// CHECK-NEXT: |  |     +--Name `string`
// CHECK-NEXT: |  |     `--ArgumentList
// CHECK-NEXT: |  +--ContentStmt
// CHECK-NEXT: |  |  `--ContentExpr
// CHECK-NEXT: |  |     `--StringLiteral `B`
// CHECK-NEXT: |  +--ContentStmt
// CHECK-NEXT: |  |  `--ContentExpr
// CHECK-NEXT: |  |     `--StringLiteral `A`
// CHECK-NEXT: |  +--ContentStmt
// CHECK-NEXT: |  |  `--ContentExpr
// CHECK-NEXT: |  |     `--BraceExpr
// CHECK-NEXT: |  |        `--CallExpr
// CHECK-NEXT: |  |           +--Name `string`
// CHECK-NEXT: |  |           `--ArgumentList
// CHECK-NEXT: |  `--ContentStmt
// CHECK-NEXT: |     `--ContentExpr
// CHECK-NEXT: |        `--StringLiteral `B`
// CHECK-NEXT: `--FunctionDecl
// CHECK-NEXT:    +--Name `string`
// CHECK-NEXT:    +--ParameterList
// CHECK-NEXT:    `--BlockStmt
// CHECK-NEXT:       `--ReturnStmt
// CHECK-NEXT:          `--StringExpr `"{3}"`
// CHECK-NEXT:             `--BraceExpr
// CHECK-NEXT:                `--NumberLiteral `3`

A
~temp someTemp = string()
B
A
{string()}
B
=== function string()
    ~ return "{3}"
}
