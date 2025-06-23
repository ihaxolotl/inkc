// RUN: %ink-compiler < %s --stdin --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:12, line:13>
// CHECK-NEXT:    |--TempDecl <line:12, col:3:15>
// CHECK-NEXT:    |  |--Identifier `a` <col:8, col:9>
// CHECK-NEXT:    |  `--NumberLiteral `123` <col:12, col:15>
// CHECK-NEXT:    `--AssignStmt <line:13, col:3:10>
// CHECK-NEXT:       |--Identifier `a` <col:3, col:4>
// CHECK-NEXT:       `--NumberLiteral `321` <col:7, col:10>

~ temp a = 123
~ a = 321
