#include "lex.h"

const char *ink_token_type_strz(enum ink_token_type type)
{
    const char *str;

    switch (type) {
    case INK_TT_EOF:
        str = "TT_EOF";
        break;
    case INK_TT_NL:
        str = "TT_NL";
        break;
    case INK_TT_ERR:
        str = "TT_ERR";
        break;
    default:
        str = "<invalid>";
        break;
    }
    return str;
}

void ink_token_next(struct ink_lexer *lexer, struct ink_token *token)
{
    const struct ink_source *source = lexer->source;

    lexer->start_offset = lexer->cursor_offset;

    for (;;) {
        const unsigned char c = source->bytes[lexer->cursor_offset++];

        if (lexer->cursor_offset >= source->length) {
            token->type = INK_TT_EOF;
            break;
        }
        switch (c) {
        case '\0':
            token->type = INK_TT_EOF;
            goto exit_loop;
        case '\n':
            token->type = INK_TT_NL;
            goto exit_loop;
        default:
            token->type = INK_TT_ERR;
            goto exit_loop;
        }
    }

exit_loop:
    token->start_offset = lexer->start_offset;
    token->end_offset = lexer->cursor_offset;
}
