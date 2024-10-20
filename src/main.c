#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "parse.h"
#include "source.h"

int main(int argc, char *argv[])
{
    int rc;
    const char *filename;
    struct ink_source source;
    struct ink_syntax_node *tree;

    if (argc < 2) {
        fprintf(stderr, "Not enough arguments.\n");
        return EXIT_FAILURE;
    }

    filename = argv[1];

    rc = ink_source_load(filename, &source);
    if (rc < 0) {
        switch (-rc) {
        case INK_E_OS: {
            fprintf(stderr, "[ERROR] OS Error.\n");
            break;
        }
        case INK_E_FILE: {
            fprintf(stderr, "[ERROR] %s is not an ink script.\n", filename);
            break;
        }
        default:
            fprintf(stderr, "[ERROR] Unknown error.\n");
            break;
        }
        return EXIT_FAILURE;
    }

    printf("Source file %s is %zu bytes\n", source.filename, source.length);
    ink_parse(&source, &tree);
    ink_syntax_node_print(tree);
    ink_source_free(&source);
    return EXIT_SUCCESS;
}
