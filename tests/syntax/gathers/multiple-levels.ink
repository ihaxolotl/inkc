// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:34, line:41>
// CHECK-NEXT:    |--GatheredStmt <line:34, line:41>
// CHECK-NEXT:    |  |--ChoiceStmt <line:34, line:41>
// CHECK-NEXT:    |  |  `--ChoiceStarStmt <line:34, col:1:4>
// CHECK-NEXT:    |  |     |--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:    |  |     |  `--ChoiceStartContentExpr `A` <col:3, col:4>
// CHECK-NEXT:    |  |     `--BlockStmt <line:36, line:41>
// CHECK-NEXT:    |  |        |--GatheredStmt <line:35, line:38>
// CHECK-NEXT:    |  |        |  |--ChoiceStmt <line:35, line:38>
// CHECK-NEXT:    |  |        |  |  `--ChoiceStarStmt <line:35, col:1:6>
// CHECK-NEXT:    |  |        |  |     |--ChoiceContentExpr <col:4, col:6>
// CHECK-NEXT:    |  |        |  |     |  `--ChoiceStartContentExpr `A1` <col:4, col:6>
// CHECK-NEXT:    |  |        |  |     `--BlockStmt <line:36, line:36>
// CHECK-NEXT:    |  |        |  |        `--ContentStmt <line:36, col:1:10>
// CHECK-NEXT:    |  |        |  |           `--Content <col:1, col:10>
// CHECK-NEXT:    |  |        |  |              `--StringLiteral `A1 Nested` <col:1, col:10>
// CHECK-NEXT:    |  |        |  `--GatherPoint <line:37, col:1:4>
// CHECK-NEXT:    |  |        `--ChoiceStmt <line:38, line:41>
// CHECK-NEXT:    |  |           `--ChoiceStarStmt <line:38, col:1:6>
// CHECK-NEXT:    |  |              |--ChoiceContentExpr <col:4, col:6>
// CHECK-NEXT:    |  |              |  `--ChoiceStartContentExpr `A2` <col:4, col:6>
// CHECK-NEXT:    |  |              `--BlockStmt <line:39, line:39>
// CHECK-NEXT:    |  |                 `--ContentStmt <line:39, col:1:10>
// CHECK-NEXT:    |  |                    `--Content <col:1, col:10>
// CHECK-NEXT:    |  |                       `--StringLiteral `A2 Nested` <col:1, col:10>
// CHECK-NEXT:    |  `--GatherPoint <line:40, col:1:3>
// CHECK-NEXT:    `--ContentStmt <line:41, col:1:12>
// CHECK-NEXT:       `--Content <col:1, col:12>
// CHECK-NEXT:          `--StringLiteral `Base Nested` <col:1, col:12>

* A
** A1
A1 Nested
--
** A2
A2 Nested
-
Base Nested
