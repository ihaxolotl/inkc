// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--CallExpr
// CHECK-NEXT:    |           +--Name `FLOOR`
// CHECK-NEXT:    |           `--ArgumentList
// CHECK-NEXT:    |              `--NumberLiteral `1.2`
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--CallExpr
// CHECK-NEXT:    |           +--Name `INT`
// CHECK-NEXT:    |           `--ArgumentList
// CHECK-NEXT:    |              `--NumberLiteral `1.2`
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--CallExpr
// CHECK-NEXT:    |           +--Name `CEILING`
// CHECK-NEXT:    |           `--ArgumentList
// CHECK-NEXT:    |              `--NumberLiteral `1.2`
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--DivideExpr
// CHECK-NEXT:    |           +--CallExpr
// CHECK-NEXT:    |           |  +--Name `CEILING`
// CHECK-NEXT:    |           |  `--ArgumentList
// CHECK-NEXT:    |           |     `--NumberLiteral `1.2`
// CHECK-NEXT:    |           `--NumberLiteral `3`
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--DivideExpr
// CHECK-NEXT:    |           +--CallExpr
// CHECK-NEXT:    |           |  +--Name `INT`
// CHECK-NEXT:    |           |  `--ArgumentList
// CHECK-NEXT:    |           |     `--CallExpr
// CHECK-NEXT:    |           |        +--Name `CEILING`
// CHECK-NEXT:    |           |        `--ArgumentList
// CHECK-NEXT:    |           |           `--NumberLiteral `1.2`
// CHECK-NEXT:    |           `--NumberLiteral `3`
// CHECK-NEXT:    `--ContentStmt
// CHECK-NEXT:       `--ContentExpr
// CHECK-NEXT:          `--BraceExpr
// CHECK-NEXT:             `--CallExpr
// CHECK-NEXT:                +--Name `FLOOR`
// CHECK-NEXT:                `--ArgumentList
// CHECK-NEXT:                   `--NumberLiteral `1`

{FLOOR(1.2)}
{INT(1.2)}
{CEILING(1.2)}
{CEILING(1.2) / 3}
{INT(CEILING(1.2)) / 3}
{FLOOR(1)}
