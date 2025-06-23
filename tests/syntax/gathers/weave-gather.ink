// RUN: %ink-compiler < %s --stdin --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:34, line:40>
// CHECK-NEXT:    |--GatherPoint <line:34, col:1:2>
// CHECK-NEXT:    |--GatheredStmt <line:35, line:40>
// CHECK-NEXT:    |  |--ChoiceStmt <line:35, line:40>
// CHECK-NEXT:    |  |  |--ChoiceStarStmt <line:35, col:2:7>
// CHECK-NEXT:    |  |  |  |--ChoiceContentExpr <col:4, col:7>
// CHECK-NEXT:    |  |  |  |  `--ChoiceStartContentExpr `one` <col:4, col:7>
// CHECK-NEXT:    |  |  |  `--BlockStmt <line:37, line:37>
// CHECK-NEXT:    |  |  |     |--GatheredStmt <line:36, line:37>
// CHECK-NEXT:    |  |  |     |  |--ChoiceStmt <line:36, line:37>
// CHECK-NEXT:    |  |  |     |  |  `--ChoiceStarStmt <line:36, col:5:12>
// CHECK-NEXT:    |  |  |     |  |     `--ChoiceContentExpr <col:9, col:12>
// CHECK-NEXT:    |  |  |     |  |        `--ChoiceStartContentExpr `two` <col:9, col:12>
// CHECK-NEXT:    |  |  |     |  `--GatherPoint <line:37, col:4:8>
// CHECK-NEXT:    |  |  |     `--ContentStmt <line:37, col:8:13>
// CHECK-NEXT:    |  |  |        `--Content <col:8, col:13>
// CHECK-NEXT:    |  |  |           `--StringLiteral `three` <col:8, col:13>
// CHECK-NEXT:    |  |  `--ChoiceStarStmt <line:38, col:2:9>
// CHECK-NEXT:    |  |     |--ChoiceContentExpr <col:5, col:9>
// CHECK-NEXT:    |  |     |  `--ChoiceStartContentExpr `four` <col:5, col:9>
// CHECK-NEXT:    |  |     `--BlockStmt <line:39, line:39>
// CHECK-NEXT:    |  |        |--GatherPoint <line:39, col:4:8>
// CHECK-NEXT:    |  |        `--ContentStmt <line:39, col:8:12>
// CHECK-NEXT:    |  |           `--Content <col:8, col:12>
// CHECK-NEXT:    |  |              `--StringLiteral `five` <col:8, col:12>
// CHECK-NEXT:    |  `--GatherPoint <line:40, col:1:3>
// CHECK-NEXT:    `--ContentStmt <line:40, col:3:6>
// CHECK-NEXT:       `--Content <col:3, col:6>
// CHECK-NEXT:          `--StringLiteral `six` <col:3, col:6>

-
 * one
    * * two
   - - three
 *  four
   - - five
- six
