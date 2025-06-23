// RUN: %ink-compiler --stdin --compile-only --dump-ast < %s | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:28, line:35>
// CHECK-NEXT:    `--ContentStmt <line:28, col:1:57>
// CHECK-NEXT:       `--Content <col:1, col:57>
// CHECK-NEXT:          `--MultiIfStmt <col:1, col:54>
// CHECK-NEXT:             |--IfBranch <col:3, col:13>
// CHECK-NEXT:             |  |--True <col:3, col:7>
// CHECK-NEXT:             |  `--BlockStmt <line:30, line:30>
// CHECK-NEXT:             |     `--ContentStmt <line:30, col:5:9>
// CHECK-NEXT:             |        `--Content <col:5, col:9>
// CHECK-NEXT:             |           `--StringLiteral `True` <col:5, col:9>
// CHECK-NEXT:             |--ElseBranch <col:3, col:13>
// CHECK-NEXT:             |  `--BlockStmt <line:32, line:32>
// CHECK-NEXT:             |     `--ContentStmt <line:32, col:5:10>
// CHECK-NEXT:             |        `--Content <col:5, col:10>
// CHECK-NEXT:             |           `--StringLiteral `False` <col:5, col:10>
// CHECK-NEXT:             `--ElseBranch <col:3, col:13>
// CHECK-NEXT:                `--BlockStmt <line:34, line:34>
// CHECK-NEXT:                   `--ContentStmt <line:34, col:5:10>
// CHECK-NEXT:                      `--Content <col:5, col:10>
// CHECK-NEXT:                         `--StringLiteral `False` <col:5, col:10>
// CHECK-NEXT: <STDIN>:31:3: error: 'else' case should always be the final case in conditional
// CHECK-NEXT:   31 | - else:
// CHECK-NEXT:      |   ^

{
- true:
    True
- else:
    False
- else:
    False
}
