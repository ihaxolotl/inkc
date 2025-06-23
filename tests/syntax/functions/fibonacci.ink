// RUN: %ink-compiler < %s --stdin --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: |--BlockStmt <line:52, line:52>
// CHECK-NEXT: |  `--ContentStmt <line:52, col:1:12>
// CHECK-NEXT: |     `--Content <col:1, col:12>
// CHECK-NEXT: |        `--InlineLogicExpr <col:1, col:12>
// CHECK-NEXT: |           `--CallExpr <col:3, col:11>
// CHECK-NEXT: |              |--Identifier `fib` <col:3, col:6>
// CHECK-NEXT: |              `--ArgumentList <col:6, col:11>
// CHECK-NEXT: |                 `--NumberLiteral `10` <col:7, col:9>
// CHECK-NEXT: `--FunctionDecl <line:54, line:62>
// CHECK-NEXT:    |--FunctionProto <col:1, col:19>
// CHECK-NEXT:    |  |--Identifier `fib` <col:13, col:16>
// CHECK-NEXT:    |  `--ParamList <col:16, col:19>
// CHECK-NEXT:    |     `--ParamDecl `n` <col:17, col:18>
// CHECK-NEXT:    `--BlockStmt <line:55, line:62>
// CHECK-NEXT:       `--ContentStmt <line:55, col:3:121>
// CHECK-NEXT:          `--Content <col:3, col:121>
// CHECK-NEXT:             `--MultiIfStmt <col:5, col:116>
// CHECK-NEXT:                |--IfBranch <col:7, col:21>
// CHECK-NEXT:                |  |--LogicalEqualityExpr <col:7, col:13>
// CHECK-NEXT:                |  |  |--Identifier `n` <col:7, col:8>
// CHECK-NEXT:                |  |  `--NumberLiteral `0` <col:12, col:13>
// CHECK-NEXT:                |  `--BlockStmt <line:57, line:57>
// CHECK-NEXT:                |     `--ReturnStmt <line:57, col:9:17>
// CHECK-NEXT:                |        `--NumberLiteral `0` <col:16, col:17>
// CHECK-NEXT:                |--IfBranch <col:7, col:21>
// CHECK-NEXT:                |  |--LogicalEqualityExpr <col:7, col:13>
// CHECK-NEXT:                |  |  |--Identifier `n` <col:7, col:8>
// CHECK-NEXT:                |  |  `--NumberLiteral `1` <col:12, col:13>
// CHECK-NEXT:                |  `--BlockStmt <line:59, line:59>
// CHECK-NEXT:                |     `--ReturnStmt <line:59, col:9:17>
// CHECK-NEXT:                |        `--NumberLiteral `1` <col:16, col:17>
// CHECK-NEXT:                `--ElseBranch <col:7, col:19>
// CHECK-NEXT:                   `--BlockStmt <line:61, line:61>
// CHECK-NEXT:                      `--ReturnStmt <line:61, col:9:39>
// CHECK-NEXT:                         `--AddExpr <col:16, col:39>
// CHECK-NEXT:                            |--CallExpr <col:16, col:27>
// CHECK-NEXT:                            |  |--Identifier `fib` <col:16, col:19>
// CHECK-NEXT:                            |  `--ArgumentList <col:19, col:27>
// CHECK-NEXT:                            |     `--SubtractExpr <col:20, col:25>
// CHECK-NEXT:                            |        |--Identifier `n` <col:20, col:21>
// CHECK-NEXT:                            |        `--NumberLiteral `1` <col:24, col:25>
// CHECK-NEXT:                            `--CallExpr <col:29, col:39>
// CHECK-NEXT:                               |--Identifier `fib` <col:29, col:32>
// CHECK-NEXT:                               `--ArgumentList <col:32, col:39>
// CHECK-NEXT:                                  `--SubtractExpr <col:33, col:38>
// CHECK-NEXT:                                     |--Identifier `n` <col:33, col:34>
// CHECK-NEXT:                                     `--NumberLiteral `2` <col:37, col:38>

{ fib(10) }

== function fib(n)
  {
    - n == 0:
      ~ return 0
    - n == 1:
      ~ return 1
    - else:
      ~ return fib(n - 1) + fib(n - 2)
  }
