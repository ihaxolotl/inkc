#include <assert.h>
#include <stdbool.h>

#include "lex.h"
#include "source.h"
#include "util.h"

enum ink_lex_state {
    INK_LEX_START,
    INK_LEX_CONTENT,
    INK_LEX_DIGIT,
    INK_LEX_SLASH,
    INK_LEX_COMMENT_LINE,
    INK_LEX_COMMENT_BLOCK,
    INK_LEX_COMMENT_BLOCK_STAR,
    INK_LEX_WHITESPACE,
    INK_LEX_NEWLINE,
};

static void ink_scan_next(struct ink_lexer *lexer)
{
    lexer->cursor_offset++;
}

void ink_token_next(struct ink_lexer *lexer, struct ink_token *token)
{
    unsigned char c;
    enum ink_lex_state state = INK_LEX_START;
    const struct ink_source *source = lexer->source;

    for (;;) {
        c = source->bytes[lexer->cursor_offset];

        if (lexer->cursor_offset >= source->length) {
            token->type = INK_TT_EOF;
            break;
        }
        switch (state) {
        case INK_LEX_START: {
            lexer->start_offset = lexer->cursor_offset;

            switch (c) {
            case '\0': {
                token->type = INK_TT_EOF;
                goto exit_loop;
            }
            case '\n': {
                state = INK_LEX_NEWLINE;
                break;
            }
            case ' ':
            case '\t': {
                state = INK_LEX_WHITESPACE;
                break;
            }
            case '"': {
                token->type = INK_TT_DOUBLE_QUOTE;
                ink_scan_next(lexer);
                goto exit_loop;
            }
            case '=': {
                token->type = INK_TT_EQUAL;
                ink_scan_next(lexer);
                goto exit_loop;
            }
            case '+': {
                token->type = INK_TT_PLUS;
                ink_scan_next(lexer);
                goto exit_loop;
            }
            case '-': {
                token->type = INK_TT_MINUS;
                ink_scan_next(lexer);
                goto exit_loop;
            }
            case '*': {
                token->type = INK_TT_STAR;
                ink_scan_next(lexer);
                goto exit_loop;
            }
            case '/': {
                state = INK_LEX_SLASH;
                break;
            }
            case '%': {
                token->type = INK_TT_PERCENT;
                ink_scan_next(lexer);
                goto exit_loop;
            }
            case '|': {
                token->type = INK_TT_PIPE;
                ink_scan_next(lexer);
                goto exit_loop;
            }
            case '(': {
                token->type = INK_TT_LEFT_PAREN;
                ink_scan_next(lexer);
                goto exit_loop;
            }
            case ')': {
                token->type = INK_TT_RIGHT_PAREN;
                ink_scan_next(lexer);
                goto exit_loop;
            }
            case '[': {
                token->type = INK_TT_LEFT_BRACKET;
                ink_scan_next(lexer);
                goto exit_loop;
            }
            case ']': {
                token->type = INK_TT_RIGHT_BRACKET;
                ink_scan_next(lexer);
                goto exit_loop;
            }
            case '{': {
                token->type = INK_TT_LEFT_BRACE;
                ink_scan_next(lexer);
                goto exit_loop;
            }
            case '}': {
                token->type = INK_TT_RIGHT_BRACE;
                ink_scan_next(lexer);
                goto exit_loop;
            }
            default:
                if (ink_is_alpha(c)) {
                    state = INK_LEX_CONTENT;
                } else if (ink_is_digit(c)) {
                    state = INK_LEX_DIGIT;
                } else {
                    token->type = INK_TT_STRING;
                    ink_scan_next(lexer);
                    goto exit_loop;
                }
                break;
            }
            break;
        }
        case INK_LEX_CONTENT: {
            if (!ink_is_alpha(c) && !ink_is_digit(c)) {
                token->type = INK_TT_STRING;
                goto exit_loop;
            }
            break;
        }
        case INK_LEX_DIGIT: {
            if (!ink_is_digit(c)) {
                token->type = INK_TT_NUMBER;
                goto exit_loop;
            }
            break;
        }
        case INK_LEX_SLASH: {
            switch (c) {
            case '/': {
                state = INK_LEX_COMMENT_LINE;
                break;
            }
            case '*': {
                state = INK_LEX_COMMENT_BLOCK;
                break;
            }
            default:
                token->type = INK_TT_SLASH;
                goto exit_loop;
            }
            break;
        }
        case INK_LEX_COMMENT_LINE: {
            switch (c) {
            case '\0': {
                state = INK_LEX_START;
                break;
            }
            case '\n': {
                state = INK_LEX_START;
                lexer->is_line_start = true;
                break;
            }
            default:
                break;
            }
            break;
        }
        case INK_LEX_COMMENT_BLOCK: {
            switch (c) {
            case '\0':
                token->type = INK_TT_ERROR;
                goto exit_loop;
            case '*':
                state = INK_LEX_COMMENT_BLOCK_STAR;
                break;
            default:
                break;
            }
            break;
        }
        case INK_LEX_COMMENT_BLOCK_STAR: {
            switch (c) {
            case '\0':
                token->type = INK_TT_ERROR;
                goto exit_loop;
            case '/':
                state = INK_LEX_START;
                break;
            default:
                state = INK_LEX_COMMENT_BLOCK;
                break;
            }
            break;
        }
        case INK_LEX_WHITESPACE: {
            switch (c) {
            case ' ':
            case '\t':
                break;
            default:
                if (lexer->is_line_start) {
                    state = INK_LEX_START;
                    break;
                }
                token->type = INK_TT_WHITESPACE;
                goto exit_loop;
            }
            break;
        }
        case INK_LEX_NEWLINE: {
            if (c != '\n') {
                if (lexer->is_line_start) {
                    lexer->is_line_start = false;
                    state = INK_LEX_START;
                    continue;
                } else {
                    token->type = INK_TT_NL;
                    goto exit_loop;
                }
            }
            break;
        }
        default:
            /* Unreachable */
            break;
        }

        ink_scan_next(lexer);
    }
exit_loop:
    if (lexer->is_line_start) {
        lexer->is_line_start = false;
    }

    token->start_offset = lexer->start_offset;
    token->end_offset = lexer->cursor_offset;
}
