// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:1, line:18>
// CHECK-NEXT:    `--ChoiceStmt <line:16, line:18>
// CHECK-NEXT:       |--ChoiceStarStmt <line:16, col:1:5>
// CHECK-NEXT:       |  `--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:       |     `--ChoiceStartContentExpr `A` <col:3, col:4>
// CHECK-NEXT:       |--ChoiceStarStmt <line:17, col:1:5>
// CHECK-NEXT:       |  `--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:       |     `--ChoiceStartContentExpr `B` <col:3, col:4>
// CHECK-NEXT:       `--ChoiceStarStmt <line:18, col:1:4>
// CHECK-NEXT:          `--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:             `--ChoiceStartContentExpr `C` <col:3, col:4>

* A
* B
* C
