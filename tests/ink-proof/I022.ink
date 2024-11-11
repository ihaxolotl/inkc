// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File(LeadingToken: String [0])
// CHECK-NEXT: --BlockStmt(LeadingToken: String [0])
// CHECK-NEXT: ----ContentStmt(LeadingToken: String [0])
// CHECK-NEXT: ------ContentExpr(LeadingToken: String [0])
// CHECK-NEXT: --------StringLiteral(LeadingToken: String(`My name is "`) [0])
// CHECK-NEXT: --------BraceExpr(LeadingToken: LeftBrace [7])
// CHECK-NEXT: ----------StringExpr(LeadingToken: DoubleQuote(`"J{"o"}e"`) [8])
// CHECK-NEXT: ------------StringLiteral(LeadingToken: String(`J`) [9])
// CHECK-NEXT: ------------BraceExpr(LeadingToken: LeftBrace [10])
// CHECK-NEXT: --------------StringExpr(LeadingToken: DoubleQuote(`"o"`) [11])
// CHECK-NEXT: ----------------StringLiteral(LeadingToken: String(`o`) [12])
// CHECK-NEXT: ------------StringLiteral(LeadingToken: String(`e`) [15])
// CHECK-NEXT: --------StringLiteral(LeadingToken: DoubleQuote(`"`) [18])

My name is "{"J{"o"}e"}"
