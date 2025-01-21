// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:30, line:37>
// CHECK-NEXT:    |--VarDecl <line:30, col:1:15>
// CHECK-NEXT:    |  |--Identifier `foo` <col:5, col:8>
// CHECK-NEXT:    |  `--True <col:11, col:15>
// CHECK-NEXT:    `--ChoiceStmt <line:32, line:37>
// CHECK-NEXT:       |--ChoiceStarStmt <line:32, col:1:4>
// CHECK-NEXT:       |  |--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:       |  |  `--ChoiceStartContentExpr `A` <col:3, col:4>
// CHECK-NEXT:       |  `--BlockStmt <line:33, line:36>
// CHECK-NEXT:       |     `--ContentStmt <line:33, col:1:32>
// CHECK-NEXT:       |        `--Content <col:1, col:32>
// CHECK-NEXT:       |           `--InlineLogicExpr <col:1, col:32>
// CHECK-NEXT:       |              |--Identifier `foo` <col:2, col:5>
// CHECK-NEXT:       |              `--ConditionalContent <col:1, col:25>
// CHECK-NEXT:       |                 `--BlockStmt <line:34, line:36>
// CHECK-NEXT:       |                    `--ChoiceStmt <line:34, line:36>
// CHECK-NEXT:       |                       |--ChoiceStarStmt <line:34, col:5:12>
// CHECK-NEXT:       |                       |  `--ChoiceContentExpr <col:7, col:12>
// CHECK-NEXT:       |                       |     `--ChoiceStartContentExpr `Foo A` <col:7, col:12>
// CHECK-NEXT:       |                       `--ChoiceStarStmt <line:35, col:5:12>
// CHECK-NEXT:       |                          `--ChoiceContentExpr <col:7, col:12>
// CHECK-NEXT:       |                             `--ChoiceStartContentExpr `Foo B` <col:7, col:12>
// CHECK-NEXT:       `--ChoiceStarStmt <line:37, col:1:4>
// CHECK-NEXT:          `--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:             `--ChoiceStartContentExpr `B` <col:3, col:4>

VAR foo = true

* A
{foo:
    * Foo A
    * Foo B
}
* B
