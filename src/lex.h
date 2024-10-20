#ifndef __INK_LEX_H__
#define __INK_LEX_H__

#ifdef __cplusplus
extern "C" {
#endif

struct ink_source;

#define INK_TT(T)                                                              \
    T(TT_EOF, "TT_EOF")                                                        \
    T(TT_NL, "TT_NL")                                                          \
    T(TT_LBRACE, "TT_LBRACE")                                                  \
    T(TT_RBRACE, "TT_RBRACE")                                                  \
    T(TT_PIPE, "TT_PIPE")                                                      \
    T(TT_DQUOTE, "TT_DQUOTE")                                                  \
    T(TT_STRING, "TT_STRING")                                                  \
    T(TT_ERROR, "TT_ERROR")

#define T(name, description) INK_##name,
enum ink_token_type {
    INK_TT(T)
};
#undef T

struct ink_token {
    enum ink_token_type type;
    unsigned int start_offset;
    unsigned int end_offset;
};

struct ink_lexer {
    struct ink_source *source;
    unsigned int cursor_offset;
    unsigned int start_offset;
};

extern const char *ink_token_type_strz(enum ink_token_type type);
extern void ink_token_next(struct ink_lexer *lexer, struct ink_token *token);
void ink_token_print(struct ink_source *source, const struct ink_token *token);

#ifdef __cplusplus
}
#endif

#endif
