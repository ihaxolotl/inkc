#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "astgen.h"
#include "compile.h"
#include "parse.h"
#include "story.h"

#define INK_ARENA_ALIGNMENT (8u)
#define INK_ARENA_BLOCK_SIZE (8192u)

int ink_compile(struct ink_story *story, const struct ink_load_opts *opts)
{
    int rc = -1;
    struct ink_arena arena;
    struct ink_ast ast;
    uint8_t *buf = NULL;

    ink_arena_init(&arena, INK_ARENA_BLOCK_SIZE, INK_ARENA_ALIGNMENT);

    buf = ink_malloc(opts->source_length + 1);
    if (!buf) {
        return -INK_E_PANIC;
    }

    memcpy(buf, opts->source_bytes, opts->source_length);
    buf[opts->source_length] = '\0';

    rc = ink_parse(buf, opts->source_length, opts->filename, &arena, &ast,
                   opts->flags);
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
    ink_free(buf);
    ink_ast_deinit(&ast);
    ink_arena_release(&arena);
    return rc;
}
