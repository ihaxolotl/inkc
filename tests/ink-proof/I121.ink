// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- AddExpr
// CHECK:     |           +-- MultiplyExpr
// CHECK:     |           |   +-- NumberLiteral `2 `
// CHECK:     |           |   `-- NumberLiteral `3 `
// CHECK:     |           `-- MultiplyExpr
// CHECK:     |               +-- NumberLiteral `5 `
// CHECK:     |               `-- NumberLiteral `6 `
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- ModExpr
// CHECK:     |           +-- NumberLiteral `8 `
// CHECK:     |           `-- NumberLiteral `3`
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- ModExpr
// CHECK:     |           +-- NumberLiteral `13 `
// CHECK:     |           `-- NumberLiteral `5`
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- DivideExpr
// CHECK:     |           +-- NumberLiteral `7 `
// CHECK:     |           `-- NumberLiteral `3 `
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- DivideExpr
// CHECK:     |           +-- NumberLiteral `5 `
// CHECK:     |           `-- NumberLiteral `2.0 `
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- SubtractExpr
// CHECK:     |           +-- NumberLiteral `10 `
// CHECK:     |           `-- NumberLiteral `2 `
// CHECK:     `-- ContentStmt
// CHECK:         `-- BraceExpr
// CHECK:             `-- MultiplyExpr
// CHECK:                 +-- NumberLiteral `2 `
// CHECK:                 `-- SubtractExpr
// CHECK:                     +-- NumberLiteral `5`
// CHECK:                     `-- NumberLiteral `1`

{ 2 * 3 + 5 * 6 }
{8 mod 3}
{13 % 5}
{ 7 / 3 }
{ 5 / 2.0 }
{ 10 - 2 }
{ 2 * (5-1) }
