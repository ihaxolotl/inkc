// RUN: %ink-compiler < %s --stdin --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:23, line:27>
// CHECK-NEXT:    |--TempDecl <line:23, col:3:13>
// CHECK-NEXT:    |  |--Identifier `x` <col:8, col:9>
// CHECK-NEXT:    |  `--NumberLiteral `2` <col:12, col:13>
// CHECK-NEXT:    `--ContentStmt <line:25, col:1:48>
// CHECK-NEXT:       `--Content <col:1, col:48>
// CHECK-NEXT:          `--IfStmt <col:1, col:26>
// CHECK-NEXT:             |--AndExpr <col:2, col:20>
// CHECK-NEXT:             |  |--LogicalGreaterOrEqualExpr <col:2, col:8>
// CHECK-NEXT:             |  |  |--Identifier `x` <col:2, col:3>
// CHECK-NEXT:             |  |  `--NumberLiteral `1` <col:7, col:8>
// CHECK-NEXT:             |  `--LogicalLesserOrEqualExpr <col:13, col:20>
// CHECK-NEXT:             |     |--Identifier `x` <col:13, col:14>
// CHECK-NEXT:             |     `--NumberLiteral `10` <col:18, col:20>
// CHECK-NEXT:             `--BlockStmt <line:26, line:26>
// CHECK-NEXT:                `--ContentStmt <line:26, col:5:25>
// CHECK-NEXT:                   `--Content <col:5, col:25>
// CHECK-NEXT:                      `--StringLiteral `Between one and ten!` <col:5, col:25>

~ temp x = 2

{x >= 1 and x <= 10:
    Between one and ten!
}
