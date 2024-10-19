#include <stdbool.h>
#include <stdio.h>

#include "lex.h"
#include "source.h"

enum ink_lex_state {
    INK_LEX_STATE_START,
    INK_LEX_STATE_CONTENT,
};

static bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

const char *ink_token_type_strz(enum ink_token_type type)
{
    const char *str;

    switch (type) {
    case INK_TT_EOF: {
        str = "TT_EOF";
        break;
    }
    case INK_TT_NL: {
        str = "TT_NL";
        break;
    }
    case INK_TT_STRING: {
        str = "TT_STRING";
        break;
    }
    case INK_TT_ERR: {
        str = "TT_ERR";
        break;
    }
    default:
        str = "<invalid>";
        break;
    }
    return str;
}

void ink_token_print(struct ink_source *source, const struct ink_token *token)
{
    const unsigned int start = token->start_offset;
    const unsigned int end = token->end_offset;

    switch (token->type) {
    case INK_TT_EOF:
        printf("[DEBUG] %s(%u, %u): `\\0`\n", ink_token_type_strz(token->type),
               start, end);
        break;
    case INK_TT_NL:
        printf("[DEBUG] %s(%u, %u): `\\n`\n", ink_token_type_strz(token->type),
               start, end);
        break;
    default:
        printf("[DEBUG] %s(%u, %u): `%.*s`\n", ink_token_type_strz(token->type),
               start, end, (int)(end - start), source->bytes + start);
        break;
    }
}

void ink_token_next(struct ink_lexer *lexer, struct ink_token *token)
{
    unsigned char c;
    enum ink_lex_state state = INK_LEX_STATE_START;
    const struct ink_source *source = lexer->source;

    for (;;) {
        c = source->bytes[lexer->cursor_offset];

        if (lexer->cursor_offset >= source->length) {
            token->type = INK_TT_EOF;
            break;
        }
        switch (state) {
        case INK_LEX_STATE_START: {
            lexer->start_offset = lexer->cursor_offset;

            switch (c) {
            case '\0': {
                token->type = INK_TT_EOF;
                goto exit_loop;
            }
            case '\n': {
                token->type = INK_TT_NL;
                lexer->cursor_offset++;
                goto exit_loop;
            }
            default:
                if (is_alpha(c)) {
                    state = INK_LEX_STATE_CONTENT;
                } else {
                    token->type = INK_TT_STRING;
                    lexer->cursor_offset++;
                    goto exit_loop;
                }
            }
        } break;
        case INK_LEX_STATE_CONTENT: {
            if (c == '\0' || c == '\n' || !is_alpha(c)) {
                token->type = INK_TT_STRING;
                goto exit_loop;
            }
            break;
        }
        default:
            /* Unreachable */
            break;
        }

        lexer->cursor_offset++;
    }

exit_loop:
    token->start_offset = lexer->start_offset;
    token->end_offset = lexer->cursor_offset;
}
