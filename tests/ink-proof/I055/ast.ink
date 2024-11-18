// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: +--BlockStmt
// CHECK-NEXT: |  `--DivertStmt
// CHECK-NEXT: |     `--DivertExpr
// CHECK-NEXT: |        `--Name `hurry_home`
// CHECK-NEXT: +--KnotDecl
// CHECK-NEXT: |  +--KnotPrototype
// CHECK-NEXT: |  |  `--Name `hurry_home `
// CHECK-NEXT: |  `--BlockStmt
// CHECK-NEXT: |     `--ContentStmt
// CHECK-NEXT: |        `--ContentExpr
// CHECK-NEXT: |           +--StringLiteral `We hurried home to Savile Row `
// CHECK-NEXT: |           `--DivertExpr
// CHECK-NEXT: |              `--Name `as_fast_as_we_could`
// CHECK-NEXT: `--KnotDecl
// CHECK-NEXT:    +--KnotPrototype
// CHECK-NEXT:    |  `--Name `as_fast_as_we_could `
// CHECK-NEXT:    `--BlockStmt
// CHECK-NEXT:       +--ContentStmt
// CHECK-NEXT:       |  `--ContentExpr
// CHECK-NEXT:       |     `--StringLiteral `as fast as we could.`
// CHECK-NEXT:       `--DivertStmt
// CHECK-NEXT:          `--DivertExpr
// CHECK-NEXT:             `--Name `DONE`

-> hurry_home
=== hurry_home ===
We hurried home to Savile Row -> as_fast_as_we_could
=== as_fast_as_we_could ===
as fast as we could.
-> DONE
