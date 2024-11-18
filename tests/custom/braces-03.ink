// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt
// CHECK-NEXT:    `--ContentStmt
// CHECK-NEXT:       `--ContentExpr
// CHECK-NEXT:          `--BraceExpr
// CHECK-NEXT:             `--SequenceExpr
// CHECK-NEXT:                +--ContentExpr
// CHECK-NEXT:                |  `--StringLiteral `1
// CHECK-NEXT:                `--ContentExpr

{1|}
