// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:15, line:16>
// CHECK-NEXT:    |--ConstDecl <line:15, col:1:14>
// CHECK-NEXT:    |  |--Identifier `c` <col:7, col:8>
// CHECK-NEXT:    |  `--NumberLiteral `123` <col:11, col:14>
// CHECK-NEXT:    `--AssignStmt <line:16, col:3:8>
// CHECK-NEXT:       |--Identifier `c` <col:3, col:4>
// CHECK-NEXT:       `--NumberLiteral `0` <col:7, col:8>
// CHECK-NEXT: <STDIN>:16:3: error: attempt to modify constant value
// CHECK-NEXT:   16 | ~ c = 0
// CHECK-NEXT:      |

CONST c = 123
~ c = 0
