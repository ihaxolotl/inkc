// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:22, line:24>
// CHECK-NEXT:    |--ConstDecl <line:22, col:1:36>
// CHECK-NEXT:    |  |--Identifier `CONST_STR` <col:7, col:16>
// CHECK-NEXT:    |  `--StringExpr <line:22, col:19:35>
// CHECK-NEXT:    |     `--StringLiteral `ConstantString` <col:20, col:34>
// CHECK-NEXT:    |--VarDecl <line:23, col:1:24>
// CHECK-NEXT:    |  |--Identifier `varStr` <col:5, col:11>
// CHECK-NEXT:    |  `--Identifier `CONST_STR` <col:14, col:23>
// CHECK-NEXT:    `--ContentStmt <line:24, col:1:30>
// CHECK-NEXT:       `--Content <col:1, col:30>
// CHECK-NEXT:          `--InlineLogicExpr <col:1, col:30>
// CHECK-NEXT:             |--LogicalEqualityExpr <col:21, col:21>
// CHECK-NEXT:             |  |--Identifier `varStr` <col:2, col:8>
// CHECK-NEXT:             |  `--Identifier `CONST_STR` <col:12, col:21>
// CHECK-NEXT:             `--SequenceExpr <col:22, col:29>
// CHECK-NEXT:                `--Content <col:22, col:29>
// CHECK-NEXT:                   `--StringLiteral `success` <col:22, col:29>

CONST CONST_STR = "ConstantString"
VAR varStr = CONST_STR
{varStr == CONST_STR:success}
