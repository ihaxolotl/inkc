// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: +--BlockStmt
// CHECK-NEXT: |  `--DivertStmt
// CHECK-NEXT: |     `--DivertExpr
// CHECK-NEXT: |        `--Name `firstKnot`
// CHECK-NEXT: +--KnotDecl
// CHECK-NEXT: |  +--Name `firstKnot`
// CHECK-NEXT: |  `--BlockStmt
// CHECK-NEXT: |     +--ContentStmt
// CHECK-NEXT: |     |  `--ContentExpr
// CHECK-NEXT: |     |     `--StringLiteral `Hello!`
// CHECK-NEXT: |     `--DivertStmt
// CHECK-NEXT: |        `--DivertExpr
// CHECK-NEXT: |           `--Name `anotherKnot`
// CHECK-NEXT: `--KnotDecl
// CHECK-NEXT:    +--Name `anotherKnot`
// CHECK-NEXT:    `--BlockStmt
// CHECK-NEXT:       +--ContentStmt
// CHECK-NEXT:       |  `--ContentExpr
// CHECK-NEXT:       |     `--StringLiteral `World.`
// CHECK-NEXT:       `--DivertStmt
// CHECK-NEXT:          `--DivertExpr
// CHECK-NEXT:             `--Name `END`

-> firstKnot
=== firstKnot
    Hello!
    -> anotherKnot
=== anotherKnot
    World.
    -> END
