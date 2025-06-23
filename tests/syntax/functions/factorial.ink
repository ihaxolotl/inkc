// RUN: %ink-compiler < %s --stdin --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: |--BlockStmt <line:39, line:39>
// CHECK-NEXT: |  `--ContentStmt <line:39, col:1:18>
// CHECK-NEXT: |     `--Content <col:1, col:18>
// CHECK-NEXT: |        `--InlineLogicExpr <col:1, col:18>
// CHECK-NEXT: |           `--CallExpr <col:3, col:17>
// CHECK-NEXT: |              |--Identifier `factorial` <col:3, col:12>
// CHECK-NEXT: |              `--ArgumentList <col:12, col:17>
// CHECK-NEXT: |                 `--NumberLiteral `10` <col:13, col:15>
// CHECK-NEXT: `--FunctionDecl <line:41, line:46>
// CHECK-NEXT:    |--FunctionProto <col:1, col:25>
// CHECK-NEXT:    |  |--Identifier `factorial` <col:13, col:22>
// CHECK-NEXT:    |  `--ParamList <col:22, col:25>
// CHECK-NEXT:    |     `--ParamDecl `n` <col:23, col:24>
// CHECK-NEXT:    `--BlockStmt <line:42, line:46>
// CHECK-NEXT:       `--ContentStmt <line:42, col:3:83>
// CHECK-NEXT:          `--Content <col:3, col:83>
// CHECK-NEXT:             `--IfStmt <col:1, col:70>
// CHECK-NEXT:                |--LogicalEqualityExpr <col:5, col:11>
// CHECK-NEXT:                |  |--Identifier `n` <col:5, col:6>
// CHECK-NEXT:                |  `--NumberLiteral `1` <col:10, col:11>
// CHECK-NEXT:                |--BlockStmt <line:43, line:43>
// CHECK-NEXT:                |  `--ReturnStmt <line:43, col:9:17>
// CHECK-NEXT:                |     `--NumberLiteral `1` <col:16, col:17>
// CHECK-NEXT:                `--ElseBranch <col:7, col:19>
// CHECK-NEXT:                   `--BlockStmt <line:45, line:45>
// CHECK-NEXT:                      `--ReturnStmt <line:45, col:9:38>
// CHECK-NEXT:                         `--MultiplyExpr <col:17, col:37>
// CHECK-NEXT:                            |--Identifier `n` <col:17, col:18>
// CHECK-NEXT:                            `--CallExpr <col:21, col:37>
// CHECK-NEXT:                               |--Identifier `factorial` <col:21, col:30>
// CHECK-NEXT:                               `--ArgumentList <col:30, col:37>
// CHECK-NEXT:                                  `--SubtractExpr <col:31, col:36>
// CHECK-NEXT:                                     |--Identifier `n` <col:31, col:32>
// CHECK-NEXT:                                     `--NumberLiteral `1` <col:35, col:36>

{ factorial(10) }

== function factorial(n)
  { n == 1:
      ~ return 1
    - else:
      ~ return (n * factorial(n - 1))
  }
