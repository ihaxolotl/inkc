// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File(LeadingToken: KeywordVar [0])
// CHECK-NEXT: --BlockStmt(LeadingToken: KeywordVar [0])
// CHECK-NEXT: ----VarDecl(LeadingToken: KeywordVar [0])
// CHECK-NEXT: ------IdentifierExpr(LeadingToken: Identifier(`x`) [2])
// CHECK-NEXT: ------StringExpr(LeadingToken: DoubleQuote(`"world"`) [6])
// CHECK-NEXT: --------StringLiteral(LeadingToken: String(`world`) [7])
// CHECK-NEXT: ----ContentStmt(LeadingToken: String [10])
// CHECK-NEXT: ------ContentExpr(LeadingToken: String [10])
// CHECK-NEXT: --------StringLiteral(LeadingToken: String(`Hello `) [10])
// CHECK-NEXT: --------BraceExpr(LeadingToken: LeftBrace [12])
// CHECK-NEXT: ----------IdentifierExpr(LeadingToken: Identifier(`x`) [13])
// CHECK-NEXT: --------StringLiteral(LeadingToken: String(`.`) [15])

VAR x = "world"
Hello {x}.
