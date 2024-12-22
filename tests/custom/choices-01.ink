// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--ChoiceStmt <line:24, line:26>
// CHECK-NEXT:    |--ChoiceStarStmt <line: 24, col:1:5>
// CHECK-NEXT:    |  `--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:    |     `--ChoiceStartContentExpr `A` <col:3, col:4>
// CHECK-NEXT:    |--ChoiceStarStmt <line: 25, col:1:5>
// CHECK-NEXT:    |  `--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:    |     `--ChoiceStartContentExpr `B` <col:3, col:4>
// CHECK-NEXT:    `--ChoiceStarStmt <line: 26, col:1:4>
// CHECK-NEXT:       `--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:          `--ChoiceStartContentExpr `C` <col:3, col:4>

* A
* B
* C
