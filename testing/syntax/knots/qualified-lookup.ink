// RUN: %ink-compiler < %s --dump-ast --compile-only | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: |--BlockStmt <line:30, line:30>
// CHECK-NEXT: |  `--DivertStmt <line:30, col:1:16>
// CHECK-NEXT: |     `--Divert <col:1, col:16>
// CHECK-NEXT: |        `--CallExpr <col:4, col:16>
// CHECK-NEXT: |           |--SelectorExpr <col:4, col:7>
// CHECK-NEXT: |           |  |--Identifier `a` <col:4, col:5>
// CHECK-NEXT: |           |  `--Identifier `b` <col:6, col:7>
// CHECK-NEXT: |           `--ArgumentList <col:7, col:16>
// CHECK-NEXT: |              `--StringExpr `"Brett"` <col:8, col:15>
// CHECK-NEXT: |                 `--StringLiteral `Brett` <col:9, col:14>
// CHECK-NEXT: `--KnotDecl <line:32, line:34>
// CHECK-NEXT:    |--KnotProto <col:1, col:5>
// CHECK-NEXT:    |  `--Identifier `a` <col:4, col:5>
// CHECK-NEXT:    `--StitchDecl <line:33, line:34>
// CHECK-NEXT:       |--StitchProto <col:1, col:10>
// CHECK-NEXT:       |  |--Identifier `b` <col:3, col:4>
// CHECK-NEXT:       |  `--ParamList <col:4, col:10>
// CHECK-NEXT:       |     `--ParamDecl `name` <col:5, col:9>
// CHECK-NEXT:       `--BlockStmt <line:34, line:34>
// CHECK-NEXT:          `--ContentStmt <line:34, col:1:15>
// CHECK-NEXT:             `--Content <col:1, col:15>
// CHECK-NEXT:                |--StringLiteral `Hello, ` <col:1, col:8>
// CHECK-NEXT:                |--InlineLogicExpr <col:8, col:14>
// CHECK-NEXT:                |  `--Identifier `name` <col:9, col:13>
// CHECK-NEXT:                `--StringLiteral `!` <col:14, col:15>

-> a.b("Brett")

== a
= b(name)
Hello, {name}!
