// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK-NEXT: `--BlockStmt
// CHECK-NEXT:    +--ContentStmt
// CHECK-NEXT:    |  `--ContentExpr
// CHECK-NEXT:    |     `--StringLiteral `"What's that?" my master asked.`
// CHECK-NEXT:    `--ChoiceStmt
// CHECK-NEXT:       +--ChoiceStarBranch
// CHECK-NEXT:       |  +--ChoiceContentExpr
// CHECK-NEXT:       |  |  +--StringLiteral `"I am somewhat tired`
// CHECK-NEXT:       |  |  +--StringLiteral `."`
// CHECK-NEXT:       |  |  `--StringLiteral `," I repeated.`
// CHECK-NEXT:       |  `--BlockStmt
// CHECK-NEXT:       |     `--ContentStmt
// CHECK-NEXT:       |        `--ContentExpr
// CHECK-NEXT:       |           `--StringLiteral `"Really," he responded. "How deleterious."`
// CHECK-NEXT:       +--ChoiceStarBranch
// CHECK-NEXT:       |  +--ChoiceContentExpr
// CHECK-NEXT:       |  |  +--StringLiteral `"Nothing, Monsieur!"`
// CHECK-NEXT:       |  |  +--StringLiteral ``
// CHECK-NEXT:       |  |  `--StringLiteral ` I replied.`
// CHECK-NEXT:       |  `--BlockStmt
// CHECK-NEXT:       |     `--ContentStmt
// CHECK-NEXT:       |        `--ContentExpr
// CHECK-NEXT:       |           `--StringLiteral `"Very good, then."`
// CHECK-NEXT:       `--ChoiceStarBranch
// CHECK-NEXT:          +--ChoiceContentExpr
// CHECK-NEXT:          |  +--StringLiteral `"I said, this journey is appalling`
// CHECK-NEXT:          |  +--StringLiteral `."`
// CHECK-NEXT:          |  `--StringLiteral ` and I want no more of it."`
// CHECK-NEXT:          `--BlockStmt
// CHECK-NEXT:             `--ContentStmt
// CHECK-NEXT:                `--ContentExpr
// CHECK-NEXT:                   `--StringLiteral `"Ah," he replied, not unkindly. "I see you are feeling frustrated. Tomorrow, things will improve."`

"What's that?" my master asked.
* "I am somewhat tired[."]," I repeated.
  "Really," he responded. "How deleterious."
* "Nothing, Monsieur!"[] I replied.
  "Very good, then."
* "I said, this journey is appalling[."] and I want no more of it."
  "Ah," he replied, not unkindly. "I see you are feeling frustrated. Tomorrow, things will improve."
