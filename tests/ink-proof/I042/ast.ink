// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: +--BlockStmt
// CHECK-NEXT: |  `--DivertStmt
// CHECK-NEXT: |     `--DivertExpr
// CHECK-NEXT: |        `--Name `test`
// CHECK-NEXT: `--KnotDecl
// CHECK-NEXT:    +--KnotPrototype
// CHECK-NEXT:    |  `--Name `test`
// CHECK-NEXT:    `--BlockStmt
// CHECK-NEXT:       `--ChoiceStmt
// CHECK-NEXT:          `--ChoiceStarBranch
// CHECK-NEXT:             +--ChoiceContentExpr
// CHECK-NEXT:             |  +--StringLiteral `Hello`
// CHECK-NEXT:             |  +--StringLiteral `.`
// CHECK-NEXT:             |  `--StringLiteral `, world.`
// CHECK-NEXT:             `--BlockStmt
// CHECK-NEXT:                `--DivertStmt
// CHECK-NEXT:                   `--DivertExpr
// CHECK-NEXT:                      `--Name `END`

-> test
=== test
    * Hello[.], world.
    -> END
