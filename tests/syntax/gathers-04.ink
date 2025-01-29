// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:39, line:48>
// CHECK-NEXT:    |--GatheredChoiceStmt <line:39, line:48>
// CHECK-NEXT:    |  |--ChoiceStmt <line:39, line:48>
// CHECK-NEXT:    |  |  |--ChoiceStarStmt <line:39, col:1:4>
// CHECK-NEXT:    |  |  |  |--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:    |  |  |  |  `--ChoiceStartContentExpr `A` <col:3, col:4>
// CHECK-NEXT:    |  |  |  `--BlockStmt <line:41, line:46>
// CHECK-NEXT:    |  |  |     |--GatheredChoiceStmt <line:40, line:43>
// CHECK-NEXT:    |  |  |     |  |--ChoiceStmt <line:40, line:43>
// CHECK-NEXT:    |  |  |     |  |  `--ChoiceStarStmt <line:40, col:1:6>
// CHECK-NEXT:    |  |  |     |  |     |--ChoiceContentExpr <col:4, col:6>
// CHECK-NEXT:    |  |  |     |  |     |  `--ChoiceStartContentExpr `A1` <col:4, col:6>
// CHECK-NEXT:    |  |  |     |  |     `--BlockStmt <line:41, line:41>
// CHECK-NEXT:    |  |  |     |  |        `--ContentStmt <line:41, col:1:10>
// CHECK-NEXT:    |  |  |     |  |           `--Content <col:1, col:10>
// CHECK-NEXT:    |  |  |     |  |              `--StringLiteral `A1 Nested` <col:1, col:10>
// CHECK-NEXT:    |  |  |     |  `--GatherStmt <line:42, col:1:3>
// CHECK-NEXT:    |  |  |     `--GatheredChoiceStmt <line:43, line:46>
// CHECK-NEXT:    |  |  |        |--ChoiceStmt <line:43, line:46>
// CHECK-NEXT:    |  |  |        |  `--ChoiceStarStmt <line:43, col:1:6>
// CHECK-NEXT:    |  |  |        |     |--ChoiceContentExpr <col:4, col:6>
// CHECK-NEXT:    |  |  |        |     |  `--ChoiceStartContentExpr `A2` <col:4, col:6>
// CHECK-NEXT:    |  |  |        |     `--BlockStmt <line:44, line:44>
// CHECK-NEXT:    |  |  |        |        `--ContentStmt <line:44, col:1:10>
// CHECK-NEXT:    |  |  |        |           `--Content <col:1, col:10>
// CHECK-NEXT:    |  |  |        |              `--StringLiteral `A2 Nested` <col:1, col:10>
// CHECK-NEXT:    |  |  |        `--GatherStmt <line:45, col:1:3>
// CHECK-NEXT:    |  |  `--ChoiceStarStmt <line:46, col:1:4>
// CHECK-NEXT:    |  |     `--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:    |  |        `--ChoiceStartContentExpr `B` <col:3, col:4>
// CHECK-NEXT:    |  `--GatherStmt <line:47, col:1:2>
// CHECK-NEXT:    `--ContentStmt <line:48, col:1:12>
// CHECK-NEXT:       `--Content <col:1, col:12>
// CHECK-NEXT:          `--StringLiteral `Base Nested` <col:1, col:12>

* A
** A1
A1 Nested
--
** A2
A2 Nested
--
* B
-
Base Nested
