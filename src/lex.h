#ifndef __INK_LEX_H__
#define __INK_LEX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#define INK_PARSE_DEPTH 128

struct ink_source;

#define INK_TT(T)                                                              \
    T(TT_EOF, "TT_EOF")                                                        \
    T(TT_NL, "TT_NL")                                                          \
    T(TT_AMP, "TT_AMP")                                                        \
    T(TT_AMP_AMP, "TT_AMP_AMP")                                                \
    T(TT_BANG, "TT_BANG")                                                      \
    T(TT_BANG_QUESTION, "TT_BANG_QUESTION")                                    \
    T(TT_BANG_EQUAL, "TT_BANG_EQUAL")                                          \
    T(TT_CARET, "TT_CARET")                                                    \
    T(TT_COLON, "TT_COLON")                                                    \
    T(TT_COMMA, "TT_COMMA")                                                    \
    T(TT_DOT, "TT_DOT")                                                        \
    T(TT_DOUBLE_QUOTE, "TT_DOUBLE_QUOTE")                                      \
    T(TT_EQUAL, "TT_EQUAL")                                                    \
    T(TT_EQUAL_EQUAL, "TT_EQUAL_EQUAL")                                        \
    T(TT_EQUAL_SEQ, "TT_EQUAL_SEQ")                                            \
    T(TT_GLUE, "TT_GLUE")                                                      \
    T(TT_GREATER_EQUAL, "TT_GREATER_EQUAL")                                    \
    T(TT_GREATER_THAN, "TT_GREATER_THAN")                                      \
    T(TT_LEFT_BRACE, "TT_LEFT_BRACE")                                          \
    T(TT_LEFT_BRACKET, "TT_LEFT_BRACKET")                                      \
    T(TT_LEFT_PAREN, "TT_LEFT_PAREN")                                          \
    T(TT_LESS_EQUAL, "TT_LESS_EQUAL")                                          \
    T(TT_LESS_THAN, "TT_LESS_THAN")                                            \
    T(TT_MINUS, "TT_MINUS")                                                    \
    T(TT_MINUS_EQUAL, "TT_MINUS_EQUAL")                                        \
    T(TT_MINUS_MINUS, "TT_MINUS_MINUS")                                        \
    T(TT_NUMBER, "TT_NUMBER")                                                  \
    T(TT_PERCENT, "TT_PERCENT")                                                \
    T(TT_PIPE, "TT_PIPE")                                                      \
    T(TT_PIPE_PIPE, "TT_PIPE_PIPE")                                            \
    T(TT_PLUS, "TT_PLUS")                                                      \
    T(TT_PLUS_EQUAL, "TT_PLUS_EQUAL")                                          \
    T(TT_PLUS_PLUS, "TT_PLUS_PLUS")                                            \
    T(TT_POUND, "TT_POUND")                                                    \
    T(TT_QUESTION, "TT_QUESTION")                                              \
    T(TT_RIGHT_ARROW, "TT_RIGHT_ARROW")                                        \
    T(TT_RIGHT_BRACE, "TT_RIGHT_BRACE")                                        \
    T(TT_RIGHT_BRACKET, "TT_RIGHT_BRACKET")                                    \
    T(TT_RIGHT_PAREN, "TT_RIGHT_PAREN")                                        \
    T(TT_SLASH, "TT_SLASH")                                                    \
    T(TT_STAR, "TT_STAR")                                                      \
    T(TT_STRING, "TT_STRING")                                                  \
    T(TT_TILDE, "TT_TILDE")                                                    \
    T(TT_WHITESPACE, "TT_WHITESPACE")                                          \
    T(TT_ERROR, "TT_ERROR")

#define T(name, description) INK_##name,
enum ink_token_type {
    INK_TT(T)
};
#undef T

struct ink_token {
    enum ink_token_type type;
    size_t start_offset;
    size_t end_offset;
};

struct ink_lexer {
    struct ink_source *source;
    size_t cursor_offset;
    size_t start_offset;
};

extern void ink_token_next(struct ink_lexer *lexer, struct ink_token *token);

#ifdef __cplusplus
}
#endif

#endif
