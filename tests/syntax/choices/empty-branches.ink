// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:28, line:33>
// CHECK-NEXT:    `--ChoiceStmt <line:28, line:33>
// CHECK-NEXT:       |--ChoiceStarStmt <line:28, col:1:4>
// CHECK-NEXT:       |  |--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:       |  |  `--ChoiceStartContentExpr `A` <col:3, col:4>
// CHECK-NEXT:       |  `--BlockStmt <line:29, line:33>
// CHECK-NEXT:       |     |--ContentStmt <line:29, col:1:17>
// CHECK-NEXT:       |     |  `--Content <col:1, col:17>
// CHECK-NEXT:       |     |     `--StringLiteral `Content under A!` <col:1, col:17>
// CHECK-NEXT:       |     `--ChoiceStmt <line:30, line:33>
// CHECK-NEXT:       |        |--ChoiceStarStmt <line:30, col:1:6>
// CHECK-NEXT:       |        |  `--ChoiceContentExpr <col:4, col:6>
// CHECK-NEXT:       |        |     `--ChoiceStartContentExpr `A1` <col:4, col:6>
// CHECK-NEXT:       |        `--ChoiceStarStmt <line:31, col:1:6>
// CHECK-NEXT:       |           |--ChoiceContentExpr <col:4, col:6>
// CHECK-NEXT:       |           |  `--ChoiceStartContentExpr `A2` <col:4, col:6>
// CHECK-NEXT:       |           `--BlockStmt <line:32, line:32>
// CHECK-NEXT:       |              `--ContentStmt <line:32, col:1:18>
// CHECK-NEXT:       |                 `--Content <col:1, col:18>
// CHECK-NEXT:       |                    `--StringLiteral `Content under A2!` <col:1, col:18>
// CHECK-NEXT:       `--ChoiceStarStmt <line:33, col:1:4>
// CHECK-NEXT:          `--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:             `--ChoiceStartContentExpr `B` <col:3, col:4>

* A
Content under A!
** A1
** A2
Content under A2!
* B
