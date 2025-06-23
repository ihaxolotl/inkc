// RUN: %ink-compiler < %s --stdin --dump-ast --compile-only | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--KnotDecl <line:15, line:17>
// CHECK-NEXT:    |--KnotProto <col:1, col:13>
// CHECK-NEXT:    |  `--Identifier `knot` <col:5, col:9>
// CHECK-NEXT:    `--StitchDecl <line:16, line:17>
// CHECK-NEXT:       |--StitchProto <col:1, col:9>
// CHECK-NEXT:       |  `--Identifier `stitch` <col:3, col:9>
// CHECK-NEXT:       `--BlockStmt <line:17, line:17>
// CHECK-NEXT:          `--ContentStmt <line:17, col:1:14>
// CHECK-NEXT:             `--Content <col:1, col:14>
// CHECK-NEXT:                `--StringLiteral `Hello, world!` <col:1, col:14>

=== knot ===
= stitch
Hello, world!
