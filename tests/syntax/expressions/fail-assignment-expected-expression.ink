// RUN: %ink-compiler --stdin --compile-only --dump-ast < %s | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: <STDIN>:12:11: error: expected expression
// CHECK-NEXT:    12 | ~ temp a =
// CHECK-NEXT:       |           ^

// CHECK: <STDIN>:13:6: error: expected expression
// CHECK-NEXT:    13 | ~ a =
// CHECK-NEXT:       |      ^

~ temp a =
~ a =
