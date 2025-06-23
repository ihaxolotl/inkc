// RUN: %ink-compiler < %s --stdin --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:21, line:23>
// CHECK-NEXT:    |--VarDecl <line:21, col:1:24>
// CHECK-NEXT:    |  |--Identifier `x` <col:5, col:6>
// CHECK-NEXT:    |  `--StringExpr `"Hello world 1"` <col:9, col:24>
// CHECK-NEXT:    |     `--StringLiteral `Hello world 1` <col:10, col:23>
// CHECK-NEXT:    |--ContentStmt <line:22, col:1:4>
// CHECK-NEXT:    |  `--Content <col:1, col:4>
// CHECK-NEXT:    |     `--InlineLogicExpr <col:1, col:4>
// CHECK-NEXT:    |        `--Identifier `x` <col:2, col:3>
// CHECK-NEXT:    `--ContentStmt <line:23, col:1:19>
// CHECK-NEXT:       `--Content <col:1, col:19>
// CHECK-NEXT:          |--StringLiteral `Hello ` <col:1, col:7>
// CHECK-NEXT:          |--InlineLogicExpr <col:7, col:16>
// CHECK-NEXT:          |  `--StringExpr `"world"` <col:8, col:15>
// CHECK-NEXT:          |     `--StringLiteral `world` <col:9, col:14>
// CHECK-NEXT:          `--StringLiteral ` 2.` <col:16, col:19>

VAR x = "Hello world 1"
{x}
Hello {"world"} 2.
