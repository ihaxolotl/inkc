// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:11, line:12>
// CHECK-NEXT:    `--ChoiceStmt <line:11, line:12>
// CHECK-NEXT:       `--ChoicePlusStmt <line:11, col:1:2>
// CHECK-NEXT:          |--ChoiceContentExpr <col:2, col:2>
// CHECK-NEXT:          `--BlockStmt <line:12, line:12>
// CHECK-NEXT:             `--GatherPoint <line:12, col:1:3>

+
--
