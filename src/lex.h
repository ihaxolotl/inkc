#ifndef __INK_LEX_H__
#define __INK_LEX_H__

#ifdef __cplusplus
extern "C" {
#endif

struct ink_source;

enum ink_token_type {
    INK_TT_EOF,
    INK_TT_NL,
    INK_TT_STRING,
    INK_TT_ERR,
};

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

#ifdef __cplusplus
}
#endif

#endif
