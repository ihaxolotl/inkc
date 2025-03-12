// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:23, line:27>
// CHECK-NEXT:    |--TempDecl <line:23, col:3:13>
// CHECK-NEXT:    |  |--Identifier `x` <col:8, col:9>
// CHECK-NEXT:    |  `--NumberLiteral `2` <col:12, col:13>
// CHECK-NEXT:    `--ContentStmt <line:25, col:1:56>
// CHECK-NEXT:       `--Content <col:1, col:56>
// CHECK-NEXT:          `--IfStmt <col:1, col:32>
// CHECK-NEXT:             |--OrExpr <col:2, col:22>
// CHECK-NEXT:             |  |--LogicalGreaterOrEqualExpr <col:2, col:8>
// CHECK-NEXT:             |  |  |--Identifier `x` <col:2, col:3>
// CHECK-NEXT:             |  |  `--NumberLiteral `1` <col:7, col:8>
// CHECK-NEXT:             |  `--LogicalEqualityExpr <col:12, col:22>
// CHECK-NEXT:             |     |--Identifier `x` <col:12, col:13>
// CHECK-NEXT:             |     `--False <col:17, col:22>
// CHECK-NEXT:             `--BlockStmt <line:26, line:26>
// CHECK-NEXT:                `--ContentStmt <line:26, col:5:31>
// CHECK-NEXT:                   `--Content <col:5, col:31>
// CHECK-NEXT:                      `--StringLiteral `Greater than one or false!` <col:5, col:31>

~ temp x = 2

{x >= 1 or x == false:
    Greater than one or false!
}
