// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: |--BlockStmt <line:25, line:25>
// CHECK-NEXT: |  `--DivertStmt <line:25, col:1:8>
// CHECK-NEXT: |     `--Divert <col:1, col:8>
// CHECK-NEXT: |        `--Identifier `test` <col:4, col:8>
// CHECK-NEXT: `--KnotDecl <line:26, line:30>
// CHECK-NEXT:    |--KnotProto <col:1, col:12>
// CHECK-NEXT:    |  `--Identifier `test` <col:4, col:8>
// CHECK-NEXT:    `--BlockStmt <line:27, line:30>
// CHECK-NEXT:       |--ContentStmt <line:27, col:1:6>
// CHECK-NEXT:       |  `--Content <col:1, col:6>
// CHECK-NEXT:       |     `--StringLiteral `hello` <col:1, col:6>
// CHECK-NEXT:       |--DivertStmt <line:28, col:1:7>
// CHECK-NEXT:       |  `--Divert <col:1, col:7>
// CHECK-NEXT:       |     `--Identifier `END` <col:4, col:7>
// CHECK-NEXT:       |--ContentStmt <line:29, col:1:6>
// CHECK-NEXT:       |  `--Content <col:1, col:6>
// CHECK-NEXT:       |     `--StringLiteral `world` <col:1, col:6>
// CHECK-NEXT:       `--DivertStmt <line:30, col:1:7>
// CHECK-NEXT:          `--Divert <col:1, col:7>
// CHECK-NEXT:             `--Identifier `END` <col:4, col:7>

-> test
== test ==
hello
-> END
world
-> END
