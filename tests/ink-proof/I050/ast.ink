// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--AddExpr
// CHECK-NEXT:    |           +--ContainsExpr
// CHECK-NEXT:    |           |  +--StringExpr `"hello world"`
// CHECK-NEXT:    |           |  |  `--StringLiteral `hello world`
// CHECK-NEXT:    |           |  `--StringExpr `"o wo"`
// CHECK-NEXT:    |           |     `--StringLiteral `o wo`
// CHECK-NEXT:    |           `--NumberLiteral `0`
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--AddExpr
// CHECK-NEXT:    |           +--ContainsExpr
// CHECK-NEXT:    |           |  +--StringExpr `"hello world"`
// CHECK-NEXT:    |           |  |  `--StringLiteral `hello world`
// CHECK-NEXT:    |           |  `--StringExpr `"something else"`
// CHECK-NEXT:    |           |     `--StringLiteral `something else`
// CHECK-NEXT:    |           `--NumberLiteral `0`
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--AddExpr
// CHECK-NEXT:    |           +--ContainsExpr
// CHECK-NEXT:    |           |  +--StringExpr `"hello"`
// CHECK-NEXT:    |           |  |  `--StringLiteral `hello`
// CHECK-NEXT:    |           |  `--StringExpr `""`
// CHECK-NEXT:    |           `--NumberLiteral `0`
// CHECK-NEXT:    `--ContentStmt
// CHECK-NEXT:       `--ContentExpr
// CHECK-NEXT:          `--BraceExpr
// CHECK-NEXT:             `--AddExpr
// CHECK-NEXT:                +--ContainsExpr
// CHECK-NEXT:                |  +--StringExpr `""`
// CHECK-NEXT:                |  `--StringExpr `""`
// CHECK-NEXT:                `--NumberLiteral `0`

{("hello world" ? "o wo") + 0}
{("hello world" ? "something else") + 0}
{("hello" ? "") + 0}
{("" ? "") + 0}
