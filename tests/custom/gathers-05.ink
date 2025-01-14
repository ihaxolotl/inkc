// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "./tests/custom/gathers-05.ink"
// CHECK-NEXT: `--BlockStmt <line:1, line:5>
// CHECK-NEXT:    |--GatheredChoiceStmt <col:1, col:27>
// CHECK-NEXT:    |  |--ChoiceStmt <line:1, line:5>
// CHECK-NEXT:    |  |  `--ChoiceStarStmt <line:1, col:1:5>
// CHECK-NEXT:    |  |     |--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:    |  |     |  `--ChoiceStartContentExpr `A` <col:3, col:4>
// CHECK-NEXT:    |  |     `--BlockStmt <line:2, line:5>
// CHECK-NEXT:    |  |        `--ChoiceStmt <line:2, line:5>
// CHECK-NEXT:    |  |           `--ChoiceStarStmt <line:2, col:1:7>
// CHECK-NEXT:    |  |              |--ChoiceContentExpr <col:4, col:6>
// CHECK-NEXT:    |  |              |  `--ChoiceStartContentExpr `A1` <col:4, col:6>
// CHECK-NEXT:    |  |              `--BlockStmt <line:3, line:5>
// CHECK-NEXT:    |  |                 `--ChoiceStmt <line:3, line:5>
// CHECK-NEXT:    |  |                    `--ChoiceStarStmt <line:3, col:1:15>
// CHECK-NEXT:    |  |                       `--ChoiceContentExpr <col:5, col:14>
// CHECK-NEXT:    |  |                          `--ChoiceStartContentExpr `A1 nested` <col:5, col:14>
// CHECK-NEXT:    |  `--GatherStmt <col:1, col:3>
// CHECK-NEXT:    `--ContentStmt <line:5, col:1:7>
// CHECK-NEXT:       `--ContentExpr <col:1, col:7>
// CHECK-NEXT:          `--StringLiteral `Hello!` <col:1, col:7>

* A
** A1
*** A1 nested
-
Hello!
