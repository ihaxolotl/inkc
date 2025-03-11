// RUN: %ink-compiler < %s --compile-only --dump-ast | FileCheck %s
// Copy of `ink-proof/ink/I081/story.ink`.

// CHECK: File "<STDIN>"
// CHECK-NEXT: `--BlockStmt <line:18, line:19>
// CHECK-NEXT:    |--GatherPoint <line:18, col:1:3>
// CHECK-NEXT:    |--GatheredStmt <line:18, line:19>
// CHECK-NEXT:    |  |--ChoiceStmt <line:18, line:19>
// CHECK-NEXT:    |  |  `--ChoiceStarStmt <line:18, col:3:10>
// CHECK-NEXT:    |  |     `--ChoiceContentExpr <col:5, col:10>
// CHECK-NEXT:    |  |        `--ChoiceStartContentExpr `hello` <col:5, col:10>
// CHECK-NEXT:    |  `--GatherPoint <line:19, col:1:3>
// CHECK-NEXT:    `--ChoiceStmt <line:19, line:19>
// CHECK-NEXT:       `--ChoiceStarStmt <line:19, col:3:10>
// CHECK-NEXT:          `--ChoiceContentExpr <col:5, col:10>
// CHECK-NEXT:             `--ChoiceStartContentExpr `world` <col:5, col:10>

- * hello
- * world
