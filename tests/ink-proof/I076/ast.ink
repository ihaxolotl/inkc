// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: +--BlockStmt
// CHECK-NEXT: |  +--ContentStmt
// CHECK-NEXT: |  |  `--ContentExpr
// CHECK-NEXT: |  |     `--BraceExpr
// CHECK-NEXT: |  |        `--AddExpr
// CHECK-NEXT: |  |           +--CallExpr
// CHECK-NEXT: |  |           |  +--Name `six`
// CHECK-NEXT: |  |           |  `--ArgumentList
// CHECK-NEXT: |  |           `--CallExpr
// CHECK-NEXT: |  |              +--Name `two`
// CHECK-NEXT: |  |              `--ArgumentList
// CHECK-NEXT: |  `--DivertStmt
// CHECK-NEXT: |     `--DivertExpr
// CHECK-NEXT: |        `--Name `END`
// CHECK-NEXT: +--FunctionDecl
// CHECK-NEXT: |  +--Name `six`
// CHECK-NEXT: |  `--BlockStmt
// CHECK-NEXT: |     `--ReturnStmt
// CHECK-NEXT: |        `--AddExpr
// CHECK-NEXT: |           +--CallExpr
// CHECK-NEXT: |           |  +--Name `four`
// CHECK-NEXT: |           |  `--ArgumentList
// CHECK-NEXT: |           `--CallExpr
// CHECK-NEXT: |              +--Name `two`
// CHECK-NEXT: |              `--ArgumentList
// CHECK-NEXT: +--FunctionDecl
// CHECK-NEXT: |  +--Name `four`
// CHECK-NEXT: |  `--BlockStmt
// CHECK-NEXT: |     `--ReturnStmt
// CHECK-NEXT: |        `--AddExpr
// CHECK-NEXT: |           +--CallExpr
// CHECK-NEXT: |           |  +--Name `two`
// CHECK-NEXT: |           |  `--ArgumentList
// CHECK-NEXT: |           `--CallExpr
// CHECK-NEXT: |              +--Name `two`
// CHECK-NEXT: |              `--ArgumentList
// CHECK-NEXT: `--FunctionDecl
// CHECK-NEXT:    +--Name `two`
// CHECK-NEXT:    `--BlockStmt
// CHECK-NEXT:       `--ReturnStmt
// CHECK-NEXT:          `--NumberLiteral `2`

    { six() + two() }
    -> END
=== function six
    ~ return four() + two()
=== function four
    ~ return two() + two()
=== function two
    ~ return 2
