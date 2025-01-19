// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt <line:23, line:26>
// CHECK-NEXT:    `--ContentStmt <line:23, col:1:29>
// CHECK-NEXT:       `--Content <col:1, col:29>
// CHECK-NEXT:          `--InlineLogicExpr <col:1, col:29>
// CHECK-NEXT:             |--NumberLiteral `1` <col:3, col:4>
// CHECK-NEXT:             `--ConditionalContent <col:1, col:23>
// CHECK-NEXT:                |--ConditionalBranch <col:7, col:10>
// CHECK-NEXT:                |  |--NumberLiteral `2` <col:7, col:8>
// CHECK-NEXT:                |  `--BlockStmt <line:24, line:25>
// CHECK-NEXT:                |     `--ContentStmt <line:24, col:10:12>
// CHECK-NEXT:                |        `--Content <col:10, col:11>
// CHECK-NEXT:                |           `--StringLiteral `x` <col:10, col:11>
// CHECK-NEXT:                `--ConditionalBranch <col:7, col:10>
// CHECK-NEXT:                   |--NumberLiteral `3` <col:7, col:8>
// CHECK-NEXT:                   `--BlockStmt <line:25, line:26>
// CHECK-NEXT:                      `--ContentStmt <line:25, col:10:12>
// CHECK-NEXT:                         `--Content <col:10, col:11>
// CHECK-NEXT:                            `--StringLiteral `y` <col:10, col:11>

{ 1:
    - 2: x
    - 3: y
}
