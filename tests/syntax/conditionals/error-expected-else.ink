// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:23, line:27>
// CHECK-NEXT:    `--ContentStmt <line:23, col:1:47>
// CHECK-NEXT:       `--Content <col:1, col:47>
// CHECK-NEXT:          `--IfStmt <col:1, col:39>
// CHECK-NEXT:             |--True <col:2, col:6>
// CHECK-NEXT:             |--BlockStmt <line:24, line:24>
// CHECK-NEXT:             |  `--ContentStmt <line:24, col:5:18>
// CHECK-NEXT:             |     `--Content <col:5, col:18>
// CHECK-NEXT:             |        `--StringLiteral `Hello, world!` <col:5, col:18>
// CHECK-NEXT:             `--SwitchCase <col:3, col:14>
// CHECK-NEXT:                |--False <col:3, col:8>
// CHECK-NEXT:                `--BlockStmt <line:26, line:26>
// CHECK-NEXT:                   `--ContentStmt <line:26, col:5:11>
// CHECK-NEXT:                      `--Content <col:5, col:11>
// CHECK-NEXT:                         `--StringLiteral `False!` <col:5, col:11>
// CHECK-NEXT: <STDIN>:25:3: error: expected '- else:' clause rather than extra condition
// CHECK-NEXT:   25 | - false:
// CHECK-NEXT:      |   ^

{true:
    Hello, world!
- false:
    False!
}
