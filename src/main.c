#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "arena.h"
#include "common.h"
#include "logging.h"
#include "parse.h"
#include "source.h"
#include "tree.h"

enum {
    OPT_TRACING = 1000,
    OPT_CACHING,
    OPT_DUMP_AST,
};

static const struct option LONGOPTS[] = {
    {"help", no_argument, NULL, 'h'},
    {"tracing", optional_argument, NULL, OPT_TRACING},
    {"caching", optional_argument, NULL, OPT_CACHING},
    {"dump-ast", optional_argument, NULL, OPT_DUMP_AST},
    {NULL, 0, NULL, 0},
};

static const char *USAGE_MSG =
    "Usage: %s [OPTION]... [FILE]\n"
    "Load and execute an Ink story.\n\n"
    "  -h, --help       Print this message.\n"
    "  --tracing        Enable tracing.\n"
    "  --caching        Enable cachine.\n"
    "  --dump-ast       Dump the parsed file's AST\n";

static void print_usage(const char *name)
{
    fprintf(stderr, USAGE_MSG, name);
}

int main(int argc, char *argv[])
{
    static const size_t arena_alignment = 8;
    static const size_t arena_block_size = 8192;
    const char *filename;
    struct ink_arena arena;
    struct ink_source source;
    struct ink_syntax_tree syntax_tree;
    int opti, rc;
    int flags = 0;
    int opt = 0;
    bool dump_ast = false;

    while ((opt = getopt_long(argc, argv, "", LONGOPTS, &opti)) != -1) {
        switch (opt) {
        case OPT_TRACING: {
            flags |= INK_PARSER_F_TRACING;
            break;
        }
        case OPT_CACHING: {
            flags |= INK_PARSER_F_CACHING;
            break;
        }
        case OPT_DUMP_AST: {
            dump_ast = true;
            break;
        }
        case '?': {
            fprintf(stderr, "Unknown option `%c'.\n", optopt);
            break;
        }
        case ':': {
            fprintf(stderr, "Option -%c requires an argument.\n", optopt);
            break;
        }
        default:
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    filename = argv[optind];

    if (filename == NULL || *filename == '\0') {
        rc = ink_source_load_stdin(&source);
    } else {
        rc = ink_source_load(filename, &source);
    }
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
    }

    ink_arena_initialize(&arena, arena_block_size, arena_alignment);

    rc = ink_syntax_tree_initialize(&source, &syntax_tree);
    if (rc < 0)
        goto cleanup;

    ink_parse(&arena, &source, &syntax_tree, flags);

    if (dump_ast)
        ink_syntax_tree_print(&syntax_tree);
cleanup:
    ink_syntax_tree_cleanup(&syntax_tree);
    ink_arena_release(&arena);
    ink_source_free(&source);

    return EXIT_SUCCESS;
}
