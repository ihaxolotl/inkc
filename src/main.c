#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "logging.h"
#include "option.h"
#include "source.h"
#include "story.h"

enum {
    OPT_COLORS = 1000,
    OPT_TRACING,
    OPT_CACHING,
    OPT_DUMP_AST,
    OPT_HELP,
    OPT_ARG_EXAMPLE
};

static const struct option opts[] = {
    {"--colors", OPT_COLORS, false},
    {"--tracing", OPT_TRACING, false},
    {"--caching", OPT_CACHING, false},
    {"--dump-ast", OPT_DUMP_AST, false},
    {"--help", OPT_HELP, false},
    {"-h", OPT_HELP, false},
    {"--arg-example", OPT_ARG_EXAMPLE, true},
    (struct option){0},
};

static const char *USAGE_MSG = "Usage: %s [OPTION]... [FILE]\n"
                               "Load and execute an Ink story.\n\n"
                               "  -h, --help       Print this message\n"
                               "  --colors         Enable color output\n"
                               "  --tracing        Enable tracing\n"
                               "  --caching        Enable caching\n"
                               "  --dump-ast       Dump a source file's AST\n";

static void print_usage(const char *name)
{
    fprintf(stderr, USAGE_MSG, name);
}

int main(int argc, char *argv[])
{
    struct ink_source source;
    struct ink_story story;
    int flags = 0;
    int opt = 0;
    int rc = -1;
    const char *filename = 0;

    option_setopts(opts, argv);

    while ((opt = option_nextopt())) {
        switch (opt) {
        case OPT_COLORS: {
            flags |= INK_F_COLOR;
            break;
        }
        case OPT_TRACING: {
            flags |= INK_F_TRACING;
            break;
        }
        case OPT_CACHING: {
            flags |= INK_F_CACHING;
            break;
        }
        case OPT_DUMP_AST: {
            flags |= INK_F_DUMP_AST;
            break;
        }
        case OPTION_UNKNOWN: {
            fprintf(stderr, "Unrecognised option %s.\n\n", option_unknown_opt);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
        case OPT_ARG_EXAMPLE: {
            char *arg;
            while (*(arg = option_nextarg())) {
                printf("--arg-example arg : %s\n", arg);
            }
            break;
        }
        case OPT_HELP: {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        case OPTION_OPERAND: {
            filename = option_operand;
            break;
        }
        default:
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }
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
        return rc;
    }

    rc = ink_story_load(&story, (char *)source.bytes, flags);
    if (rc < 0) {
        ink_source_free(&source);
        return EXIT_FAILURE;
    }
    while (story.can_continue) {
        char *content = ink_story_continue(&story);

        printf("%s\n", content);
        ink_free(content);
    }

    ink_story_free(&story);
    ink_source_free(&source);
    return EXIT_SUCCESS;
}
