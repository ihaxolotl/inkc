// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:54, line:60>
// CHECK-NEXT:    |--ContentStmt <line:54, col:1:18>
// CHECK-NEXT:    |  `--Content <col:1, col:18>
// CHECK-NEXT:    |     `--InlineLogicExpr <col:1, col:18>
// CHECK-NEXT:    |        `--AddExpr <col:3, col:16>
// CHECK-NEXT:    |           |--MultiplyExpr <col:3, col:8>
// CHECK-NEXT:    |           |  |--NumberLiteral `2` <col:3, col:4>
// CHECK-NEXT:    |           |  `--NumberLiteral `3` <col:7, col:8>
// CHECK-NEXT:    |           `--MultiplyExpr <col:11, col:16>
// CHECK-NEXT:    |              |--NumberLiteral `5` <col:11, col:12>
// CHECK-NEXT:    |              `--NumberLiteral `6` <col:15, col:16>
// CHECK-NEXT:    |--ContentStmt <line:55, col:1:10>
// CHECK-NEXT:    |  `--Content <col:1, col:10>
// CHECK-NEXT:    |     `--InlineLogicExpr <col:1, col:10>
// CHECK-NEXT:    |        `--ModExpr <col:2, col:9>
// CHECK-NEXT:    |           |--NumberLiteral `8` <col:2, col:3>
// CHECK-NEXT:    |           `--NumberLiteral `3` <col:8, col:9>
// CHECK-NEXT:    |--ContentStmt <line:56, col:1:9>
// CHECK-NEXT:    |  `--Content <col:1, col:9>
// CHECK-NEXT:    |     `--InlineLogicExpr <col:1, col:9>
// CHECK-NEXT:    |        `--ModExpr <col:2, col:8>
// CHECK-NEXT:    |           |--NumberLiteral `13` <col:2, col:4>
// CHECK-NEXT:    |           `--NumberLiteral `5` <col:7, col:8>
// CHECK-NEXT:    |--ContentStmt <line:57, col:1:10>
// CHECK-NEXT:    |  `--Content <col:1, col:10>
// CHECK-NEXT:    |     `--InlineLogicExpr <col:1, col:10>
// CHECK-NEXT:    |        `--DivideExpr <col:3, col:8>
// CHECK-NEXT:    |           |--NumberLiteral `7` <col:3, col:4>
// CHECK-NEXT:    |           `--NumberLiteral `3` <col:7, col:8>
// CHECK-NEXT:    |--ContentStmt <line:58, col:1:12>
// CHECK-NEXT:    |  `--Content <col:1, col:12>
// CHECK-NEXT:    |     `--InlineLogicExpr <col:1, col:12>
// CHECK-NEXT:    |        `--DivideExpr <col:3, col:10>
// CHECK-NEXT:    |           |--NumberLiteral `5` <col:3, col:4>
// CHECK-NEXT:    |           `--NumberLiteral `2.0` <col:7, col:10>
// CHECK-NEXT:    |--ContentStmt <line:59, col:1:11>
// CHECK-NEXT:    |  `--Content <col:1, col:11>
// CHECK-NEXT:    |     `--InlineLogicExpr <col:1, col:11>
// CHECK-NEXT:    |        `--SubtractExpr <col:3, col:9>
// CHECK-NEXT:    |           |--NumberLiteral `10` <col:3, col:5>
// CHECK-NEXT:    |           `--NumberLiteral `2` <col:8, col:9>
// CHECK-NEXT:    `--ContentStmt <line:60, col:1:14>
// CHECK-NEXT:       `--Content <col:1, col:14>
// CHECK-NEXT:          `--InlineLogicExpr <col:1, col:14>
// CHECK-NEXT:             `--MultiplyExpr <col:3, col:11>
// CHECK-NEXT:                |--NumberLiteral `2` <col:3, col:4>
// CHECK-NEXT:                `--SubtractExpr <col:8, col:11>
// CHECK-NEXT:                   |--NumberLiteral `5` <col:8, col:9>
// CHECK-NEXT:                   `--NumberLiteral `1` <col:10, col:11>

{ 2 * 3 + 5 * 6 }
{8 mod 3}
{13 % 5}
{ 7 / 3 }
{ 5 / 2.0 }
{ 10 - 2 }
{ 2 * (5-1) }
