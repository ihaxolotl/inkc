// RUN: %ink-compiler --stdin --compile-only --dump-ast < %s | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:36, line:42>
// CHECK-NEXT:    |--VarDecl <line:36, col:1:10>
// CHECK-NEXT:    |  |--Identifier `x` <col:5, col:6>
// CHECK-NEXT:    |  `--NumberLiteral `1` <col:9, col:10>
// CHECK-NEXT:    `--ContentStmt <line:38, col:1:48>
// CHECK-NEXT:       `--Content <col:1, col:48>
// CHECK-NEXT:          `--MultiIfStmt <col:1, col:45>
// CHECK-NEXT:             |--IfBranch <col:3, col:11>
// CHECK-NEXT:             |  |--LogicalEqualityExpr <col:3, col:9>
// CHECK-NEXT:             |  |  |--Identifier `x` <col:3, col:4>
// CHECK-NEXT:             |  |  `--NumberLiteral `1` <col:8, col:9>
// CHECK-NEXT:             |  `--BlockStmt <line:39, line:39>
// CHECK-NEXT:             |     `--ContentStmt <line:39, col:11:14>
// CHECK-NEXT:             |        `--Content <col:11, col:14>
// CHECK-NEXT:             |           `--StringLiteral `One` <col:11, col:14>
// CHECK-NEXT:             |--IfBranch <col:3, col:11>
// CHECK-NEXT:             |  |--LogicalEqualityExpr <col:3, col:9>
// CHECK-NEXT:             |  |  |--Identifier `x` <col:3, col:4>
// CHECK-NEXT:             |  |  `--NumberLiteral `2` <col:8, col:9>
// CHECK-NEXT:             |  `--BlockStmt <line:40, line:40>
// CHECK-NEXT:             |     `--ContentStmt <line:40, col:11:14>
// CHECK-NEXT:             |        `--Content <col:11, col:14>
// CHECK-NEXT:             |           `--StringLiteral `Two` <col:11, col:14>
// CHECK-NEXT:             `--IfBranch <col:3, col:11>
// CHECK-NEXT:                |--LogicalEqualityExpr <col:3, col:9>
// CHECK-NEXT:                |  |--Identifier `x` <col:3, col:4>
// CHECK-NEXT:                |  `--NumberLiteral `3` <col:8, col:9>
// CHECK-NEXT:                `--BlockStmt <line:41, line:41>
// CHECK-NEXT:                   `--ContentStmt <line:41, col:11:16>
// CHECK-NEXT:                      `--Content <col:11, col:16>
// CHECK-NEXT:                         `--StringLiteral `Three` <col:11, col:16>

VAR x = 1

{
- x == 1: One
- x == 2: Two
- x == 3: Three
}
