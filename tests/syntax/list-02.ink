// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt <line:13, line:13>
// CHECK-NEXT:    `--ListDecl <col:1, col:29>
// CHECK-NEXT:       `--ArgumentList <col:10, col:29>
// CHECK-NEXT:          |--SelectionListElementExpr <col:10, col:15>
// CHECK-NEXT:          |  `--Name `one` <col:11, col:14>
// CHECK-NEXT:          |--SelectionListElementExpr <col:17, col:22>
// CHECK-NEXT:          |  `--Name `two` <col:18, col:21>
// CHECK-NEXT:          `--Name `three` <col:24, col:29>

LIST a = (one), (two), three
