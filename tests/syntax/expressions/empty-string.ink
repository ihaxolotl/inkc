// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:14, line:15>
// CHECK-NEXT:    |--TempDecl <line:14, col:3:16>
// CHECK-NEXT:    |  |--Identifier `str` <col:8, col:11>
// CHECK-NEXT:    |  `--StringExpr `""` <col:14, col:16>
// CHECK-NEXT:    |     `--EmptyString <col:15, col:15>
// CHECK-NEXT:    `--ContentStmt <line:15, col:1:6>
// CHECK-NEXT:       `--Content <col:1, col:6>
// CHECK-NEXT:          `--InlineLogicExpr <col:1, col:6>
// CHECK-NEXT:             `--Identifier `str` <col:2, col:5>

~ temp str = ""
{str}
