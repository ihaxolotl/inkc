// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: +--BlockStmt
// CHECK-NEXT: |  `--DivertStmt
// CHECK-NEXT: |     `--DivertExpr
// CHECK-NEXT: |        `--Name `2tests`
// CHECK-NEXT: `--KnotDecl
// CHECK-NEXT:    +--Name `2tests `
// CHECK-NEXT:    `--BlockStmt
// CHECK-NEXT:       +--TempStmt
// CHECK-NEXT:       |  +--Name `512x2 `
// CHECK-NEXT:       |  `--MultiplyExpr
// CHECK-NEXT:       |     +--NumberLiteral `512 `
// CHECK-NEXT:       |     `--NumberLiteral `2`
// CHECK-NEXT:       +--TempStmt
// CHECK-NEXT:       |  +--Name `512x2p2 `
// CHECK-NEXT:       |  `--AddExpr
// CHECK-NEXT:       |     +--Name `512x2 `
// CHECK-NEXT:       |     `--NumberLiteral `2`
// CHECK-NEXT:       +--ContentStmt
// CHECK-NEXT:       |  `--ContentExpr
// CHECK-NEXT:       |     +--StringLiteral `512x2 = `
// CHECK-NEXT:       |     `--BraceExpr
// CHECK-NEXT:       |        `--Name `512x2`
// CHECK-NEXT:       +--ContentStmt
// CHECK-NEXT:       |  `--ContentExpr
// CHECK-NEXT:       |     +--StringLiteral `512x2p2 = `
// CHECK-NEXT:       |     `--BraceExpr
// CHECK-NEXT:       |        `--Name `512x2p2`
// CHECK-NEXT:       `--DivertStmt
// CHECK-NEXT:          `--DivertExpr
// CHECK-NEXT:             `--Name `DONE`

-> 2tests
== 2tests ==
~ temp 512x2 = 512 * 2
~ temp 512x2p2 = 512x2 + 2
512x2 = {512x2}
512x2p2 = {512x2p2}
-> DONE
