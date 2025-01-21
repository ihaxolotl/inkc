// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "tests/ink-proof/I007/ast.ink"
// CHECK-NEXT: `--BlockStmt <line:16, line:17>
// CHECK-NEXT:    |--VarDecl <line:16, col:1:17>
// CHECK-NEXT:    |  |--Identifier `x` <col:5, col:6>
// CHECK-NEXT:    |  `--StringExpr <line:16, col:9:16>
// CHECK-NEXT:    |     `--StringLiteral `world` <col:10, col:15>
// CHECK-NEXT:    `--ContentStmt <line:17, col:1:11>
// CHECK-NEXT:       `--Content <col:1, col:11>
// CHECK-NEXT:          |--StringLiteral `Hello ` <col:1, col:7>
// CHECK-NEXT:          |--InlineLogicExpr <col:7, col:10>
// CHECK-NEXT:          |  `--Identifier `x` <col:8, col:9>
// CHECK-NEXT:          `--StringLiteral `.` <col:10, col:11>

VAR x = "world"
Hello {x}.
