// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:44, line:53>
// CHECK-NEXT:    |--ContentStmt <line:44, col:1:40>
// CHECK-NEXT:    |  `--Content <col:1, col:40>
// CHECK-NEXT:    |     `--InlineLogicExpr <col:1, col:40>
// CHECK-NEXT:    |        |--Identifier `x` <col:2, col:3>
// CHECK-NEXT:    |        `--ConditionalContent <col:1, col:35>
// CHECK-NEXT:    |           |--ConditionalBranch <col:7, col:10>
// CHECK-NEXT:    |           |  |--NumberLiteral `1` <col:7, col:8>
// CHECK-NEXT:    |           |  `--BlockStmt <line:45, line:45>
// CHECK-NEXT:    |           |     `--ExprStmt <line:45, col:12:17>
// CHECK-NEXT:    |           |        `--AddExpr <col:17, col:17>
// CHECK-NEXT:    |           |           |--NumberLiteral `1` <col:12, col:13>
// CHECK-NEXT:    |           |           `--NumberLiteral `1` <col:16, col:17>
// CHECK-NEXT:    |           `--ConditionalBranch <col:7, col:10>
// CHECK-NEXT:    |              |--NumberLiteral `2` <col:7, col:8>
// CHECK-NEXT:    |              `--BlockStmt <line:46, line:46>
// CHECK-NEXT:    |                 `--ExprStmt <line:46, col:12:17>
// CHECK-NEXT:    |                    `--AddExpr <col:17, col:17>
// CHECK-NEXT:    |                       |--NumberLiteral `2` <col:12, col:13>
// CHECK-NEXT:    |                       `--NumberLiteral `2` <col:16, col:17>
// CHECK-NEXT:    `--ContentStmt <line:48, col:1:56>
// CHECK-NEXT:       `--Content <col:1, col:56>
// CHECK-NEXT:          `--InlineLogicExpr <col:1, col:56>
// CHECK-NEXT:             |--Identifier `x` <col:2, col:3>
// CHECK-NEXT:             `--ConditionalContent <col:1, col:51>
// CHECK-NEXT:                |--ConditionalBranch <col:7, col:18>
// CHECK-NEXT:                |  |--NumberLiteral `1` <col:7, col:8>
// CHECK-NEXT:                |  `--BlockStmt <line:50, line:50>
// CHECK-NEXT:                |     `--ExprStmt <line:50, col:11:16>
// CHECK-NEXT:                |        `--AddExpr <col:16, col:16>
// CHECK-NEXT:                |           |--NumberLiteral `1` <col:11, col:12>
// CHECK-NEXT:                |           `--NumberLiteral `1` <col:15, col:16>
// CHECK-NEXT:                `--ConditionalBranch <col:7, col:18>
// CHECK-NEXT:                   |--NumberLiteral `2` <col:7, col:8>
// CHECK-NEXT:                   `--BlockStmt <line:52, line:52>
// CHECK-NEXT:                      `--ExprStmt <line:52, col:11:16>
// CHECK-NEXT:                         `--AddExpr <col:16, col:16>
// CHECK-NEXT:                            |--NumberLiteral `2` <col:11, col:12>
// CHECK-NEXT:                            `--NumberLiteral `2` <col:15, col:16>

{x:
    - 1: ~ 1 + 1
    - 2: ~ 2 + 2
}
{x:
    - 1:
        ~ 1 + 1
    - 2:
        ~ 2 + 2
}
