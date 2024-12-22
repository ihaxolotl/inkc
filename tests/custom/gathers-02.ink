// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--CompoundStmt <line:1, line:5>
// CHECK-NEXT:    `--ChoiceStmt <line:1, line:5>
// CHECK-NEXT:       `--ChoiceStarStmt <line:1, col:1:5>
// CHECK-NEXT:          |--ChoiceContentExpr <col:3, col:4>
// CHECK-NEXT:          |  `--ChoiceStartContentExpr `A` <col:3, col:4>
// CHECK-NEXT:          `--CompoundStmt <line:2, line:5>
// CHECK-NEXT:             |--GatheredChoiceStmt <col:1, col:24>
// CHECK-NEXT:             |  |--ChoiceStmt <line:2, line:4>
// CHECK-NEXT:             |  |  `--ChoiceStarStmt <line:2, col:1:7>
// CHECK-NEXT:             |  |     |--ChoiceContentExpr <col:4, col:6>
// CHECK-NEXT:             |  |     |  `--ChoiceStartContentExpr `A1` <col:4, col:6>
// CHECK-NEXT:             |  |     `--CompoundStmt <line:3, line:4>
// CHECK-NEXT:             |  |        `--ContentStmt <line:3, col:1:11>
// CHECK-NEXT:             |  |           `--ContentExpr <col:1, col:10>
// CHECK-NEXT:             |  |              `--StringLiteral `A1 Nested` <col:1, col:10>
// CHECK-NEXT:             |  `--GatherStmt <col:1, col:4>
// CHECK-NEXT:             `--ContentStmt <line:5, col:1:9>
// CHECK-NEXT:                `--ContentExpr <col:1, col:9>
// CHECK-NEXT:                   `--StringLiteral `A Nested` <col:1, col:9>

* A
** A1
A1 Nested
--
A Nested
