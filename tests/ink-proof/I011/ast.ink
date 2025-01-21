// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:18, line:20>
// CHECK-NEXT:    |--VarDecl <line:18, col:1:10>
// CHECK-NEXT:    |  |--Identifier `x` <col:5, col:6>
// CHECK-NEXT:    |  `--NumberLiteral `5` <col:9, col:10>
// CHECK-NEXT:    |--TempDecl <line:19, col:3:14>
// CHECK-NEXT:    |  |--Identifier `y` <col:8, col:9>
// CHECK-NEXT:    |  `--NumberLiteral `4` <col:12, col:13>
// CHECK-NEXT:    `--ContentStmt <line:20, col:1:7>
// CHECK-NEXT:       `--Content <col:1, col:7>
// CHECK-NEXT:          |--InlineLogicExpr <col:1, col:4>
// CHECK-NEXT:          |  `--Identifier `x` <col:2, col:3>
// CHECK-NEXT:          `--InlineLogicExpr <col:4, col:7>
// CHECK-NEXT:             `--Identifier `y` <col:5, col:6>

VAR x = 5
~ temp y = 4
{x}{y}
