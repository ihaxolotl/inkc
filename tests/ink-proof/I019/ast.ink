// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--KnotDecl <line:15, line:17>
// CHECK-NEXT:    |--KnotProto <col:1, col:12>
// CHECK-NEXT:    |  `--Identifier `test` <col:4, col:8>
// CHECK-NEXT:    `--BlockStmt <line:16, line:17>
// CHECK-NEXT:       |--ContentStmt <line:16, col:1:9>
// CHECK-NEXT:       |  `--Content <col:1, col:8>
// CHECK-NEXT:       |     `--StringLiteral `Content` <col:1, col:8>
// CHECK-NEXT:       `--DivertStmt <line:17, col:1:7>
// CHECK-NEXT:          `--Divert <col:1, col:7>
// CHECK-NEXT:             `--Identifier `END` <col:4, col:7>

== test ==
Content
-> END
