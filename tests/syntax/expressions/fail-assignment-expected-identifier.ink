// RUN: %ink-compiler --stdin --compile-only --dump-ast < %s | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: <STDIN>:12:5: error: expected identifier
// CHECK-NEXT:    12 | VAR = 123
// CHECK-NEXT:       |     ^

// CHECK: <STDIN>:13:8: error: expected identifier
// CHECK-NEXT:    13 | ~ temp = 123
// CHECK-NEXT:       |        ^

VAR = 123
~ temp = 123
