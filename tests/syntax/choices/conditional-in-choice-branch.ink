// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:29, line:36>
// CHECK-NEXT:    |--VarDecl <line:29, col:1:15>
// CHECK-NEXT:    |  |--Identifier `foo` <col:5, col:8>
// CHECK-NEXT:    |  `--True <col:11, col:15>
// CHECK-NEXT:    `--ChoiceStmt <line:31, line:36>
// CHECK-NEXT:       |--ChoiceStarStmt <line:31, col:1:4>
// CHECK-NEXT:       |  |--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:       |  |  `--ChoiceStartContentExpr `A` <col:3, col:4>
// CHECK-NEXT:       |  `--BlockStmt <line:32, line:35>
// CHECK-NEXT:       |     `--ContentStmt <line:32, col:1:32>
// CHECK-NEXT:       |        `--Content <col:1, col:32>
// CHECK-NEXT:       |           `--IfStmt <col:1, col:25>
// CHECK-NEXT:       |              |--Identifier `foo` <col:2, col:5>
// CHECK-NEXT:       |              `--BlockStmt <line:33, line:35>
// CHECK-NEXT:       |                 `--ChoiceStmt <line:33, line:35>
// CHECK-NEXT:       |                    |--ChoiceStarStmt <line:33, col:5:12>
// CHECK-NEXT:       |                    |  `--ChoiceContentExpr <col:7, col:12>
// CHECK-NEXT:       |                    |     `--ChoiceStartContentExpr `Foo A` <col:7, col:12>
// CHECK-NEXT:       |                    `--ChoiceStarStmt <line:34, col:5:12>
// CHECK-NEXT:       |                       `--ChoiceContentExpr <col:7, col:12>
// CHECK-NEXT:       |                          `--ChoiceStartContentExpr `Foo B` <col:7, col:12>
// CHECK-NEXT:       `--ChoiceStarStmt <line:36, col:1:4>
// CHECK-NEXT:          `--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:             `--ChoiceStartContentExpr `B` <col:3, col:4>

VAR foo = true

* A
{foo:
    * Foo A
    * Foo B
}
* B
