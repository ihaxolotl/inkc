// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:19, line:23>
// CHECK-NEXT:    `--ContentStmt <line:19, col:1:52>
// CHECK-NEXT:       `--Content <col:1, col:52>
// CHECK-NEXT:          `--IfStmt <col:1, col:44>
// CHECK-NEXT:             |--True <col:2, col:6>
// CHECK-NEXT:             |--BlockStmt <line:20, line:20>
// CHECK-NEXT:             |  `--ContentStmt <line:20, col:5:18>
// CHECK-NEXT:             |     `--Content <col:5, col:18>
// CHECK-NEXT:             |        `--StringLiteral `Hello, world!` <col:5, col:18>
// CHECK-NEXT:             `--ElseBranch <col:3, col:13>
// CHECK-NEXT:                `--BlockStmt <line:22, line:22>
// CHECK-NEXT:                   `--ContentStmt <line:22, col:5:17>
// CHECK-NEXT:                      `--Content <col:5, col:17>
// CHECK-NEXT:                         `--StringLiteral `Unreachable!` <col:5, col:17>

{true:
    Hello, world!
- else:
    Unreachable!
}
