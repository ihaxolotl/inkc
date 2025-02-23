#include <stddef.h>
#include <stdint.h>

#include "arena.h"
#include "ast.h"
#include "astgen.h"
#include "compile.h"
#include "parse.h"
#include "story.h"

#define INK_ARENA_ALIGNMENT (8u)
#define INK_ARENA_BLOCK_SIZE (8192u)

int ink_compile(const uint8_t *source_bytes, const uint8_t *filename,
                struct ink_story *story, int flags)
{
    int rc;
    struct ink_arena arena;
    struct ink_ast ast;

    ink_arena_init(&arena, INK_ARENA_BLOCK_SIZE, INK_ARENA_ALIGNMENT);

    rc = ink_parse(source_bytes, filename, &arena, &ast, flags);
    if (rc < 0) {
        goto out;
    }
    if (flags & INK_F_DUMP_AST) {
        ink_ast_print(&ast, flags & INK_F_COLOR);
    }

    rc = ink_astgen(&ast, story, flags);
    if (rc < 0) {
        goto out;
    }
    if (flags & INK_F_DUMP_CODE) {
        ink_story_dump(story);
    }
out:
    ink_ast_deinit(&ast);
    ink_arena_release(&arena);
    return rc;
}
