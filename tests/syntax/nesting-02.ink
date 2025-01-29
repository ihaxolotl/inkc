// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:41, line:52>
// CHECK-NEXT:    |--VarDecl <line:41, col:1:15>
// CHECK-NEXT:    |  |--Identifier `foo` <col:5, col:8>
// CHECK-NEXT:    |  `--True <col:11, col:15>
// CHECK-NEXT:    `--ChoiceStmt <line:43, line:52>
// CHECK-NEXT:       |--ChoiceStarStmt <line:43, col:1:4>
// CHECK-NEXT:       |  |--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:       |  |  `--ChoiceStartContentExpr `A` <col:3, col:4>
// CHECK-NEXT:       |  `--BlockStmt <line:44, line:51>
// CHECK-NEXT:       |     `--ContentStmt <line:44, col:1:96>
// CHECK-NEXT:       |        `--Content <col:1, col:96>
// CHECK-NEXT:       |           `--InlineLogicExpr <col:1, col:96>
// CHECK-NEXT:       |              |--Identifier `foo` <col:2, col:5>
// CHECK-NEXT:       |              `--ConditionalContent <col:1, col:89>
// CHECK-NEXT:       |                 |--ConditionalBranch <col:7, col:21>
// CHECK-NEXT:       |                 |  |--True <col:7, col:11>
// CHECK-NEXT:       |                 |  `--BlockStmt <line:46, line:49>
// CHECK-NEXT:       |                 |     `--ChoiceStmt <line:46, line:49>
// CHECK-NEXT:       |                 |        |--ChoiceStarStmt <line:46, col:9:16>
// CHECK-NEXT:       |                 |        |  `--ChoiceContentExpr <col:11, col:16>
// CHECK-NEXT:       |                 |        |     `--ChoiceStartContentExpr `Foo A` <col:11, col:16>
// CHECK-NEXT:       |                 |        `--ChoiceStarStmt <line:47, col:9:16>
// CHECK-NEXT:       |                 |           `--ChoiceContentExpr <col:11, col:16>
// CHECK-NEXT:       |                 |              `--ChoiceStartContentExpr `Foo B` <col:11, col:16>
// CHECK-NEXT:       |                 `--ConditionalElseBranch <col:7, col:21>
// CHECK-NEXT:       |                    `--BlockStmt <line:49, line:51>
// CHECK-NEXT:       |                       `--ChoiceStmt <line:49, line:51>
// CHECK-NEXT:       |                          |--ChoiceStarStmt <line:49, col:9:16>
// CHECK-NEXT:       |                          |  `--ChoiceContentExpr <col:11, col:16>
// CHECK-NEXT:       |                          |     `--ChoiceStartContentExpr `Foo C` <col:11, col:16>
// CHECK-NEXT:       |                          `--ChoiceStarStmt <line:50, col:9:16>
// CHECK-NEXT:       |                             `--ChoiceContentExpr <col:11, col:16>
// CHECK-NEXT:       |                                `--ChoiceStartContentExpr `Foo D` <col:11, col:16>
// CHECK-NEXT:       `--ChoiceStarStmt <line:52, col:1:4>
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
