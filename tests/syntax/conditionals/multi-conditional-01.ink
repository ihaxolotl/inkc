// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:41, line:47>
// CHECK-NEXT:    |--TempDecl <line:41, col:3:15>
// CHECK-NEXT:    |  |--Identifier `foo` <col:8, col:11>
// CHECK-NEXT:    |  `--NumberLiteral `3` <col:14, col:15>
// CHECK-NEXT:    `--ContentStmt <line:42, col:1:97>
// CHECK-NEXT:       `--Content <col:1, col:97>
// CHECK-NEXT:          `--MultiIfStmt <col:5, col:94>
// CHECK-NEXT:             |--IfBranch <col:7, col:17>
// CHECK-NEXT:             |  |--LogicalEqualityExpr <col:7, col:15>
// CHECK-NEXT:             |  |  |--Identifier `foo` <col:7, col:10>
// CHECK-NEXT:             |  |  `--NumberLiteral `1` <col:14, col:15>
// CHECK-NEXT:             |  `--BlockStmt <line:43, line:43>
// CHECK-NEXT:             |     `--ContentStmt <line:43, col:17:21>
// CHECK-NEXT:             |        `--Content <col:17, col:21>
// CHECK-NEXT:             |           `--StringLiteral `One!` <col:17, col:21>
// CHECK-NEXT:             |--IfBranch <col:7, col:17>
// CHECK-NEXT:             |  |--LogicalEqualityExpr <col:7, col:15>
// CHECK-NEXT:             |  |  |--Identifier `foo` <col:7, col:10>
// CHECK-NEXT:             |  |  `--NumberLiteral `2` <col:14, col:15>
// CHECK-NEXT:             |  `--BlockStmt <line:44, line:44>
// CHECK-NEXT:             |     `--ContentStmt <line:44, col:17:21>
// CHECK-NEXT:             |        `--Content <col:17, col:21>
// CHECK-NEXT:             |           `--StringLiteral `Two!` <col:17, col:21>
// CHECK-NEXT:             |--IfBranch <col:7, col:17>
// CHECK-NEXT:             |  |--LogicalEqualityExpr <col:7, col:15>
// CHECK-NEXT:             |  |  |--Identifier `foo` <col:7, col:10>
// CHECK-NEXT:             |  |  `--NumberLiteral `3` <col:14, col:15>
// CHECK-NEXT:             |  `--BlockStmt <line:45, line:45>
// CHECK-NEXT:             |     `--ContentStmt <line:45, col:17:23>
// CHECK-NEXT:             |        `--Content <col:17, col:23>
// CHECK-NEXT:             |           `--StringLiteral `Three!` <col:17, col:23>
// CHECK-NEXT:             `--ElseBranch <col:7, col:13>
// CHECK-NEXT:                `--BlockStmt <line:46, line:46>
// CHECK-NEXT:                   `--ContentStmt <line:46, col:13:28>
// CHECK-NEXT:                      `--Content <col:13, col:28>
// CHECK-NEXT:                         `--StringLiteral `Something else!` <col:13, col:28>

~ temp foo = 3
{
    - foo == 1: One!
    - foo == 2: Two!
    - foo == 3: Three!
    - else: Something else!
}
