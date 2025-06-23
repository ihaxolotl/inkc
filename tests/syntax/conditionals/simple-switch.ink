// RUN: %ink-compiler < %s --stdin --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:36, line:43>
// CHECK-NEXT:    |--TempDecl <line:36, col:3:13>
// CHECK-NEXT:    |  |--Identifier `x` <col:8, col:9>
// CHECK-NEXT:    |  `--NumberLiteral `3` <col:12, col:13>
// CHECK-NEXT:    `--ContentStmt <line:38, col:1:51>
// CHECK-NEXT:       `--Content <col:1, col:51>
// CHECK-NEXT:          `--SwitchStmt <col:1, col:45>
// CHECK-NEXT:             |--Identifier `x` <col:3, col:4>
// CHECK-NEXT:             |--SwitchCase <col:3, col:7>
// CHECK-NEXT:             |  |--NumberLiteral `0` <col:3, col:4>
// CHECK-NEXT:             |  `--BlockStmt <line:39, line:39>
// CHECK-NEXT:             |     `--ContentStmt <line:39, col:7:11>
// CHECK-NEXT:             |        `--Content <col:7, col:11>
// CHECK-NEXT:             |           `--StringLiteral `zero` <col:7, col:11>
// CHECK-NEXT:             |--SwitchCase <col:3, col:7>
// CHECK-NEXT:             |  |--NumberLiteral `1` <col:3, col:4>
// CHECK-NEXT:             |  `--BlockStmt <line:40, line:40>
// CHECK-NEXT:             |     `--ContentStmt <line:40, col:7:10>
// CHECK-NEXT:             |        `--Content <col:7, col:10>
// CHECK-NEXT:             |           `--StringLiteral `one` <col:7, col:10>
// CHECK-NEXT:             |--SwitchCase <col:3, col:7>
// CHECK-NEXT:             |  |--NumberLiteral `2` <col:3, col:4>
// CHECK-NEXT:             |  `--BlockStmt <line:41, line:41>
// CHECK-NEXT:             |     `--ContentStmt <line:41, col:7:10>
// CHECK-NEXT:             |        `--Content <col:7, col:10>
// CHECK-NEXT:             |           `--StringLiteral `two` <col:7, col:10>
// CHECK-NEXT:             `--ElseBranch <col:3, col:9>
// CHECK-NEXT:                `--BlockStmt <line:42, line:42>
// CHECK-NEXT:                   `--ContentStmt <line:42, col:9:13>
// CHECK-NEXT:                      `--Content <col:9, col:13>
// CHECK-NEXT:                         `--StringLiteral `lots` <col:9, col:13>

~ temp x = 3

{ x:
- 0: 	zero
- 1: 	one
- 2: 	two
- else: lots
}
