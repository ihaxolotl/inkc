#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <ink/ink.h>

#include "arena.h"
#include "ast.h"
#include "astgen.h"
#include "compile.h"
#include "parse.h"

#define INK_ARENA_ALIGNMENT (8u)
#define INK_ARENA_BLOCK_SIZE (8192u)

int ink_compile(struct ink_story *story, const struct ink_load_opts *opts)
{
    int rc = -1;
    struct ink_arena arena;
    struct ink_ast ast;

    assert(opts);
    assert(opts->source_bytes != NULL);

    ink_arena_init(&arena, INK_ARENA_BLOCK_SIZE, INK_ARENA_ALIGNMENT);

    rc = ink_parse(opts->source_bytes, opts->source_length, opts->filename,
                   &arena, &ast, opts->flags);
    if (rc < 0) {
        goto out;
    }
    if (opts->flags & INK_F_DUMP_AST) {
        ink_ast_print(&ast, opts->flags & INK_F_COLOR);
    }

    rc = ink_astgen(&ast, story, opts->flags);
    if (rc < 0) {
        goto out;
    }
    if (opts->flags & INK_F_DUMP_CODE) {
        ink_story_dump(story);
    }
out:
    ink_ast_deinit(&ast);
    ink_arena_release(&arena);
    return rc;
}
