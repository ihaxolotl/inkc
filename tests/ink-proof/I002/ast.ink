// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:36, line:42>
// CHECK-NEXT:    |--ContentStmt <line:36, col:1:32>
// CHECK-NEXT:    |  `--Content <col:1, col:32>
// CHECK-NEXT:    |     `--StringLiteral `"What's that?" my master asked.` <col:1, col:32>
// CHECK-NEXT:    `--ChoiceStmt <line:37, line:42>
// CHECK-NEXT:       |--ChoiceStarStmt <line:37, col:1:41>
// CHECK-NEXT:       |  |--ChoiceContentExpr <col:3, col:41>
// CHECK-NEXT:       |  |  |--ChoiceStartContentExpr `"I am somewhat tired` <col:3, col:23>
// CHECK-NEXT:       |  |  |--ChoiceOptionOnlyContentExpr `."` <col:24, col:26>
// CHECK-NEXT:       |  |  `--ChoiceInnerContentExpr `," I repeated.` <col:27, col:41>
// CHECK-NEXT:       |  `--BlockStmt <line:38, line:38>
// CHECK-NEXT:       |     `--ContentStmt <line:38, col:3:45>
// CHECK-NEXT:       |        `--Content <col:3, col:45>
// CHECK-NEXT:       |           `--StringLiteral `"Really," he responded. "How deleterious."` <col:3, col:45>
// CHECK-NEXT:       |--ChoiceStarStmt <line:39, col:1:36>
// CHECK-NEXT:       |  |--ChoiceContentExpr <col:3, col:36>
// CHECK-NEXT:       |  |  |--ChoiceStartContentExpr `"Nothing, Monsieur!"` <col:3, col:23>
// CHECK-NEXT:       |  |  `--ChoiceInnerContentExpr ` I replied.` <col:25, col:36>
// CHECK-NEXT:       |  `--BlockStmt <line:40, line:40>
// CHECK-NEXT:       |     `--ContentStmt <line:40, col:3:21>
// CHECK-NEXT:       |        `--Content <col:3, col:21>
// CHECK-NEXT:       |           `--StringLiteral `"Very good, then."` <col:3, col:21>
// CHECK-NEXT:       `--ChoiceStarStmt <line:41, col:1:68>
// CHECK-NEXT:          |--ChoiceContentExpr <col:3, col:68>
// CHECK-NEXT:          |  |--ChoiceStartContentExpr `"I said, this journey is appalling` <col:3, col:37>
// CHECK-NEXT:          |  |--ChoiceOptionOnlyContentExpr `."` <col:38, col:40>
// CHECK-NEXT:          |  `--ChoiceInnerContentExpr ` and I want no more of it."` <col:41, col:68>
// CHECK-NEXT:          `--BlockStmt <line:42, line:42>
// CHECK-NEXT:             `--ContentStmt <line:42, col:3:101>
// CHECK-NEXT:                `--Content <col:3, col:101>
// CHECK-NEXT:                   `--StringLiteral `"Ah," he replied, not unkindly. "I see you are feeling frustrated. Tomorrow, things will improve."` <col:3, col:101>

"What's that?" my master asked.
* "I am somewhat tired[."]," I repeated.
  "Really," he responded. "How deleterious."
* "Nothing, Monsieur!"[] I replied.
  "Very good, then."
* "I said, this journey is appalling[."] and I want no more of it."
  "Ah," he replied, not unkindly. "I see you are feeling frustrated. Tomorrow, things will improve."
