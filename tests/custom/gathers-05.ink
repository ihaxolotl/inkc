// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:1, line:29>
// CHECK-NEXT:    |--GatheredChoiceStmt <line:25, line:28>
// CHECK-NEXT:    |  |--ChoiceStmt <line:25, line:29>
// CHECK-NEXT:    |  |  `--ChoiceStarStmt <line:25, col:1:5>
// CHECK-NEXT:    |  |     |--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:    |  |     |  `--ChoiceStartContentExpr `A` <col:3, col:4>
// CHECK-NEXT:    |  |     `--BlockStmt <line:26, line:29>
// CHECK-NEXT:    |  |        `--ChoiceStmt <line:26, line:29>
// CHECK-NEXT:    |  |           `--ChoiceStarStmt <line:26, col:1:7>
// CHECK-NEXT:    |  |              |--ChoiceContentExpr <col:4, col:6>
// CHECK-NEXT:    |  |              |  `--ChoiceStartContentExpr `A1` <col:4, col:6>
// CHECK-NEXT:    |  |              `--BlockStmt <line:27, line:29>
// CHECK-NEXT:    |  |                 `--ChoiceStmt <line:27, line:29>
// CHECK-NEXT:    |  |                    `--ChoiceStarStmt <line:27, col:1:15>
// CHECK-NEXT:    |  |                       `--ChoiceContentExpr <col:5, col:14>
// CHECK-NEXT:    |  |                          `--ChoiceStartContentExpr `A1 nested` <col:5, col:14>
// CHECK-NEXT:    |  `--GatherStmt <col:1, col:3>
// CHECK-NEXT:    `--ContentStmt <line:29, col:1:7>
// CHECK-NEXT:       `--ContentExpr <col:1, col:7>
// CHECK-NEXT:          `--StringLiteral `Hello!` <col:1, col:7>

* A
** A1
*** A1 nested
-
Hello!
