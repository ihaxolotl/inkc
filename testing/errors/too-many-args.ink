// RUN: %ink-compiler < %s --dump-ast --compile-only | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: |--BlockStmt <line:27, line:27>
// CHECK-NEXT: |  `--ExprStmt <line:27, col:4:13>
// CHECK-NEXT: |     `--CallExpr <col:4, col:13>
// CHECK-NEXT: |        |--Identifier `a` <col:3, col:4>
// CHECK-NEXT: |        `--ArgumentList <col:4, col:13>
// CHECK-NEXT: |           |--NumberLiteral `1` <col:5, col:6>
// CHECK-NEXT: |           |--NumberLiteral `2` <col:8, col:9>
// CHECK-NEXT: |           `--NumberLiteral `3` <col:11, col:12>
// CHECK-NEXT: `--FunctionDecl <col:1, col:47>
// CHECK-NEXT:    |--FunctionProto <col:1, col:27>
// CHECK-NEXT:    |  |--Identifier `a` <col:13, col:14>
// CHECK-NEXT:    |  `--ParamList <col:14, col:26>
// CHECK-NEXT:    |     |--ParamDecl `arg1` <col:15, col:19>
// CHECK-NEXT:    |     `--ParamDecl `arg2` <col:21, col:25>
// CHECK-NEXT:    `--BlockStmt <line:30, line:30>
// CHECK-NEXT:       `--ReturnStmt <line:30, col:3:21>
// CHECK-NEXT:          `--AddExpr <col:21, col:21>
// CHECK-NEXT:             |--Identifier `arg1` <col:10, col:14>
// CHECK-NEXT:             `--Identifier `arg2` <col:17, col:21>
// CHECK-NEXT: <STDIN>:27:3: error: too many arguments to 'a'
// CHECK-NEXT:   27 | ~ a(1, 2, 3)
// CHECK-NEXT:      |   ^

~ a(1, 2, 3)

== function a(arg1, arg2)
~ return arg1 + arg2
