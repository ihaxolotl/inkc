// RUN: %ink-compiler < %s --dump-ast --compile-only | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: |--BlockStmt <line:25, line:25>
// CHECK-NEXT: |  `--ExprStmt <line:25, col:3:7>
// CHECK-NEXT: |     `--CallExpr <col:3, col:7>
// CHECK-NEXT: |        |--Identifier `a` <col:3, col:4>
// CHECK-NEXT: |        `--ArgumentList <col:4, col:7>
// CHECK-NEXT: |           `--NumberLiteral `1` <col:5, col:6>
// CHECK-NEXT: `--FunctionDecl <line:27, line:28>
// CHECK-NEXT:    |--FunctionProto <col:1, col:26>
// CHECK-NEXT:    |  |--Identifier `a` <col:13, col:14>
// CHECK-NEXT:    |  `--ParamList <col:14, col:26>
// CHECK-NEXT:    |     |--ParamDecl `arg1` <col:15, col:19>
// CHECK-NEXT:    |     `--ParamDecl `arg2` <col:21, col:25>
// CHECK-NEXT:    `--BlockStmt <line:28, line:28>
// CHECK-NEXT:       `--ReturnStmt <line:28, col:3:21>
// CHECK-NEXT:          `--AddExpr <col:10, col:21>
// CHECK-NEXT:             |--Identifier `arg1` <col:10, col:14>
// CHECK-NEXT:             `--Identifier `arg2` <col:17, col:21>
// CHECK-NEXT: <STDIN>:25:3: error: too few arguments to 'a'
// CHECK-NEXT:   25 | ~ a(1)
// CHECK-NEXT:      |   ^

~ a(1)

== function a(arg1, arg2)
~ return arg1 + arg2
