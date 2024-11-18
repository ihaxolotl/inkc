// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: +-- BlockStmt
// CHECK: |   `-- DivertStmt
// CHECK: |       `-- DivertExpr
// CHECK: |           `-- Name `hurry_home`
// CHECK: +-- KnotDecl
// CHECK: |   +-- KnotPrototype
// CHECK: |   |   `-- Name `hurry_home `
// CHECK: |   `-- BlockStmt
// CHECK: |       `-- ContentStmt
// CHECK: |           +-- StringLiteral `We hurried home to Savile Row `
// CHECK: |           `-- DivertExpr
// CHECK: |               `-- Name `as_fast_as_we_could`
// CHECK: `-- KnotDecl
// CHECK:     +-- KnotPrototype
// CHECK:     |   `-- Name `as_fast_as_we_could `
// CHECK:     `-- BlockStmt
// CHECK:         +-- ContentStmt
// CHECK:         |   `-- StringLiteral `as fast as we could.`
// CHECK:         `-- DivertStmt
// CHECK:             `-- DivertExpr
// CHECK:                 `-- Name `DONE`

-> hurry_home
=== hurry_home ===
We hurried home to Savile Row -> as_fast_as_we_could
=== as_fast_as_we_could ===
as fast as we could.
-> DONE
