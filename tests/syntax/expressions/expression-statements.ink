// RUN: %ink-compiler < %s --stdin --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:16, line:16>
// CHECK-NEXT:    `--ExprStmt <line:16, col:3:20>
// CHECK-NEXT:       `--SubtractExpr <col:4, col:20>
// CHECK-NEXT:          |--MultiplyExpr <col:4, col:15>
// CHECK-NEXT:          |  |--AddExpr <col:4, col:10>
// CHECK-NEXT:          |  |  |--NegateExpr <col:4, col:6>
// CHECK-NEXT:          |  |  |  `--NumberLiteral `1` <col:5, col:6>
// CHECK-NEXT:          |  |  `--NumberLiteral `2` <col:9, col:10>
// CHECK-NEXT:          |  `--NumberLiteral `3` <col:14, col:15>
// CHECK-NEXT:          `--NegateExpr <col:18, col:20>
// CHECK-NEXT:             `--NumberLiteral `4` <col:19, col:20>

~ (-1 + 2) * 3 - -4
