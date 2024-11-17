// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- CallExpr
// CHECK:     |           +-- Name `FLOOR`
// CHECK:     |           `-- ArgumentList
// CHECK:     |               `-- NumberLiteral `1.2`
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- CallExpr
// CHECK:     |           +-- Name `INT`
// CHECK:     |           `-- ArgumentList
// CHECK:     |               `-- NumberLiteral `1.2`
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- CallExpr
// CHECK:     |           +-- Name `CEILING`
// CHECK:     |           `-- ArgumentList
// CHECK:     |               `-- NumberLiteral `1.2`
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- DivideExpr
// CHECK:     |           +-- CallExpr
// CHECK:     |           |   +-- Name `CEILING`
// CHECK:     |           |   `-- ArgumentList
// CHECK:     |           |       `-- NumberLiteral `1.2`
// CHECK:     |           `-- NumberLiteral `3`
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- DivideExpr
// CHECK:     |           +-- CallExpr
// CHECK:     |           |   +-- Name `INT`
// CHECK:     |           |   `-- ArgumentList
// CHECK:     |           |       `-- CallExpr
// CHECK:     |           |           +-- Name `CEILING`
// CHECK:     |           |           `-- ArgumentList
// CHECK:     |           |               `-- NumberLiteral `1.2`
// CHECK:     |           `-- NumberLiteral `3`
// CHECK:     `-- ContentStmt
// CHECK:         `-- BraceExpr
// CHECK:             `-- CallExpr
// CHECK:                 +-- Name `FLOOR`
// CHECK:                 `-- ArgumentList
// CHECK:                     `-- NumberLiteral `1`

{FLOOR(1.2)}
{INT(1.2)}
{CEILING(1.2)}
{CEILING(1.2) / 3}
{INT(CEILING(1.2)) / 3}
{FLOOR(1)}
