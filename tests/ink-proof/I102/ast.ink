// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: +--BlockStmt
// CHECK-NEXT: |  +--ThreadStmt
// CHECK-NEXT: |  |  `--ThreadExpr
// CHECK-NEXT: |  |     `--Name `move`
// CHECK-NEXT: |  `--ContentStmt
// CHECK-NEXT: |     `--ContentExpr
// CHECK-NEXT: |        `--StringLiteral `Limes`
// CHECK-NEXT: `--KnotDecl
// CHECK-NEXT:    +--Name `move`
// CHECK-NEXT:    `--BlockStmt
// CHECK-NEXT:       `--ChoiceStmt
// CHECK-NEXT:          `--ChoiceStarBranch
// CHECK-NEXT:             +--ChoiceContentExpr
// CHECK-NEXT:             |  +--StringLiteral `boop`
// CHECK-NEXT:             |  +--NullNode
// CHECK-NEXT:             |  `--NullNode
// CHECK-NEXT:             `--BlockStmt
// CHECK-NEXT:                `--DivertStmt
// CHECK-NEXT:                   `--DivertExpr
// CHECK-NEXT:                      `--Name `END`

<- move
Limes
=== move
    * boop
        -> END
