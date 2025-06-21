// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:22, line:23>
// CHECK-NEXT:    |--ContentStmt <line:22, col:1:17>
// CHECK-NEXT:    |  `--Content <col:1, col:17>
// CHECK-NEXT:    |     `--InlineLogicExpr <col:1, col:17>
// CHECK-NEXT:    |        `--AddExpr <col:4, col:15>
// CHECK-NEXT:    |           |--LogicalEqualityExpr <col:4, col:10>
// CHECK-NEXT:    |           |  |--NumberLiteral `1` <col:4, col:5>
// CHECK-NEXT:    |           |  `--NumberLiteral `1` <col:9, col:10>
// CHECK-NEXT:    |           `--NumberLiteral `1` <col:14, col:15>
// CHECK-NEXT:    `--ContentStmt <line:23, col:1:17>
// CHECK-NEXT:       `--Content <col:1, col:17>
// CHECK-NEXT:          `--InlineLogicExpr <col:1, col:17>
// CHECK-NEXT:             `--SubtractExpr <col:4, col:15>
// CHECK-NEXT:                |--LogicalInequalityExpr <col:4, col:10>
// CHECK-NEXT:                |  |--NumberLiteral `1` <col:4, col:5>
// CHECK-NEXT:                |  `--NumberLiteral `1` <col:9, col:10>
// CHECK-NEXT:                `--NumberLiteral `1` <col:14, col:15>

{ (1 == 1) + 1 }
{ (1 != 1) - 1 }
