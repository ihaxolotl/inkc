// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--StringLiteral `hello`
// CHECK-NEXT:    +--DivertStmt
// CHECK-NEXT:    |  `--DivertExpr
// CHECK-NEXT:    |     `--Name `END`
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--StringLiteral `world`
// CHECK-NEXT:    `--DivertStmt
// CHECK-NEXT:       `--DivertExpr
// CHECK-NEXT:          `--Name `END`

hello
-> END
world
-> END
