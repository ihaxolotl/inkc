// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:41, line:48>
// CHECK-NEXT:    |--ContentStmt <line:41, col:1:32>
// CHECK-NEXT:    |  `--Content <col:1, col:32>
// CHECK-NEXT:    |     `--StringLiteral `"What's that?" my master asked.` <col:1, col:32>
// CHECK-NEXT:    |--GatheredStmt <line:42, line:48>
// CHECK-NEXT:    |  |--ChoiceStmt <line:42, line:48>
// CHECK-NEXT:    |  |  |--ChoiceStarStmt <line:42, col:1:41>
// CHECK-NEXT:    |  |  |  |--ChoiceContentExpr <col:3, col:41>
// CHECK-NEXT:    |  |  |  |  |--ChoiceStartContentExpr `"I am somewhat tired` <col:3, col:23>
// CHECK-NEXT:    |  |  |  |  |--ChoiceOptionOnlyContentExpr `."` <col:24, col:26>
// CHECK-NEXT:    |  |  |  |  `--ChoiceInnerContentExpr `," I repeated.` <col:27, col:41>
// CHECK-NEXT:    |  |  |  `--BlockStmt <line:43, line:43>
// CHECK-NEXT:    |  |  |     `--ContentStmt <line:43, col:5:47>
// CHECK-NEXT:    |  |  |        `--Content <col:5, col:47>
// CHECK-NEXT:    |  |  |           `--StringLiteral `"Really," he responded. "How deleterious."` <col:5, col:47>
// CHECK-NEXT:    |  |  |--ChoiceStarStmt <line:44, col:1:36>
// CHECK-NEXT:    |  |  |  |--ChoiceContentExpr <col:3, col:36>
// CHECK-NEXT:    |  |  |  |  |--ChoiceStartContentExpr `"Nothing, Monsieur!"` <col:3, col:23>
// CHECK-NEXT:    |  |  |  |  `--ChoiceInnerContentExpr ` I replied.` <col:25, col:36>
// CHECK-NEXT:    |  |  |  `--BlockStmt <line:45, line:45>
// CHECK-NEXT:    |  |  |     `--ContentStmt <line:45, col:5:23>
// CHECK-NEXT:    |  |  |        `--Content <col:5, col:23>
// CHECK-NEXT:    |  |  |           `--StringLiteral `"Very good, then."` <col:5, col:23>
// CHECK-NEXT:    |  |  `--ChoiceStarStmt <line:46, col:1:69>
// CHECK-NEXT:    |  |     |--ChoiceContentExpr <col:4, col:69>
// CHECK-NEXT:    |  |     |  |--ChoiceStartContentExpr `"I said, this journey is appalling` <col:4, col:38>
// CHECK-NEXT:    |  |     |  |--ChoiceOptionOnlyContentExpr `."` <col:39, col:41>
// CHECK-NEXT:    |  |     |  `--ChoiceInnerContentExpr ` and I want no more of it."` <col:42, col:69>
// CHECK-NEXT:    |  |     `--BlockStmt <line:47, line:47>
// CHECK-NEXT:    |  |        `--ContentStmt <line:47, col:5:103>
// CHECK-NEXT:    |  |           `--Content <col:5, col:103>
// CHECK-NEXT:    |  |              `--StringLiteral `"Ah," he replied, not unkindly. "I see you are feeling frustrated. Tomorrow, things will improve."` <col:5, col:103>
// CHECK-NEXT:    |  `--GatherPoint <line:48, col:1:3>
// CHECK-NEXT:    `--ContentStmt <line:48, col:3:41>
// CHECK-NEXT:       `--Content <col:3, col:41>
// CHECK-NEXT:          `--StringLiteral `With that Monsieur Fogg left the room.` <col:3, col:41>

"What's that?" my master asked.
*	"I am somewhat tired[."]," I repeated.
    "Really," he responded. "How deleterious."
*	"Nothing, Monsieur!"[] I replied.
    "Very good, then."
*  "I said, this journey is appalling[."] and I want no more of it."
    "Ah," he replied, not unkindly. "I see you are feeling frustrated. Tomorrow, things will improve."
-	With that Monsieur Fogg left the room.
