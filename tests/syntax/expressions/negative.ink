// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:37, line:42>
// CHECK-NEXT:    |--VarDecl <line:37, col:1:25>
// CHECK-NEXT:    |  |--Identifier `negativeLiteral` <col:5, col:20>
// CHECK-NEXT:    |  `--NegateExpr <col:23, col:25>
// CHECK-NEXT:    |     `--NumberLiteral `1` <col:24, col:25>
// CHECK-NEXT:    |--VarDecl <line:38, col:1:37>
// CHECK-NEXT:    |  |--Identifier `negativeLiteral2` <col:5, col:21>
// CHECK-NEXT:    |  `--NotExpr <col:24, col:37>
// CHECK-NEXT:    |     `--NotExpr <col:28, col:37>
// CHECK-NEXT:    |        `--False <col:32, col:37>
// CHECK-NEXT:    |--VarDecl <line:39, col:1:28>
// CHECK-NEXT:    |  |--Identifier `negativeLiteral3` <col:5, col:21>
// CHECK-NEXT:    |  `--NotExpr <col:24, col:27>
// CHECK-NEXT:    |     `--NumberLiteral `0` <col:26, col:27>
// CHECK-NEXT:    |--ContentStmt <line:40, col:1:22>
// CHECK-NEXT:    |  `--Content <col:1, col:22>
// CHECK-NEXT:    |     `--InlineLogicExpr <col:1, col:22>
// CHECK-NEXT:    |        `--AddExpr <col:2, col:21>
// CHECK-NEXT:    |           |--Identifier `negativeLiteral` <col:2, col:17>
// CHECK-NEXT:    |           `--NumberLiteral `0` <col:20, col:21>
// CHECK-NEXT:    |--ContentStmt <line:41, col:1:23>
// CHECK-NEXT:    |  `--Content <col:1, col:23>
// CHECK-NEXT:    |     `--InlineLogicExpr <col:1, col:23>
// CHECK-NEXT:    |        `--AddExpr <col:2, col:22>
// CHECK-NEXT:    |           |--Identifier `negativeLiteral2` <col:2, col:18>
// CHECK-NEXT:    |           `--NumberLiteral `0` <col:21, col:22>
// CHECK-NEXT:    `--ContentStmt <line:42, col:1:23>
// CHECK-NEXT:       `--Content <col:1, col:23>
// CHECK-NEXT:          `--InlineLogicExpr <col:1, col:23>
// CHECK-NEXT:             `--AddExpr <col:2, col:22>
// CHECK-NEXT:                |--Identifier `negativeLiteral3` <col:2, col:18>
// CHECK-NEXT:                `--NumberLiteral `0` <col:21, col:22>

VAR negativeLiteral = -1
VAR negativeLiteral2 = not not false
VAR negativeLiteral3 = !(0)
{negativeLiteral + 0}
{negativeLiteral2 + 0}
{negativeLiteral3 + 0}
