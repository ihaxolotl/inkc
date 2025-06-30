// RUN: %ink-compiler --stdin --compile-only --dump-ast < %s | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:12, line:13>
// CHECK-NEXT:    `--ChoiceStmt <line:12, line:13>
// CHECK-NEXT:       `--ChoicePlusStmt <line:12, col:1:2>
// CHECK-NEXT:          |--ChoiceContentExpr <col:2, col:2>
// CHECK-NEXT:          |  `--EmptyString <col:2, col:2>
// CHECK-NEXT:          `--BlockStmt <line:13, line:13>
// CHECK-NEXT:             `--GatherPoint <line:13, col:1:3>

+
--
