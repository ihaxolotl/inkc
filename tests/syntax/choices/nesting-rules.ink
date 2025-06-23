// RUN: %ink-compiler --stdin --compile-only --dump-ast < %s | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:22, line:26>
// CHECK-NEXT:    `--ChoiceStmt <line:22, line:26>
// CHECK-NEXT:       |--ChoiceStarStmt <line:22, col:1:8>
// CHECK-NEXT:       |  `--ChoiceContentExpr <col:7, col:8>
// CHECK-NEXT:       |     `--ChoiceStartContentExpr `A` <col:7, col:8>
// CHECK-NEXT:       |--ChoiceStarStmt <line:23, col:1:7>
// CHECK-NEXT:       |  `--ChoiceContentExpr <col:6, col:7>
// CHECK-NEXT:       |     `--ChoiceStartContentExpr `B` <col:6, col:7>
// CHECK-NEXT:       |--ChoiceStarStmt <line:24, col:1:6>
// CHECK-NEXT:       |  `--ChoiceContentExpr <col:5, col:6>
// CHECK-NEXT:       |     `--ChoiceStartContentExpr `C` <col:5, col:6>
// CHECK-NEXT:       |--ChoiceStarStmt <line:25, col:1:5>
// CHECK-NEXT:       |  `--ChoiceContentExpr <col:4, col:5>
// CHECK-NEXT:       |     `--ChoiceStartContentExpr `D` <col:4, col:5>
// CHECK-NEXT:       `--ChoiceStarStmt <line:26, col:1:4>
// CHECK-NEXT:          `--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:             `--ChoiceStartContentExpr `E` <col:3, col:4>

***** A
**** B
*** C
** D
* E
