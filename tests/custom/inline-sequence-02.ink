// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:16, line:16>
// CHECK-NEXT:    `--ContentStmt <line:16, col:1:8>
// CHECK-NEXT:       `--Content <col:1, col:8>
// CHECK-NEXT:          `--InlineLogicExpr <col:1, col:8>
// CHECK-NEXT:             `--SequenceExpr <col:2, col:7>
// CHECK-NEXT:                |--Content <col:2, col:3>
// CHECK-NEXT:                |  `--StringLiteral `a` <col:2, col:3>
// CHECK-NEXT:                |--Content <col:4, col:5>
// CHECK-NEXT:                |  `--StringLiteral `b` <col:4, col:5>
// CHECK-NEXT:                `--Content <col:6, col:7>
// CHECK-NEXT:                   `--StringLiteral `c` <col:6, col:7>

{a|b|c}
