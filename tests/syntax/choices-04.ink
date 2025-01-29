// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:12, line:12>
// CHECK-NEXT:    `--ChoiceStmt <line:12, line:12>
// CHECK-NEXT:       `--ChoiceStarStmt <line:12, col:1:8>
// CHECK-NEXT:          `--ChoiceContentExpr <col:3, col:8>
// CHECK-NEXT:             |--ChoiceStartContentExpr `A` <col:3, col:4>
// CHECK-NEXT:             |--ChoiceOptionOnlyContentExpr `B` <col:5, col:6>
// CHECK-NEXT:             `--ChoiceInnerContentExpr `C` <col:7, col:8>

* A[B]C
