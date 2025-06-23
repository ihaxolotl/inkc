// RUN: %ink-compiler --stdin --compile-only --dump-ast < %s | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: |--KnotDecl <line:11, line:11>
// CHECK-NEXT: |  `--KnotProto <col:1, col:5>
// CHECK-NEXT: |     `--Identifier `a` <col:4, col:5>
// CHECK-NEXT: `--KnotDecl <line:12, line:12>
// CHECK-NEXT:    `--KnotProto <col:1, col:5>
// CHECK-NEXT:       `--Identifier `b` <col:4, col:5>

== a
== b
