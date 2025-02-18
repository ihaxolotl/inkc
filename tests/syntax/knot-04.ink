// RUN: %ink-compiler < %s --dump-ast --compile-only | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--KnotDecl <line:26, line:31>
// CHECK-NEXT:    |--KnotProto <col:1, col:14>
// CHECK-NEXT:    |  `--Identifier `knot` <col:5, col:9>
// CHECK-NEXT:    |--BlockStmt <line:27, line:27>
// CHECK-NEXT:    |  `--ContentStmt <line:27, col:1:19>
// CHECK-NEXT:    |     `--Content <col:1, col:19>
// CHECK-NEXT:    |        `--StringLiteral `Hello from `knot`!` <col:1, col:19>
// CHECK-NEXT:    |--StitchDecl <line:28, line:29>
// CHECK-NEXT:    |  |--StitchProto <col:1, col:5>
// CHECK-NEXT:    |  |  `--Identifier `a` <col:3, col:4>
// CHECK-NEXT:    |  `--BlockStmt <line:29, line:29>
// CHECK-NEXT:    |     `--ContentStmt <line:29, col:1:21>
// CHECK-NEXT:    |        `--Content <col:1, col:21>
// CHECK-NEXT:    |           `--StringLiteral `Hello from `knot.a`!` <col:1, col:21>
// CHECK-NEXT:    `--StitchDecl <line:30, line:31>
// CHECK-NEXT:       |--StitchProto <col:1, col:5>
// CHECK-NEXT:       |  `--Identifier `b` <col:3, col:4>
// CHECK-NEXT:       `--BlockStmt <line:31, line:31>
// CHECK-NEXT:          `--ContentStmt <line:31, col:1:21>
// CHECK-NEXT:             `--Content <col:1, col:21>
// CHECK-NEXT:                `--StringLiteral `Hello from `knot.b`!` <col:1, col:21>

=== knot ===
Hello from `knot`!
= a
Hello from `knot.a`!
= b
Hello from `knot.b`!
