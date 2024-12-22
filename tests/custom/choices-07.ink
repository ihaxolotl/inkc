// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--CompoundStmt <line:1, line:6>
// CHECK-NEXT:    `--ChoiceStmt <line:1, line:6>
// CHECK-NEXT:       |--ChoiceStarStmt <line:1, col:1:5>
// CHECK-NEXT:       |  |--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:       |  |  `--ChoiceStartContentExpr `A` <col:3, col:4>
// CHECK-NEXT:       |  `--CompoundStmt <line:2, line:6>
// CHECK-NEXT:       |     |--ContentStmt <line:2, col:1:18>
// CHECK-NEXT:       |     |  `--ContentExpr <col:1, col:17>
// CHECK-NEXT:       |     |     `--StringLiteral `Content under A!` <col:1, col:17>
// CHECK-NEXT:       |     `--ChoiceStmt <line:3, line:6>
// CHECK-NEXT:       |        |--ChoiceStarStmt <line:3, col:1:7>
// CHECK-NEXT:       |        |  `--ChoiceContentExpr <col:4, col:6>
// CHECK-NEXT:       |        |     `--ChoiceStartContentExpr `A1` <col:4, col:6>
// CHECK-NEXT:       |        `--ChoiceStarStmt <line:4, col:1:7>
// CHECK-NEXT:       |           |--ChoiceContentExpr <col:4, col:6>
// CHECK-NEXT:       |           |  `--ChoiceStartContentExpr `A2` <col:4, col:6>
// CHECK-NEXT:       |           `--CompoundStmt <line:5, line:6>
// CHECK-NEXT:       |              `--ContentStmt <line:5, col:1:19>
// CHECK-NEXT:       |                 `--ContentExpr <col:1, col:18>
// CHECK-NEXT:       |                    `--StringLiteral `Content under A2!` <col:1, col:18>
// CHECK-NEXT:       `--ChoiceStarStmt <line:6, col:1:4>
// CHECK-NEXT:          `--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:             `--ChoiceStartContentExpr `B` <col:3, col:4>

* A
Content under A!
** A1
** A2
Content under A2!
* B
