// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File(LeadingToken: KeywordVar [0])
// CHECK-NEXT: --BlockStmt(LeadingToken: KeywordVar [0])
// CHECK-NEXT: ----VarDecl(LeadingToken: KeywordVar [0])
// CHECK-NEXT: ------IdentifierExpr(LeadingToken: Identifier(`x`) [2])
// CHECK-NEXT: ------IdentifierExpr(LeadingToken: Identifier(`c`) [6])
// CHECK-NEXT: ----ConstDecl(LeadingToken: KeywordConst [8])
// CHECK-NEXT: ------IdentifierExpr(LeadingToken: Identifier(`c`) [10])
// CHECK-NEXT: ------NumberExpr(LeadingToken: Number(`5`) [14])
// CHECK-NEXT: ----ContentStmt(LeadingToken: LeftBrace [16])
// CHECK-NEXT: ------ContentExpr(LeadingToken: LeftBrace [16])
// CHECK-NEXT: --------BraceExpr(LeadingToken: LeftBrace [16])
// CHECK-NEXT: ----------IdentifierExpr(LeadingToken: Identifier(`x`) [17])

VAR x = c
CONST c = 5
{x}
