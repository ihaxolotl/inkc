// RUN: %ink-compiler --stdin --compile-only --dump-ast < %s | FileCheck %s

// CHECK: <STDIN>:7:4: error: use of undeclared identifier 'nowhere'
// CHECK-NEXT:  7 | -> nowhere
// CHECK-NEXT     |

-> nowhere

== someplace
