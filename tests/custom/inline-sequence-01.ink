// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:24, line:26>
// CHECK-NEXT:    |--ContentStmt <line:24, col:1:5>
// CHECK-NEXT:    |  `--Content <col:1, col:4>
// CHECK-NEXT:    |     `--InlineLogicExpr <col:1, col:4>
// CHECK-NEXT:    |        `--SequenceExpr <col:2, col:3>
// CHECK-NEXT:    |           `--EmptyContent <col:2, col:2>
// CHECK-NEXT:    |--ContentStmt <line:25, col:1:6>
// CHECK-NEXT:    |  `--Content <col:1, col:5>
// CHECK-NEXT:    |     `--InlineLogicExpr <col:1, col:5>
// CHECK-NEXT:    |        `--SequenceExpr <col:2, col:4>
// CHECK-NEXT:    |           |--EmptyContent <col:2, col:2>
// CHECK-NEXT:    |           `--EmptyContent <col:3, col:3>
// CHECK-NEXT:    `--ContentStmt <line:26, col:1:6>
// CHECK-NEXT:       `--Content <col:1, col:6>
// CHECK-NEXT:          `--InlineLogicExpr <col:1, col:6>
// CHECK-NEXT:             `--SequenceExpr <col:2, col:5>
// CHECK-NEXT:                |--EmptyContent <col:2, col:2>
// CHECK-NEXT:                |--EmptyContent <col:3, col:3>
// CHECK-NEXT:                `--EmptyContent <col:4, col:4>

{|}
{||}
{|||}
