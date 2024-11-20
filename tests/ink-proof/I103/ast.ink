// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: +--BlockStmt
// CHECK-NEXT: |  +--ContentStmt
// CHECK-NEXT: |  |  `--ContentExpr
// CHECK-NEXT: |  |     `--StringLiteral `This is a thread example`
// CHECK-NEXT: |  +--ThreadStmt
// CHECK-NEXT: |  |  `--ThreadExpr
// CHECK-NEXT: |  |     `--Name `example_thread`
// CHECK-NEXT: |  `--ContentStmt
// CHECK-NEXT: |     `--ContentExpr
// CHECK-NEXT: |        `--StringLiteral `The example is now complete.`
// CHECK-NEXT: `--KnotDecl
// CHECK-NEXT:    +--Name `example_thread `
// CHECK-NEXT:    `--BlockStmt
// CHECK-NEXT:       +--ContentStmt
// CHECK-NEXT:       |  `--ContentExpr
// CHECK-NEXT:       |     `--StringLiteral `Hello.`
// CHECK-NEXT:       +--DivertStmt
// CHECK-NEXT:       |  `--DivertExpr
// CHECK-NEXT:       |     `--Name `DONE`
// CHECK-NEXT:       +--ContentStmt
// CHECK-NEXT:       |  `--ContentExpr
// CHECK-NEXT:       |     `--StringLiteral `World.`
// CHECK-NEXT:       `--DivertStmt
// CHECK-NEXT:          `--DivertExpr
// CHECK-NEXT:             `--Name `DONE`

This is a thread example
<- example_thread
The example is now complete.
== example_thread ==
Hello.
-> DONE
World.
-> DONE
