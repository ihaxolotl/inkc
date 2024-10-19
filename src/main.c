#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "lex.h"
#include "source.h"

void ink_token_print(const struct ink_token *token)
{
    printf("[DEBUG] %s(%u, %u)\n", ink_token_type_strz(token->type),
           token->start_offset, token->end_offset);
}

int main(int argc, char *argv[])
{
    int rc;
    const char *filename;
    struct ink_source source;
    struct ink_token token;
    struct ink_lexer lexer = {
        .source = &source,
        .start_offset = 0,
        .cursor_offset = 0,
    };

    if (argc < 2) {
        fprintf(stderr, "Not enough arguments.\n");
        return EXIT_FAILURE;
    }

    filename = argv[1];

    rc = ink_source_load(filename, &source);
    if (rc < 0) {
        switch (-rc) {
        case INK_E_OS:
            fprintf(stderr, "[ERROR] OS Error.\n");
            break;
        case INK_E_FILE:
            fprintf(stderr, "[ERROR] %s is not an ink script.\n", filename);
            break;
        default:
            fprintf(stderr, "[ERROR] Unknown error.\n");
            break;
        }
        return EXIT_FAILURE;
    }

    printf("Source file %s is %zu bytes\n", source.filename, source.length);

    do {
        ink_token_next(&lexer, &token);
        ink_token_print(&token);
    } while (token.type != INK_TT_EOF);

    ink_source_free(&source);
    return EXIT_SUCCESS;
}
