// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt
// CHECK-NEXT:    +--VarDecl
// CHECK-NEXT:    |  +--Name `x `
// CHECK-NEXT:    |  `--StringExpr `"Hello world 1"`
// CHECK-NEXT:    |     `--StringLiteral `Hello world 1`
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--BraceExpr
// CHECK-NEXT:    |        `--Name `x`
// CHECK-NEXT:    `--ContentStmt
// CHECK-NEXT:       `--ContentExpr
// CHECK-NEXT:          +--StringLiteral `Hello `
// CHECK-NEXT:          +--BraceExpr
// CHECK-NEXT:          |  `--StringExpr `"world"`
// CHECK-NEXT:          |     `--StringLiteral `world`
// CHECK-NEXT:          `--StringLiteral ` 2.`

VAR x = "Hello world 1"
{x}
Hello {"world"} 2.
