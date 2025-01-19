// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:1, line:5>
// CHECK-NEXT:    |--VarDecl <col:1, col:11>
// CHECK-NEXT:    |  |--Name `x` <col:5, col:6>
// CHECK-NEXT:    |  `--NumberLiteral `0` <col:9, col:10>
// CHECK-NEXT:    |--ContentStmt <line:2, col:1:24>
// CHECK-NEXT:    |  `--Content <col:1, col:23>
// CHECK-NEXT:    |     `--InlineLogicExpr <col:1, col:23>
// CHECK-NEXT:    |        |--Name `true` <col:2, col:6>
// CHECK-NEXT:    |        `--ConditionalContent <col:1, col:15>
// CHECK-NEXT:    |           `--GatherStmt <col:5, col:15>
// CHECK-NEXT:    `--ContentStmt <line:5, col:1:4>
// CHECK-NEXT:       `--Content <col:1, col:4>
// CHECK-NEXT:          `--InlineLogicExpr <col:1, col:4>
// CHECK-NEXT:             |--Name `x` <col:2, col:3>
// CHECK-NEXT:             `--Name `x` <col:2, col:3>

VAR x = 0
{true:
    - ~ x = 5
}
{x}
