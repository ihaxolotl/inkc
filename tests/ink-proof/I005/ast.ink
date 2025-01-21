// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:16, line:18>
// CHECK-NEXT:    |--VarDecl <line:16, col:1:11>
// CHECK-NEXT:    |  |--Identifier `x` <col:5, col:6>
// CHECK-NEXT:    |  `--Identifier `c` <col:9, col:10>
// CHECK-NEXT:    |--ConstDecl <line:17, col:1:13>
// CHECK-NEXT:    |  |--Identifier `c` <col:7, col:8>
// CHECK-NEXT:    |  `--NumberLiteral `5` <col:11, col:12>
// CHECK-NEXT:    `--ContentStmt <line:18, col:1:4>
// CHECK-NEXT:       `--Content <col:1, col:4>
// CHECK-NEXT:          `--InlineLogicExpr <col:1, col:4>
// CHECK-NEXT:             `--Identifier `x` <col:2, col:3>

VAR x = c
CONST c = 5
{x}
