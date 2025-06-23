// RUN: %ink-compiler < %s --stdin --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:9, line:9>
// CHECK-NEXT:    `--ChoiceStmt <line:9, line:9>
// CHECK-NEXT:       `--ChoiceStarStmt <line:9, col:1:2>
// CHECK-NEXT:          `--ChoiceContentExpr <col:2, col:2>

*
