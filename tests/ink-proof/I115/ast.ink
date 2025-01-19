// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:19, line:23>
// CHECK-NEXT:    `--ContentStmt <line:19, col:1:37>
// CHECK-NEXT:       `--Content <col:1, col:37>
// CHECK-NEXT:          `--InlineLogicExpr <col:1, col:37>
// CHECK-NEXT:             |--NumberLiteral `3` <col:3, col:4>
// CHECK-NEXT:             `--ConditionalContent <col:1, col:31>
// CHECK-NEXT:                |--ConditionalBranch <col:7, col:9>
// CHECK-NEXT:                |  `--NumberLiteral `3` <col:7, col:8>
// CHECK-NEXT:                `--ConditionalBranch <col:7, col:9>
// CHECK-NEXT:                   |--NumberLiteral `4` <col:7, col:8>
// CHECK-NEXT:                   `--BlockStmt <line:22, line:23>
// CHECK-NEXT:                      `--ContentStmt <line:22, col:9:13>
// CHECK-NEXT:                         `--Content <col:9, col:12>
// CHECK-NEXT:                            `--StringLiteral `txt` <col:9, col:12>

{ 3:
    - 3:
    - 4:
        txt
}
