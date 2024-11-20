// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: +--BlockStmt
// CHECK-NEXT: |  `--ContentStmt
// CHECK-NEXT: |     `--ContentExpr
// CHECK-NEXT: |        `--BraceExpr
// CHECK-NEXT: |           `--CallExpr
// CHECK-NEXT: |              +--Name `RunAThing`
// CHECK-NEXT: |              `--ArgumentList
// CHECK-NEXT: +--FunctionDecl
// CHECK-NEXT: |  +--Name `RunAThing `
// CHECK-NEXT: |  `--BlockStmt
// CHECK-NEXT: |     +--ContentStmt
// CHECK-NEXT: |     |  `--ContentExpr
// CHECK-NEXT: |     |     `--StringLiteral `The first line.`
// CHECK-NEXT: |     `--ContentStmt
// CHECK-NEXT: |        `--ContentExpr
// CHECK-NEXT: |           `--StringLiteral `The second line.`
// CHECK-NEXT: `--KnotDecl
// CHECK-NEXT:    +--Name `SomewhereElse `
// CHECK-NEXT:    `--BlockStmt
// CHECK-NEXT:       +--ContentStmt
// CHECK-NEXT:       |  `--ContentExpr
// CHECK-NEXT:       |     `--BraceExpr
// CHECK-NEXT:       |        `--StringExpr `"somewhere else"`
// CHECK-NEXT:       |           `--StringLiteral `somewhere else`
// CHECK-NEXT:       `--DivertStmt
// CHECK-NEXT:          `--DivertExpr
// CHECK-NEXT:             `--Name `END`

{RunAThing()}
== function RunAThing ==
The first line.
The second line.
== SomewhereElse ==
{"somewhere else"}
->END
