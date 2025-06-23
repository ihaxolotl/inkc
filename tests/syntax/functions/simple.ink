// RUN: %ink-compiler --stdin --compile-only --dump-ast < %s | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: |--BlockStmt <line:25, line:25>
// CHECK-NEXT: |  `--ContentStmt <line:25, col:1:13>
// CHECK-NEXT: |     `--Content <col:1, col:13>
// CHECK-NEXT: |        `--InlineLogicExpr <col:1, col:13>
// CHECK-NEXT: |           `--CallExpr <col:2, col:12>
// CHECK-NEXT: |              |--Identifier `func` <col:2, col:6>
// CHECK-NEXT: |              `--ArgumentList <col:6, col:12>
// CHECK-NEXT: |                 |--NumberLiteral `1` <col:7, col:8>
// CHECK-NEXT: |                 `--NumberLiteral `2` <col:10, col:11>
// CHECK-NEXT: `--FunctionDecl <line:27, line:28>
// CHECK-NEXT:    |--FunctionProto <col:1, col:23>
// CHECK-NEXT:    |  |--Identifier `func` <col:13, col:17>
// CHECK-NEXT:    |  `--ParamList <col:17, col:23>
// CHECK-NEXT:    |     |--ParamDecl `a` <col:18, col:19>
// CHECK-NEXT:    |     `--ParamDecl `b` <col:21, col:22>
// CHECK-NEXT:    `--BlockStmt <line:28, line:28>
// CHECK-NEXT:       `--ReturnStmt <line:28, col:3:15>
// CHECK-NEXT:          `--AddExpr <col:10, col:15>
// CHECK-NEXT:             |--Identifier `a` <col:10, col:11>
// CHECK-NEXT:             `--Identifier `b` <col:14, col:15>

{func(1, 2)}

== function func(a, b)
~ return a + b
