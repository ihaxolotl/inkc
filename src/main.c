#include <stdio.h>
#include <stdlib.h>

#include "arena.h"
#include "common.h"
#include "logging.h"
#include "parse.h"
#include "source.h"
#include "tree.h"

int main(int argc, char *argv[])
{
    static const size_t arena_alignment = 8;
    static const size_t arena_block_size = 8192;
    int rc;
    const char *filename;
    struct ink_arena arena;
    struct ink_source source;
    struct ink_syntax_tree syntax_tree;

    if (argc < 2) {
        ink_error("Not enough arguments.");
        return EXIT_FAILURE;
    }

    filename = argv[1];

    rc = ink_source_load(filename, &source);
    if (rc < 0) {
        switch (-rc) {
        case INK_E_OS: {
            ink_error("[ERROR] Could not open file `%s`. OS Error.", filename);
            break;
        }
        case INK_E_FILE: {
            ink_error("[ERROR] Could not open file `%s`. Not an ink script.",
                      filename);
            break;
        }
        default:
            ink_error("[ERROR] Unknown error.");
            break;
        }
        return EXIT_FAILURE;
    }

    ink_arena_initialize(&arena, arena_block_size, arena_alignment);

    rc = ink_syntax_tree_initialize(&source, &syntax_tree);
    if (rc < 0)
        goto cleanup;

    ink_parse(&arena, &source, &syntax_tree);
    ink_token_buffer_print(&source, &syntax_tree.tokens);
    ink_syntax_tree_print(&syntax_tree);
cleanup:
    ink_syntax_tree_cleanup(&syntax_tree);
    ink_arena_release(&arena);
    ink_source_free(&source);

    return EXIT_SUCCESS;
}
