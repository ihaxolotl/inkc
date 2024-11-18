// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- AddExpr
// CHECK:     |           +-- ContainsExpr
// CHECK:     |           |   +-- StringExpr `"hello world"`
// CHECK:     |           |   |   `-- StringLiteral `hello world`
// CHECK:     |           |   `-- StringExpr `"o wo"`
// CHECK:     |           |       `-- StringLiteral `o wo`
// CHECK:     |           `-- NumberLiteral `0`
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- AddExpr
// CHECK:     |           +-- ContainsExpr
// CHECK:     |           |   +-- StringExpr `"hello world"`
// CHECK:     |           |   |   `-- StringLiteral `hello world`
// CHECK:     |           |   `-- StringExpr `"something else"`
// CHECK:     |           |       `-- StringLiteral `something else`
// CHECK:     |           `-- NumberLiteral `0`
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- BraceExpr
// CHECK:     |       `-- AddExpr
// CHECK:     |           +-- ContainsExpr
// CHECK:     |           |   +-- StringExpr `"hello"`
// CHECK:     |           |   |   `-- StringLiteral `hello`
// CHECK:     |           |   `-- StringExpr `""`
// CHECK:     |           `-- NumberLiteral `0`
// CHECK:     `-- ContentStmt
// CHECK:         `-- BraceExpr
// CHECK:             `-- AddExpr
// CHECK:                 +-- ContainsExpr
// CHECK:                 |   +-- StringExpr `""`
// CHECK:                 |   `-- StringExpr `""`
// CHECK:                 `-- NumberLiteral `0`

{("hello world" ? "o wo") + 0}
{("hello world" ? "something else") + 0}
{("hello" ? "") + 0}
{("" ? "") + 0}
