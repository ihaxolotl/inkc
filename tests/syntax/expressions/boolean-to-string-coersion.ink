// RUN: %ink-compiler --stdin --compile-only --dump-ast < %s | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:18, line:19>
// CHECK-NEXT:    |--ContentStmt <line:18, col:1:11>
// CHECK-NEXT:    |  `--Content <col:1, col:11>
// CHECK-NEXT:    |     `--InlineLogicExpr <col:1, col:11>
// CHECK-NEXT:    |        `--LogicalEqualityExpr <col:3, col:9>
// CHECK-NEXT:    |           |--NumberLiteral `1` <col:3, col:4>
// CHECK-NEXT:    |           `--NumberLiteral `1` <col:8, col:9>
// CHECK-NEXT:    `--ContentStmt <line:19, col:1:11>
// CHECK-NEXT:       `--Content <col:1, col:11>
// CHECK-NEXT:          `--InlineLogicExpr <col:1, col:11>
// CHECK-NEXT:             `--LogicalInequalityExpr <col:3, col:9>
// CHECK-NEXT:                |--NumberLiteral `1` <col:3, col:4>
// CHECK-NEXT:                `--NumberLiteral `1` <col:8, col:9>

{ 1 == 1 }
{ 1 != 1 }
