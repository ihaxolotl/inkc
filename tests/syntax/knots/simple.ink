// RUN: %ink-compiler --stdin --compile-only --dump-ast < %s | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--KnotDecl <line:12, line:13>
// CHECK-NEXT:    |--KnotProto <col:1, col:13>
// CHECK-NEXT:    |  `--Identifier `knot` <col:5, col:9>
// CHECK-NEXT:    `--BlockStmt <line:13, line:13>
// CHECK-NEXT:       `--ContentStmt <line:13, col:1:14>
// CHECK-NEXT:          `--Content <col:1, col:14>
// CHECK-NEXT:             `--StringLiteral `Hello, world!` <col:1, col:14>

=== knot ===
Hello, world!
