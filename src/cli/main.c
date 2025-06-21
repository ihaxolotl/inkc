#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <ink/ink.h>

#include "cli/option.h"
#include "common.h"
#include "logging.h"
#include "object.h"
#include "source.h"

enum {
    OPT_COLORS = 1000,
    OPT_COMPILE_ONLY,
    OPT_VM_TRACING,
    OPT_GC_TRACING,
    OPT_CACHING,
    OPT_DUMP_AST,
    OPT_DUMP_STORY,
    OPT_HELP,
};

static const struct option opts[] = {
    {"--colors", OPT_COLORS, false},
    {"--compile-only", OPT_COMPILE_ONLY, false},
    {"--dump-ast", OPT_DUMP_AST, false},
    {"--dump-story", OPT_DUMP_STORY, false},
    {"--trace", OPT_VM_TRACING, false},
    {"--trace-gc", OPT_GC_TRACING, false},
    {"--help", OPT_HELP, false},
    {"-h", OPT_HELP, false},
    (struct option){0},
};

static const char *USAGE_MSG =
    "Usage: %s [OPTION]... [FILE]\n"
    "Load and execute an Ink story.\n\n"
    "  -h, --help       Print this message\n"
    "  --colors         Enable color output\n"
    "  --compile-only   Compile the story without executing\n"
    "  --dump-ast       Dump a source file's AST\n"
    "  --dump-story     Dump a story's bytecode\n"
    "  --trace          Enable execution tracing\n"
    "  --trace-gc       Enable garbage collector tracing\n"
    "\n";

static void print_usage(const char *name)
{
    fprintf(stderr, USAGE_MSG, name);
}

static void inkc_render_error(const char *filename, int rc)
{
    switch (-rc) {
    case INK_E_OK:
        break;
    case INK_E_PANIC:
        ink_error("Panic!");
        break;
    case INK_E_OOM:
        ink_error("Out of memory!");
        break;
    case INK_E_OS:
        ink_error("Could not open file `%s`. OS Error.", filename);
        break;
    case INK_E_FILE:
        ink_error("Could not open file `%s`. Not an ink script.", filename);
        break;
    case INK_E_OVERWRITE:
        ink_error("Something was overwritten.");
        break;
    case INK_E_INVALID_OPTION:
        ink_error("Invalid option.");
        break;
    case INK_E_INVALID_INST:
        ink_error("Invalid instruction.");
        break;
    case INK_E_INVALID_ARG:
        ink_error("Invalid argument.");
        break;
    case INK_E_STACK_OVERFLOW:
        ink_error("Stack overflow.");
        break;
    default:
        ink_error("Unknown error.");
        break;
    }
}

int main(int argc, char *argv[])
{
    struct ink_source source;
    struct ink_choice choice;
    bool compile_only = false;
    int flags = INK_F_GC_ENABLE | INK_F_GC_STRESS;
    int opt = 0;
    int rc = -1;
    size_t choice_index = 0;
    size_t linelen = 0;
    uint8_t *line = NULL;
    const char *filename = NULL;
    struct ink_story *story = NULL;

    option_setopts(opts, argv);

    while ((opt = option_nextopt())) {
        switch (opt) {
        case OPT_COLORS:
            flags |= INK_F_COLOR;
            break;
        case OPT_VM_TRACING:
            flags |= INK_F_VM_TRACING;
            break;
        case OPT_GC_TRACING:
            flags |= INK_F_GC_TRACING;
            break;
        case OPT_CACHING:
            /* flags |= INK_F_CACHING; */
            break;
        case OPT_DUMP_AST:
            flags |= INK_F_DUMP_AST;
            break;
        case OPT_DUMP_STORY:
            flags |= INK_F_DUMP_CODE;
            break;
        case OPT_COMPILE_ONLY:
            compile_only = true;
            break;
        case OPTION_UNKNOWN:
            fprintf(stderr, "Unrecognised option %s.\n\n", option_unknown_opt);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        case OPT_HELP:
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        case OPTION_OPERAND:
            filename = option_operand;
            break;
        default:
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }
    if (filename == NULL || *filename == '\0') {
        rc = ink_source_load_stdin(&source);
        filename = "<STDIN>";
    } else {
        rc = ink_source_load(filename, &source);
    }
    if (rc < 0) {
        inkc_render_error(filename, rc);
        return rc;
    }

    story = ink_open();
    if (!story) {
        goto out;
    }

    const struct ink_load_opts opts = {
        .source_bytes = source.bytes,
        .source_length = source.length,
        .filename = (uint8_t *)filename,
        .flags = flags,
    };

    rc = ink_story_load_opts(story, &opts);
    if (rc < 0) {
        /* TODO(Brett): Returning EXIT_FAILURE breaks llvm-lit. Look for a
         * workaround */
        goto out;
    }
    if (!compile_only) {
        while (ink_story_can_continue(story)) {
            line = NULL;
            linelen = 0;
            choice_index = 0;

            if (ink_story_continue(story, &line, &linelen) < 0) {
                goto out;
            }
            if (line) {
                printf("%.*s\n", (int)linelen, line);
            }
            while (ink_story_choice_next(story, &choice) >= 0) {
                printf("%zu: %.*s\n", choice_index + 1, (int)choice.length,
                       choice.bytes);
            }

            printf("> ");
            scanf("%zu", &choice_index);
            ink_story_choose(story, choice_index);
        }
    }
    if (rc < 0) {
        inkc_render_error(filename, rc);
    }
out:
    ink_close(story);
    ink_source_free(&source);
    return EXIT_SUCCESS;
}
