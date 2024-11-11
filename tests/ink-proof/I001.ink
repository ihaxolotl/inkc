// RUN: %ink-compiler < %s --dump-ast  | FileCheck %s

// CHECK: File(LeadingToken: String [0])
// CHECK-NEXT: --BlockStmt(LeadingToken: String [0])
// CHECK-NEXT: ----ContentStmt(LeadingToken: String [0])
// CHECK-NEXT: ------ContentExpr(LeadingToken: String [0])
// CHECK-NEXT: --------StringExpr(LeadingToken: String(`Hello, world!`) [0])

Hello, world!
