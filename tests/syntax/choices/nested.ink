// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:29, line:34>
// CHECK-NEXT:    `--ChoiceStmt <line:29, line:34>
// CHECK-NEXT:       |--ChoiceStarStmt <line:29, col:1:4>
// CHECK-NEXT:       |  |--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:       |  |  `--ChoiceStartContentExpr `A` <col:3, col:4>
// CHECK-NEXT:       |  `--BlockStmt <line:31, line:33>
// CHECK-NEXT:       |     `--ChoiceStmt <line:30, line:33>
// CHECK-NEXT:       |        |--ChoiceStarStmt <line:30, col:1:19>
// CHECK-NEXT:       |        |  `--ChoiceContentExpr <col:4, col:19>
// CHECK-NEXT:       |        |     `--ChoiceStartContentExpr `Nested inside A` <col:4, col:19>
// CHECK-NEXT:       |        `--ChoiceStarStmt <line:31, col:1:24>
// CHECK-NEXT:       |           `--ChoiceContentExpr <col:4, col:24>
// CHECK-NEXT:       |              `--ChoiceStartContentExpr `Also nested inside A` <col:4, col:24>
// CHECK-NEXT:       `--ChoiceStarStmt <line:32, col:1:4>
// CHECK-NEXT:          |--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:          |  `--ChoiceStartContentExpr `B` <col:3, col:4>
// CHECK-NEXT:          `--BlockStmt <line:34, line:34>
// CHECK-NEXT:             `--ChoiceStmt <line:33, line:34>
// CHECK-NEXT:                |--ChoiceStarStmt <line:33, col:1:20>
// CHECK-NEXT:                |  `--ChoiceContentExpr <col:5, col:20>
// CHECK-NEXT:                |     `--ChoiceStartContentExpr `Nested inside B` <col:5, col:20>
// CHECK-NEXT:                `--ChoiceStarStmt <line:34, col:1:24>
// CHECK-NEXT:                   `--ChoiceContentExpr <col:4, col:24>
// CHECK-NEXT:                      `--ChoiceStartContentExpr `Also nested inside B` <col:4, col:24>

* A
** Nested inside A
** Also nested inside A
* B
*** Nested inside B
** Also nested inside B
