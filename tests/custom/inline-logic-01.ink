// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:10, line:10>
// CHECK-NEXT:    `--ContentStmt <line:10, col:1:4>
// CHECK-NEXT:       `--Content <col:1, col:4>
// CHECK-NEXT:          `--InlineLogicExpr <col:1, col:4>
// CHECK-NEXT:             `--NumberLiteral `1` <col:2, col:3>

{1}
