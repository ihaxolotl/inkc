// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:10, line:10>
// CHECK-NEXT:   `--ChoiceStmt <line:10, line:10>
// CHECK-NEXT:       `--ChoiceStarStmt <line:10, col:1:5>
// CHECK-NEXT:          `--ChoiceContentExpr <col:3, col:5>
// CHECK-NEXT:             `--ChoiceStartContentExpr `` <col:3, col:3>

* []
