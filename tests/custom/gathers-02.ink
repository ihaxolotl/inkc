// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:1, line:28>
// CHECK-NEXT:    `--ChoiceStmt <line:24, line:28>
// CHECK-NEXT:       `--ChoiceStarStmt <line:24, col:1:5>
// CHECK-NEXT:          |--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:          |  `--ChoiceStartContentExpr `A` <col:3, col:4>
// CHECK-NEXT:          `--BlockStmt <line:25, line:28>
// CHECK-NEXT:             |--GatheredChoiceStmt <line:25, line:27>
// CHECK-NEXT:             |  |--ChoiceStmt <line:25, line:28>
// CHECK-NEXT:             |  |  `--ChoiceStarStmt <line:25, col:1:7>
// CHECK-NEXT:             |  |     |--ChoiceContentExpr <col:4, col:6>
// CHECK-NEXT:             |  |     |  `--ChoiceStartContentExpr `A1` <col:4, col:6>
// CHECK-NEXT:             |  |     `--BlockStmt <line:26, line:28>
// CHECK-NEXT:             |  |        `--ContentStmt <line:26, col:1:11>
// CHECK-NEXT:             |  |           `--ContentExpr <col:1, col:10>
// CHECK-NEXT:             |  |              `--StringLiteral `A1 Nested` <col:1, col:10>
// CHECK-NEXT:             |  `--GatherStmt <col:1, col:4>
// CHECK-NEXT:             `--ContentStmt <line:28, col:1:9>
// CHECK-NEXT:                `--ContentExpr <col:1, col:9>
// CHECK-NEXT:                   `--StringLiteral `A Nested` <col:1, col:9>

* A
** A1
A1 Nested
--
A Nested
