// RUN: %ink-compiler --stdin --compile-only --dump-ast < %s | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:9, line:9>
// CHECK-NEXT:   `--ChoiceStmt <line:9, line:9>
// CHECK-NEXT:       `--ChoiceStarStmt <line:9, col:1:5>
// CHECK-NEXT:          `--ChoiceContentExpr <col:3, col:5>

* []
