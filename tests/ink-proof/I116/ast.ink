// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:1, line:4>
// CHECK-NEXT:    `--ContentStmt <line:1, col:1:21>
// CHECK-NEXT:       `--Content <col:1, col:21>
// CHECK-NEXT:          `--InlineLogicExpr <col:1, col:21>
// CHECK-NEXT:             `--ConditionalContent <col:1, col:18>
// CHECK-NEXT:                `--ConditionalBranch <col:3, col:9>
// CHECK-NEXT:                   |--False <col:3, col:8>
// CHECK-NEXT:                   `--BlockStmt <line:3, line:4>
// CHECK-NEXT:                      `--ContentStmt <line:3, col:4:9>
// CHECK-NEXT:                         `--Content <col:4, col:8>
// CHECK-NEXT:                            `--StringLiteral `beep` <col:4, col:8>

{
- false:
   beep
}
