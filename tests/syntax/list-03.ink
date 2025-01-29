// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt <line:17, line:17>
// CHECK-NEXT:    `--ListDecl <col:1, col:49>
// CHECK-NEXT:       `--ArgumentList <col:21, col:49>
// CHECK-NEXT:          |--AssignExpr <col:21, col:28>
// CHECK-NEXT:          |  |--Name `two` <col:21, col:24>
// CHECK-NEXT:          |  `--NumberLiteral `2` <col:27, col:28>
// CHECK-NEXT:          |--AssignExpr <col:30, col:39>
// CHECK-NEXT:          |  |--Name `three` <col:30, col:35>
// CHECK-NEXT:          |  `--NumberLiteral `3` <col:38, col:39>
// CHECK-NEXT:          `--AssignExpr <col:41, col:49>
// CHECK-NEXT:             |--Name `five` <col:41, col:45>
// CHECK-NEXT:             `--NumberLiteral `5` <col:48, col:49>

LIST primeNumbers = two = 2, three = 3, five = 5
