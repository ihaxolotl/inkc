#ifndef INK_SCANNER_H
#define INK_SCANNER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "token.h"

#define INK_SCANNER_DEPTH_MAX 128

enum ink_grammar_type {
    INK_GRAMMAR_CONTENT,
    INK_GRAMMAR_EXPRESSION,
};

struct ink_scanner_mode {
    enum ink_grammar_type type;
    size_t source_offset;
};

struct ink_scanner {
    const uint8_t *source_bytes; /* Source code bytes. NULL-terminated */
    size_t start_offset;         /* Start offset for current lexeme. */
    size_t cursor_offset;        /* Current offset */
    size_t mode_depth;           /* Mode stack depth */
    bool is_line_start;          /* Did we just see a newline? */
    struct ink_scanner_mode mode_stack[INK_SCANNER_DEPTH_MAX];
};

extern bool ink_scanner_try_keyword(struct ink_scanner *scanner,
                                    struct ink_token *token,
                                    enum ink_token_type type);
extern struct ink_scanner_mode *
ink_scanner_current(struct ink_scanner *scanner);
extern int ink_scanner_push(struct ink_scanner *scanner,
                            enum ink_grammar_type type, size_t source_offset);
extern int ink_scanner_pop(struct ink_scanner *scanner);
extern void ink_scanner_rewind(struct ink_scanner *scanner,
                               size_t source_offset);
extern void ink_scanner_next(struct ink_scanner *scanner,
                             struct ink_token *token);

#ifdef __cplusplus
}
#endif

#endif
