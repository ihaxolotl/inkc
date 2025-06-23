// RUN: %ink-compiler --stdin --compile-only --dump-ast < %s | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:14, line:16>
// CHECK-NEXT:    `--ContentStmt <line:14, col:1:27>
// CHECK-NEXT:       `--Content <col:1, col:27>
// CHECK-NEXT:          `--IfStmt <col:1, col:19>
// CHECK-NEXT:             |--True <col:2, col:6>
// CHECK-NEXT:             `--BlockStmt <line:15, line:15>
// CHECK-NEXT:                `--ContentStmt <line:15, col:5:18>
// CHECK-NEXT:                   `--Content <col:5, col:18>
// CHECK-NEXT:                      `--StringLiteral `Hello, world!` <col:5, col:18>

{true:
    Hello, world!
}
