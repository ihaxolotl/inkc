// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File
// CHECK-NEXT: --BlockStmt
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------AddExpr
// CHECK-NEXT: ------------ContainsDecl
// CHECK-NEXT: --------------StringExpr `"hello world"`
// CHECK-NEXT: ----------------StringLiteral `hello world`
// CHECK-NEXT: --------------StringExpr `"o wo"`
// CHECK-NEXT: ----------------StringLiteral `o wo`
// CHECK-NEXT: ------------NumberExpr `0`
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------AddExpr
// CHECK-NEXT: ------------ContainsDecl
// CHECK-NEXT: --------------StringExpr `"hello world"`
// CHECK-NEXT: ----------------StringLiteral `hello world`
// CHECK-NEXT: --------------StringExpr `"something else"`
// CHECK-NEXT: ----------------StringLiteral `something else`
// CHECK-NEXT: ------------NumberExpr `0`
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------AddExpr
// CHECK-NEXT: ------------ContainsDecl
// CHECK-NEXT: --------------StringExpr `"hello"`
// CHECK-NEXT: ----------------StringLiteral `hello`
// CHECK-NEXT: --------------StringExpr `""`
// CHECK-NEXT: ------------NumberExpr `0`
// CHECK-NEXT: ----ContentStmt
// CHECK-NEXT: ------ContentExpr
// CHECK-NEXT: --------BraceExpr
// CHECK-NEXT: ----------AddExpr
// CHECK-NEXT: ------------ContainsDecl
// CHECK-NEXT: --------------StringExpr `""`
// CHECK-NEXT: --------------StringExpr `""`
// CHECK-NEXT: ------------NumberExpr `0`

{("hello world" ? "o wo") + 0}
{("hello world" ? "something else") + 0}
{("hello" ? "") + 0}
{("" ? "") + 0}
