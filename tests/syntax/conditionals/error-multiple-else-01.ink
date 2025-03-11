// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:27, line:33>
// CHECK-NEXT:    `--ContentStmt <line:27, col:1:43>
// CHECK-NEXT:       `--Content <col:1, col:43>
// CHECK-NEXT:          `--IfStmt <col:1, col:35>
// CHECK-NEXT:             |--True <col:2, col:6>
// CHECK-NEXT:             |--BlockStmt <line:28, line:28>
// CHECK-NEXT:             |  `--ContentStmt <line:28, col:5:6>
// CHECK-NEXT:             |     `--Content <col:5, col:6>
// CHECK-NEXT:             |        `--StringLiteral `A` <col:5, col:6>
// CHECK-NEXT:             |--ElseBranch <col:3, col:13>
// CHECK-NEXT:             |  `--BlockStmt <line:30, line:30>
// CHECK-NEXT:             |     `--ContentStmt <line:30, col:5:6>
// CHECK-NEXT:             |        `--Content <col:5, col:6>
// CHECK-NEXT:             |           `--StringLiteral `B` <col:5, col:6>
// CHECK-NEXT:             `--ElseBranch <col:3, col:13>
// CHECK-NEXT:                `--BlockStmt <line:32, line:32>
// CHECK-NEXT:                   `--ContentStmt <line:32, col:5:6>
// CHECK-NEXT:                      `--Content <col:5, col:6>
// CHECK-NEXT:                         `--StringLiteral `C` <col:5, col:6>
// CHECK-NEXT: <STDIN>:29:3: error: 'else' case should always be the final case in conditional
// CHECK-NEXT:   29 | - else:
// CHECK-NEXT:      |

{true:
    A
- else:
    B
- else:
    C
}
