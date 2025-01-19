#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "scanner.h"
#include "source.h"
#include "token.h"

enum ink_lex_state {
    INK_LEX_START,
    INK_LEX_MINUS,
    INK_LEX_SLASH,
    INK_LEX_EQUAL,
    INK_LEX_BANG,
    INK_LEX_LESS_THAN,
    INK_LEX_GREATER_THAN,
    INK_LEX_WORD,
    INK_LEX_IDENTIFIER,
    INK_LEX_NUMBER,
    INK_LEX_NUMBER_DOT,
    INK_LEX_NUMBER_DECIMAL,
    INK_LEX_WHITESPACE,
    INK_LEX_COMMENT_LINE,
    INK_LEX_COMMENT_BLOCK,
    INK_LEX_COMMENT_BLOCK_STAR,
    INK_LEX_NEWLINE,
};

static const char *INK_GRAMMAR_TYPE_STR[] = {
    [INK_GRAMMAR_CONTENT] = "Content",
    [INK_GRAMMAR_EXPRESSION] = "Expression",
};

const char *ink_grammar_type_strz(enum ink_grammar_type type)
{
    return INK_GRAMMAR_TYPE_STR[type];
}

static inline bool ink_is_alpha(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline bool ink_is_digit(unsigned char c)
{
    return (c >= '0' && c <= '9');
}

static inline bool ink_is_identifier(unsigned char c)
{
    return ink_is_alpha(c) || ink_is_digit(c) || c == '_';
}

struct ink_scanner_mode *ink_scanner_current(struct ink_scanner *scanner)
{
    return &scanner->mode_stack[scanner->mode_depth];
}

void ink_scanner_push(struct ink_scanner *scanner, enum ink_grammar_type type,
                      size_t source_offset)
{
    struct ink_scanner_mode *mode;

    assert(scanner->mode_depth < INK_SCANNER_DEPTH_MAX);

    scanner->mode_depth++;

    mode = &scanner->mode_stack[scanner->mode_depth];
    mode->type = type;
    mode->source_offset = source_offset;
}

void ink_scanner_pop(struct ink_scanner *scanner)
{
    assert(scanner->mode_depth != 0);
    scanner->mode_depth--;
}

/**
 * Rewind the parser's state to a previous token index.
 */
void ink_scanner_rewind(struct ink_scanner *scanner, size_t source_offset)
{
    assert(source_offset <= scanner->cursor_offset);

    scanner->start_offset = source_offset;
    scanner->cursor_offset = source_offset;
}

static enum ink_token_type ink_scanner_keyword(struct ink_scanner *scanner,
                                               enum ink_token_type type,
                                               size_t start_offset,
                                               size_t end_offset)
{
    const unsigned char *source = scanner->source->bytes;
    const unsigned char *lexeme = source + start_offset;
    const size_t length = end_offset - start_offset;

    switch (length) {
    case 2: {
        if (memcmp(lexeme, "or", length) == 0) {
            type = INK_TT_KEYWORD_OR;
        }
        break;
    }
    case 3: {
        if (memcmp(lexeme, "and", length) == 0) {
            type = INK_TT_KEYWORD_AND;
        } else if (memcmp(lexeme, "mod", length) == 0) {
            type = INK_TT_KEYWORD_MOD;
        } else if (memcmp(lexeme, "not", length) == 0) {
            type = INK_TT_KEYWORD_NOT;
        } else if (memcmp(lexeme, "ref", length) == 0) {
            type = INK_TT_KEYWORD_REF;
        } else if (memcmp(lexeme, "VAR", length) == 0) {
            type = INK_TT_KEYWORD_VAR;
        }
        break;
    }
    case 4: {
        if (memcmp(lexeme, "else", length) == 0) {
            type = INK_TT_KEYWORD_ELSE;
        } else if (memcmp(lexeme, "temp", length) == 0) {
            type = INK_TT_KEYWORD_TEMP;
        } else if (memcmp(lexeme, "true", length) == 0) {
            type = INK_TT_KEYWORD_TRUE;
        } else if (memcmp(lexeme, "LIST", length) == 0) {
            type = INK_TT_KEYWORD_LIST;
        }
        break;
    }
    case 5: {
        if (memcmp(lexeme, "false", length) == 0) {
            type = INK_TT_KEYWORD_FALSE;
        } else if (memcmp(lexeme, "CONST", length) == 0) {
            type = INK_TT_KEYWORD_CONST;
        }
        break;
    }
    case 6: {
        if (memcmp(lexeme, "return", length) == 0) {
            type = INK_TT_KEYWORD_RETURN;
        }
        break;
    }
    case 8: {
        if (memcmp(lexeme, "function", length) == 0) {
            type = INK_TT_KEYWORD_FUNCTION;
        }
        break;
    }
    default:
        break;
    }
    return type;
}

/**
 * Return a token type representing a keyword that the specified token
 * represents. If the token's type does not correspond to a keyword,
 * the token's type will be returned instead.
 *
 * TODO(Brett): Perhaps we could add an early return to ignore any
 * UTF-8 encoded byte sequence, as no reserved words contain such things.
 */
static enum ink_token_type
ink_scanner_keyword_from_token(struct ink_scanner *scanner,
                               const struct ink_token *token)
{
    return ink_scanner_keyword(scanner, token->type, token->start_offset,
                               token->end_offset);
}

/**
 * Try to recognize a token as a keyword.
 *
 * If true, the specified token will have its type modified.
 */
bool ink_scanner_try_keyword(struct ink_scanner *scanner,
                             struct ink_token *token, enum ink_token_type type)
{
    const enum ink_token_type keyword_type =
        ink_scanner_keyword_from_token(scanner, token);

    if (keyword_type == type) {
        token->type = type;
        return true;
    }
    return false;
}

void ink_scanner_next(struct ink_scanner *scanner, struct ink_token *token)
{
    unsigned char c;
    enum ink_lex_state state = INK_LEX_START;
    const struct ink_source *source = scanner->source;
    const struct ink_scanner_mode *mode = ink_scanner_current(scanner);

    for (;;) {
        c = source->bytes[scanner->cursor_offset];

        if (scanner->cursor_offset >= source->length) {
            token->type = INK_TT_EOF;
            break;
        }
        switch (state) {
        case INK_LEX_START: {
            scanner->start_offset = scanner->cursor_offset;

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
            case '~': {
                token->type = INK_TT_TILDE;
                scanner->cursor_offset++;
                goto exit_loop;
            }
            case ':': {
                token->type = INK_TT_COLON;
                scanner->cursor_offset++;
                goto exit_loop;
            }
            case '!': {
                state = INK_LEX_BANG;
                break;
            }
            case '"': {
                token->type = INK_TT_DOUBLE_QUOTE;
                scanner->cursor_offset++;
                goto exit_loop;
            }
            case '=': {
                state = INK_LEX_EQUAL;
                break;
            }
            case '+': {
                token->type = INK_TT_PLUS;
                scanner->cursor_offset++;
                goto exit_loop;
            }
            case '-': {
                state = INK_LEX_MINUS;
                break;
            }
            case '*': {
                token->type = INK_TT_STAR;
                scanner->cursor_offset++;
                goto exit_loop;
            }
            case '/': {
                state = INK_LEX_SLASH;
                break;
            }
            case ',': {
                token->type = INK_TT_COMMA;
                scanner->cursor_offset++;
                goto exit_loop;
            }
            case '%': {
                token->type = INK_TT_PERCENT;
                scanner->cursor_offset++;
                goto exit_loop;
            }
            case '?': {
                token->type = INK_TT_QUESTION;
                scanner->cursor_offset++;
                goto exit_loop;
            }
            case '|': {
                token->type = INK_TT_PIPE;
                scanner->cursor_offset++;
                goto exit_loop;
            }
            case '(': {
                token->type = INK_TT_LEFT_PAREN;
                scanner->cursor_offset++;
                goto exit_loop;
            }
            case ')': {
                token->type = INK_TT_RIGHT_PAREN;
                scanner->cursor_offset++;
                goto exit_loop;
            }
            case '<': {
                state = INK_LEX_LESS_THAN;
                break;
            }
            case '>': {
                state = INK_LEX_GREATER_THAN;
                break;
            }
            case '[': {
                token->type = INK_TT_LEFT_BRACKET;
                scanner->cursor_offset++;
                goto exit_loop;
            }
            case ']': {
                token->type = INK_TT_RIGHT_BRACKET;
                scanner->cursor_offset++;
                goto exit_loop;
            }
            case '{': {
                token->type = INK_TT_LEFT_BRACE;
                scanner->cursor_offset++;
                goto exit_loop;
            }
            case '}': {
                token->type = INK_TT_RIGHT_BRACE;
                scanner->cursor_offset++;
                goto exit_loop;
            }
            default:
                if (mode->type == INK_GRAMMAR_EXPRESSION) {
                    if (ink_is_alpha(c)) {
                        state = INK_LEX_IDENTIFIER;
                    } else if (ink_is_digit(c)) {
                        state = INK_LEX_NUMBER;
                    } else {
                        token->type = INK_TT_ERROR;
                        scanner->cursor_offset++;
                        goto exit_loop;
                    }
                } else {
                    state = INK_LEX_WORD;
                }
                break;
            }
            break;
        }
        case INK_LEX_MINUS: {
            if (c == '>') {
                token->type = INK_TT_RIGHT_ARROW;
                scanner->cursor_offset++;
                goto exit_loop;
            }

            token->type = INK_TT_MINUS;
            goto exit_loop;
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
        case INK_LEX_EQUAL: {
            if (mode->type == INK_GRAMMAR_EXPRESSION) {
                if (c == '=') {
                    token->type = INK_TT_EQUAL_EQUAL;
                    scanner->cursor_offset++;
                    goto exit_loop;
                }
            }

            token->type = INK_TT_EQUAL;
            goto exit_loop;
        }
        case INK_LEX_BANG: {
            if (c == '=') {
                token->type = INK_TT_BANG_EQUAL;
                scanner->cursor_offset++;
                goto exit_loop;
            }

            token->type = INK_TT_BANG;
            goto exit_loop;
        }
        case INK_LEX_LESS_THAN: {
            switch (c) {
            case '=': {
                token->type = INK_TT_LESS_EQUAL;
                scanner->cursor_offset++;
                goto exit_loop;
            }
            case '-': {
                token->type = INK_TT_LEFT_ARROW;
                scanner->cursor_offset++;
                goto exit_loop;
            }
            default:
                token->type = INK_TT_LESS_THAN;
                goto exit_loop;
            }
        }
        case INK_LEX_GREATER_THAN: {
            if (c == '=') {
                token->type = INK_TT_GREATER_EQUAL;
                scanner->cursor_offset++;
                goto exit_loop;
            }

            token->type = INK_TT_GREATER_THAN;
            goto exit_loop;
        }
        case INK_LEX_WORD: {
            if (!ink_is_identifier(c)) {
                token->type = INK_TT_STRING;
                goto exit_loop;
            }
            break;
        }
        case INK_LEX_NUMBER: {
            if (c == '.') {
                state = INK_LEX_NUMBER_DOT;
            } else {
                if (ink_is_alpha(c) || c == '_') {
                    state = INK_LEX_IDENTIFIER;
                } else if (!ink_is_digit(c)) {
                    token->type = INK_TT_NUMBER;
                    goto exit_loop;
                }
            }
            break;
        }
        case INK_LEX_NUMBER_DOT: {
            if (!ink_is_digit(c)) {
                token->type = INK_TT_ERROR;
                goto exit_loop;
            } else {
                state = INK_LEX_NUMBER_DECIMAL;
            }
            break;
        }
        case INK_LEX_NUMBER_DECIMAL: {
            if (!ink_is_digit(c)) {
                token->type = INK_TT_NUMBER;
                goto exit_loop;
            }
            break;
        }
        case INK_LEX_IDENTIFIER: {
            if (!ink_is_identifier(c)) {
                if (mode->type == INK_GRAMMAR_EXPRESSION) {
                    token->type = ink_scanner_keyword(
                        scanner, INK_TT_IDENTIFIER, scanner->start_offset,
                        scanner->cursor_offset);
                } else {
                    token->type = INK_TT_IDENTIFIER;
                }
                goto exit_loop;
            }
            break;
        }
        case INK_LEX_WHITESPACE: {
            switch (c) {
            case ' ':
            case '\t':
                break;
            default:
                if (scanner->is_line_start ||
                    mode->type == INK_GRAMMAR_EXPRESSION) {
                    state = INK_LEX_START;
                    continue;
                }

                token->type = INK_TT_WHITESPACE;
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
                scanner->is_line_start = true;
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
        case INK_LEX_NEWLINE: {
            if (c != '\n') {
                if (scanner->is_line_start) {
                    scanner->is_line_start = false;
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

        scanner->cursor_offset++;
    }
exit_loop:
    if (scanner->is_line_start) {
        scanner->is_line_start = false;
    }

    token->start_offset = scanner->start_offset;
    token->end_offset = scanner->cursor_offset;
}
