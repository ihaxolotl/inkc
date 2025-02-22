#ifndef __INK_TOKEN_H__
#define __INK_TOKEN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define INK_MAKE_TOKEN_LIST(T)                                                 \
    T(TT_EOF, "EndOfFile")                                                     \
    T(TT_NL, "NewLine")                                                        \
    T(TT_AMP, "Ampersand")                                                     \
    T(TT_AMP_AMP, "DoubleAmpersand")                                           \
    T(TT_BANG, "ExclaimationMark")                                             \
    T(TT_BANG_QUESTION, "ExclaimationQuestion")                                \
    T(TT_BANG_EQUAL, "NotEqual")                                               \
    T(TT_CARET, "Caret")                                                       \
    T(TT_COLON, "Colon")                                                       \
    T(TT_COMMA, "Comma")                                                       \
    T(TT_DOT, "Dot")                                                           \
    T(TT_DOUBLE_QUOTE, "DoubleQuote")                                          \
    T(TT_EQUAL, "Equal")                                                       \
    T(TT_EQUAL_EQUAL, "DoubleEqual")                                           \
    T(TT_EQUAL_SEQ, "ManyEqual")                                               \
    T(TT_GLUE, "Glue")                                                         \
    T(TT_GREATER_EQUAL, "GreaterOrEqual")                                      \
    T(TT_GREATER_THAN, "GreaterThan")                                          \
    T(TT_IDENTIFIER, "Name")                                                   \
    T(TT_KEYWORD_AND, "KeywordAnd")                                            \
    T(TT_KEYWORD_CONST, "KeywordConst")                                        \
    T(TT_KEYWORD_ELSE, "KeywordElse")                                          \
    T(TT_KEYWORD_FALSE, "KeywordFalse")                                        \
    T(TT_KEYWORD_FUNCTION, "KeywordFunction")                                  \
    T(TT_KEYWORD_LIST, "KeywordList")                                          \
    T(TT_KEYWORD_MOD, "KeywordMod")                                            \
    T(TT_KEYWORD_NOT, "KeywordNot")                                            \
    T(TT_KEYWORD_OR, "KeywordOr")                                              \
    T(TT_KEYWORD_REF, "KeywordRef")                                            \
    T(TT_KEYWORD_RETURN, "KeywordReturn")                                      \
    T(TT_KEYWORD_TEMP, "KeywordTemp")                                          \
    T(TT_KEYWORD_TRUE, "KeywordTrue")                                          \
    T(TT_KEYWORD_VAR, "KeywordVar")                                            \
    T(TT_LEFT_ARROW, "LeftArrow")                                              \
    T(TT_LEFT_BRACE, "LeftBrace")                                              \
    T(TT_LEFT_BRACKET, "LeftBracket")                                          \
    T(TT_LEFT_PAREN, "LeftParentheses")                                        \
    T(TT_LESS_EQUAL, "LessOrEqual")                                            \
    T(TT_LESS_THAN, "LessThan")                                                \
    T(TT_MINUS, "Minus")                                                       \
    T(TT_MINUS_EQUAL, "MinusEqual")                                            \
    T(TT_MINUS_MINUS, "MinusMinus")                                            \
    T(TT_NUMBER, "Number")                                                     \
    T(TT_PERCENT, "Percentage")                                                \
    T(TT_PIPE, "Pipe")                                                         \
    T(TT_PIPE_PIPE, "PipePipe")                                                \
    T(TT_PLUS, "Plus")                                                         \
    T(TT_PLUS_EQUAL, "PlusEqual")                                              \
    T(TT_PLUS_PLUS, "PlusPlus")                                                \
    T(TT_POUND, "Pound")                                                       \
    T(TT_QUESTION, "QuestionMark")                                             \
    T(TT_RIGHT_ARROW, "RightArrow")                                            \
    T(TT_RIGHT_BRACE, "RightBrace")                                            \
    T(TT_RIGHT_BRACKET, "RightBracket")                                        \
    T(TT_RIGHT_PAREN, "RightParen")                                            \
    T(TT_SLASH, "Slash")                                                       \
    T(TT_STAR, "Star")                                                         \
    T(TT_STRING, "String")                                                     \
    T(TT_TILDE, "Tilde")                                                       \
    T(TT_WHITESPACE, "Whitespace")                                             \
    T(TT_ERROR, "Error")

#define T(name, description) INK_##name,
enum ink_token_type {
    INK_MAKE_TOKEN_LIST(T)
};
#undef T

struct ink_token {
    enum ink_token_type type;
    size_t start_offset;
    size_t end_offset;
};

extern const char *ink_token_type_strz(enum ink_token_type type);
extern void ink_token_print(const uint8_t *source_bytes,
                            const struct ink_token *token);

#ifdef __cplusplus
}
#endif

#endif
