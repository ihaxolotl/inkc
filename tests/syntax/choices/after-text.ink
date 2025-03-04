// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:11, line:11>
// CHECK-NEXT:    `--ChoiceStmt <line:11, line:11>
// CHECK-NEXT:       `--ChoiceStarStmt <line:11, col:1:7>
// CHECK-NEXT:          `--ChoiceContentExpr <col:3, col:7>
// CHECK-NEXT:             |--ChoiceStartContentExpr `A` <col:3, col:4>
// CHECK-NEXT:             `--ChoiceInnerContentExpr `B` <col:6, col:7>

* A[]B
