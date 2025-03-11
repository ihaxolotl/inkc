// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:26, line:31>
// CHECK-NEXT:    `--ContentStmt <line:26, col:1:48>
// CHECK-NEXT:       `--Content <col:1, col:48>
// CHECK-NEXT:          `--SwitchStmt <col:1, col:40>
// CHECK-NEXT:             |--True <col:2, col:6>
// CHECK-NEXT:             |--ElseBranch <col:3, col:13>
// CHECK-NEXT:             |  `--BlockStmt <line:28, line:28>
// CHECK-NEXT:             |     `--ContentStmt <line:28, col:5:10>
// CHECK-NEXT:             |        `--Content <col:5, col:10>
// CHECK-NEXT:             |           `--StringLiteral `False` <col:5, col:10>
// CHECK-NEXT:             `--SwitchCase <col:3, col:15>
// CHECK-NEXT:                |--LogicalEqualityExpr <col:3, col:9>
// CHECK-NEXT:                |  |--NumberLiteral `1` <col:3, col:4>
// CHECK-NEXT:                |  `--NumberLiteral `1` <col:8, col:9>
// CHECK-NEXT:                `--BlockStmt <line:30, line:30>
// CHECK-NEXT:                   `--ContentStmt <line:30, col:5:11>
// CHECK-NEXT:                      `--Content <col:5, col:11>
// CHECK-NEXT:                         `--StringLiteral `Woops!` <col:5, col:11>
// CHECK-NEXT: <STDIN>:27:3: error: 'else' case should always be the final case in conditional
// CHECK-NEXT:   27 | - else:
// CHECK-NEXT:      |   ^

{true:
- else:
    False
- 1 == 1:
    Woops!
}
