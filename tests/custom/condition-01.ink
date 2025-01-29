// RUN: %ink-compiler < %s --dump-ast --dump-code --compile-only | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:16, line:18>
// CHECK-NEXT:    `--ContentStmt <line:16, col:1:17>
// CHECK-NEXT:       `--Content <col:1, col:17>
// CHECK-NEXT:          `--InlineLogicExpr <col:1, col:17>
// CHECK-NEXT:             |--True <col:2, col:6>
// CHECK-NEXT:             `--ConditionalContent <col:1, col:9>
// CHECK-NEXT:                `--BlockStmt <line:17, line:17>
// CHECK-NEXT:                   `--ExprStmt <line:17, col:3:8>
// CHECK-NEXT:                      `--AddExpr <col:8, col:8>
// CHECK-NEXT:                         |--NumberLiteral `1` <col:3, col:4>
// CHECK-NEXT:                         `--NumberLiteral `1` <col:7, col:8>

{true:
Hello, world!
~ 1 + 1
}
