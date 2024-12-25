// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt <line:11, line:11>
// CHECK-NEXT:    `--ListDecl <col:1, col:25>
// CHECK-NEXT:       `--ArgumentList <col:10, col:25>
// CHECK-NEXT:          |--Name `one` <col:10, col:13>
// CHECK-NEXT:          |--Name `two` <col:15, col:18>
// CHECK-NEXT:          `--Name `three` <col:20, col:25>

LIST a = one, two, three
