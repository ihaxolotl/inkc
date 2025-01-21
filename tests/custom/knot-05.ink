// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: |--KnotDecl <line:11, line:12>
// CHECK-NEXT: |  `--KnotProto <col:1, col:6>
// CHECK-NEXT: |     `--Identifier `a` <col:4, col:5>
// CHECK-NEXT: `--KnotDecl <line:12, line:12>
// CHECK-NEXT:    `--KnotProto <col:1, col:5>
// CHECK-NEXT:       `--Identifier `b` <col:4, col:5>

== a
== b
