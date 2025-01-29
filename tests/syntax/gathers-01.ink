// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:15, line:17>
// CHECK-NEXT:    `--GatheredChoiceStmt <line:15, line:17>
// CHECK-NEXT:       |--ChoiceStmt <line:15, line:17>
// CHECK-NEXT:       |  |--ChoiceStarStmt <line:15, col:1:4>
// CHECK-NEXT:       |  |  `--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:       |  |     `--ChoiceStartContentExpr `A` <col:3, col:4>
// CHECK-NEXT:       |  `--ChoiceStarStmt <line:16, col:1:4>
// CHECK-NEXT:       |     `--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:       |        `--ChoiceStartContentExpr `B` <col:3, col:4>
// CHECK-NEXT:       `--GatherStmt <line:17, col:1:2>

* A
* B
-
