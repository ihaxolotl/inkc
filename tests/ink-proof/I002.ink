// RUN: %ink-compiler < %s --dump-ast | FileCheck %s

// CHECK: File "STDIN"
// CHECK: `-- BlockStmt
// CHECK:     +-- ContentStmt
// CHECK:     |   `-- StringLiteral `"What's that?" my master asked.`
// CHECK:     `-- ChoiceStmt
// CHECK:         +-- ChoiceStarBranch
// CHECK:         |   +-- ChoiceContentExpr
// CHECK:         |   |   +-- StringLiteral `"I am somewhat tired`
// CHECK:         |   |   +-- StringLiteral `."`
// CHECK:         |   |   `-- StringLiteral `," I repeated.`
// CHECK:         |   `-- BlockStmt
// CHECK:         |       `-- ContentStmt
// CHECK:         |           `-- StringLiteral `"Really," he responded. "How deleterious."`
// CHECK:         +-- ChoiceStarBranch
// CHECK:         |   +-- ChoiceContentExpr
// CHECK:         |   |   +-- StringLiteral `"Nothing, Monsieur!"`
// CHECK:         |   |   +-- StringLiteral ``
// CHECK:         |   |   `-- StringLiteral ` I replied.`
// CHECK:         |   `-- BlockStmt
// CHECK:         |       `-- ContentStmt
// CHECK:         |           `-- StringLiteral `"Very good, then."`
// CHECK:         `-- ChoiceStarBranch
// CHECK:             +-- ChoiceContentExpr
// CHECK:             |   +-- StringLiteral `"I said, this journey is appalling`
// CHECK:             |   +-- StringLiteral `."`
// CHECK:             |   `-- StringLiteral ` and I want no more of it."`
// CHECK:             `-- BlockStmt
// CHECK:                 `-- ContentStmt
// CHECK:                     `-- StringLiteral `"Ah," he replied, not unkindly. "I see you are feeling frustrated. Tomorrow, things will improve."`

"What's that?" my master asked.
* "I am somewhat tired[."]," I repeated.
  "Really," he responded. "How deleterious."
* "Nothing, Monsieur!"[] I replied.
  "Very good, then."
* "I said, this journey is appalling[."] and I want no more of it."
  "Ah," he replied, not unkindly. "I see you are feeling frustrated. Tomorrow, things will improve."
