// RUN: %ink-compiler --stdin --compile-only --dump-ast < %s | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:40, line:51>
// CHECK-NEXT:    |--VarDecl <line:40, col:1:15>
// CHECK-NEXT:    |  |--Identifier `foo` <col:5, col:8>
// CHECK-NEXT:    |  `--True <col:11, col:15>
// CHECK-NEXT:    `--ChoiceStmt <line:42, line:51>
// CHECK-NEXT:       |--ChoiceStarStmt <line:42, col:1:4>
// CHECK-NEXT:       |  |--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:       |  |  `--ChoiceStartContentExpr `A` <col:3, col:4>
// CHECK-NEXT:       |  `--BlockStmt <line:43, line:50>
// CHECK-NEXT:       |     `--ContentStmt <line:43, col:1:96>
// CHECK-NEXT:       |        `--Content <col:1, col:96>
// CHECK-NEXT:       |           `--SwitchStmt <col:1, col:89>
// CHECK-NEXT:       |              |--Identifier `foo` <col:2, col:5>
// CHECK-NEXT:       |              |--SwitchCase <col:7, col:21>
// CHECK-NEXT:       |              |  |--True <col:7, col:11>
// CHECK-NEXT:       |              |  `--BlockStmt <line:45, line:48>
// CHECK-NEXT:       |              |     `--ChoiceStmt <line:45, line:48>
// CHECK-NEXT:       |              |        |--ChoiceStarStmt <line:45, col:9:16>
// CHECK-NEXT:       |              |        |  `--ChoiceContentExpr <col:11, col:16>
// CHECK-NEXT:       |              |        |     `--ChoiceStartContentExpr `Foo A` <col:11, col:16>
// CHECK-NEXT:       |              |        `--ChoiceStarStmt <line:46, col:9:16>
// CHECK-NEXT:       |              |           `--ChoiceContentExpr <col:11, col:16>
// CHECK-NEXT:       |              |              `--ChoiceStartContentExpr `Foo B` <col:11, col:16>
// CHECK-NEXT:       |              `--ElseBranch <col:7, col:21>
// CHECK-NEXT:       |                 `--BlockStmt <line:48, line:50>
// CHECK-NEXT:       |                    `--ChoiceStmt <line:48, line:50>
// CHECK-NEXT:       |                       |--ChoiceStarStmt <line:48, col:9:16>
// CHECK-NEXT:       |                       |  `--ChoiceContentExpr <col:11, col:16>
// CHECK-NEXT:       |                       |     `--ChoiceStartContentExpr `Foo C` <col:11, col:16>
// CHECK-NEXT:       |                       `--ChoiceStarStmt <line:49, col:9:16>
// CHECK-NEXT:       |                          `--ChoiceContentExpr <col:11, col:16>
// CHECK-NEXT:       |                             `--ChoiceStartContentExpr `Foo D` <col:11, col:16>
// CHECK-NEXT:       `--ChoiceStarStmt <line:51, col:1:4>
// CHECK-NEXT:          `--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:             `--ChoiceStartContentExpr `B` <col:3, col:4>

VAR foo = true

* A
{foo:
    - true:
        * Foo A
        * Foo B
    - else:
        * Foo C
        * Foo D
}
* B
