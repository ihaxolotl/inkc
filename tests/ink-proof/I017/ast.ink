// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:18, line:21>
// CHECK-NEXT:    |--ContentStmt <line:18, col:1:6>
// CHECK-NEXT:    |  `--Content <col:1, col:6>
// CHECK-NEXT:    |     `--StringLiteral `hello` <col:1, col:6>
// CHECK-NEXT:    |--DivertStmt <line:19, col:1:7>
// CHECK-NEXT:    |  `--Divert <col:1, col:7>
// CHECK-NEXT:    |     `--Identifier `END` <col:4, col:7>
// CHECK-NEXT:    |--ContentStmt <line:20, col:1:6>
// CHECK-NEXT:    |  `--Content <col:1, col:6>
// CHECK-NEXT:    |     `--StringLiteral `world` <col:1, col:6>
// CHECK-NEXT:    `--DivertStmt <line:21, col:1:7>
// CHECK-NEXT:       `--Divert <col:1, col:7>
// CHECK-NEXT:          `--Identifier `END` <col:4, col:7>

hello
-> END
world
-> END
