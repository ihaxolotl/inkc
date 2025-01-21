// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: |--BlockStmt <line:4, line:6>
// CHECK-NEXT: |  `--LogicStmt <col:1, col:11>
// CHECK-NEXT: |     `--CallExpr <col:3, col:10>
// CHECK-NEXT: |        |--Identifier `f` <col:3, col:4>
// CHECK-NEXT: |        `--ArgumentList <col:4, col:10>
// CHECK-NEXT: |           |--NumberLiteral `1` <col:5, col:6>
// CHECK-NEXT: |           `--NumberLiteral `1` <col:8, col:9>
// CHECK-NEXT: `--KnotDecl <col:1, col:95>
// CHECK-NEXT:    |--KnotProto <col:1, col:24>
// CHECK-NEXT:    |  |--Identifier `f` <col:13, col:14>
// CHECK-NEXT:    |  `--ParamList <col:14, col:21>
// CHECK-NEXT:    |     |--ParamDecl `x` <col:15, col:16>
// CHECK-NEXT:    |     `--ParamDecl `y` <col:18, col:19>
// CHECK-NEXT:    `--BlockStmt <line:7, line:12>
// CHECK-NEXT:       |--ContentStmt <line:6, col:1:64>
// CHECK-NEXT:       |  `--Content <col:1, col:63>
// CHECK-NEXT:       |     `--InlineLogicExpr <col:1, col:63>
// CHECK-NEXT:       |        |--AndExpr <col:15, col:20>
// CHECK-NEXT:       |        |  |--LogicalEqualityExpr <col:9, col:10>
// CHECK-NEXT:       |        |  |  |--Identifier `x` <col:3, col:4>
// CHECK-NEXT:       |        |  |  `--NumberLiteral `1` <col:8, col:9>
// CHECK-NEXT:       |        |  `--LogicalEqualityExpr <col:20, col:20>
// CHECK-NEXT:       |        |     |--Identifier `y` <col:14, col:15>
// CHECK-NEXT:       |        |     `--NumberLiteral `1` <col:19, col:20>
// CHECK-NEXT:       |        `--ConditionalContent <col:1, col:41>
// CHECK-NEXT:       |           |--LogicStmt <col:3, col:11>
// CHECK-NEXT:       |           |  `--AssignExpr <col:10, col:10>
// CHECK-NEXT:       |           |     |--Identifier `x` <col:5, col:6>
// CHECK-NEXT:       |           |     `--NumberLiteral `2` <col:9, col:10>
// CHECK-NEXT:       |           |--BlockStmt <line:8, line:10>
// CHECK-NEXT:       |           |  `--LogicStmt <col:3, col:13>
// CHECK-NEXT:       |           |     `--CallExpr <col:5, col:12>
// CHECK-NEXT:       |           |        |--Identifier `f` <col:5, col:6>
// CHECK-NEXT:       |           |        `--ArgumentList <col:6, col:12>
// CHECK-NEXT:       |           |           |--Identifier `y` <col:7, col:8>
// CHECK-NEXT:       |           |           `--Identifier `x` <col:10, col:11>
// CHECK-NEXT:       |           `--ConditionalElseBranch <col:3, col:8>
// CHECK-NEXT:       |              `--BlockStmt <line:10, line:11>
// CHECK-NEXT:       |                 `--ContentStmt <line:10, col:3:11>
// CHECK-NEXT:       |                    `--Content <col:3, col:10>
// CHECK-NEXT:       |                       |--InlineLogicExpr <col:3, col:6>
// CHECK-NEXT:       |                       |  `--Identifier `x` <col:4, col:5>
// CHECK-NEXT:       |                       |--StringLiteral ` ` <col:6, col:7>
// CHECK-NEXT:       |                       `--InlineLogicExpr <col:7, col:10>
// CHECK-NEXT:       |                          `--Identifier `y` <col:8, col:9>
// CHECK-NEXT:       `--LogicStmt <col:1, col:9>
// CHECK-NEXT:          `--ReturnStmt <col:3, col:9>

~ f(1, 1)
== function f(x, y) ==
{ x == 1 and y == 1:
  ~ x = 2
  ~ f(y, x)
- else:
  {x} {y}
}
~ return
