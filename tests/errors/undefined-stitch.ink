// RUN: %ink-compiler --stdin --compile-only --dump-ast < %s | FileCheck %s

// CHECK: <STDIN>:7:4: error: use of undeclared identifier 'somewhere.but_not_here'
// CHECK-NEXT:  7 | -> somewhere.but_not_here
// CHECK-NEXT     |

-> somewhere.but_not_here

== somewhere
= here
