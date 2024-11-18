// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--AddExpr
// CHECK-NEXT:    |           +--MultiplyExpr
// CHECK-NEXT:    |           |  +--NumberLiteral `2 `
// CHECK-NEXT:    |           |  `--NumberLiteral `3 `
// CHECK-NEXT:    |           `--MultiplyExpr
// CHECK-NEXT:    |              +--NumberLiteral `5 `
// CHECK-NEXT:    |              `--NumberLiteral `6 `
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--ModExpr
// CHECK-NEXT:    |           +--NumberLiteral `8 `
// CHECK-NEXT:    |           `--NumberLiteral `3`
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--ModExpr
// CHECK-NEXT:    |           +--NumberLiteral `13 `
// CHECK-NEXT:    |           `--NumberLiteral `5`
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--DivideExpr
// CHECK-NEXT:    |           +--NumberLiteral `7 `
// CHECK-NEXT:    |           `--NumberLiteral `3 `
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--DivideExpr
// CHECK-NEXT:    |           +--NumberLiteral `5 `
// CHECK-NEXT:    |           `--NumberLiteral `2.0 `
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--SubtractExpr
// CHECK-NEXT:    |           +--NumberLiteral `10 `
// CHECK-NEXT:    |           `--NumberLiteral `2 `
// CHECK-NEXT:    `--ContentStmt
// CHECK-NEXT:       `--ContentExpr
// CHECK-NEXT:          `--BraceExpr
// CHECK-NEXT:             `--MultiplyExpr
// CHECK-NEXT:                +--NumberLiteral `2 `
// CHECK-NEXT:                `--SubtractExpr
// CHECK-NEXT:                   +--NumberLiteral `5`
// CHECK-NEXT:                   `--NumberLiteral `1`

{ 2 * 3 + 5 * 6 }
{8 mod 3}
{13 % 5}
{ 7 / 3 }
{ 5 / 2.0 }
{ 10 - 2 }
{ 2 * (5-1) }
