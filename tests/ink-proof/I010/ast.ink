// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:16, line:18>
// CHECK-NEXT:    |--ContentStmt <line:16, col:1:4>
// CHECK-NEXT:    |  `--Content <col:1, col:4>
// CHECK-NEXT:    |     `--InlineLogicExpr <col:1, col:4>
// CHECK-NEXT:    |        `--Identifier `x` <col:2, col:3>
// CHECK-NEXT:    |--TempDecl <line:17, col:2:13>
// CHECK-NEXT:    |  |--Identifier `x` <col:7, col:8>
// CHECK-NEXT:    |  `--NumberLiteral `5` <col:11, col:12>
// CHECK-NEXT:    `--ContentStmt <line:18, col:1:6>
// CHECK-NEXT:       `--Content <col:1, col:6>
// CHECK-NEXT:          `--StringLiteral `hello` <col:1, col:6>

{x}
~temp x = 5
hello
