#ifndef __INK_PARSER_H__
#define __INK_PARSER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

struct ink_arena;
struct ink_source;

#define INK_NODE(T)                                                            \
    T(NODE_FILE, "NODE_FILE")                                                  \
    T(NODE_CONTENT_STMT, "NODE_CONTENT_STMT")                                  \
    T(NODE_CONTENT_EXPR, "NODE_CONTENT_EXPR")                                  \
    T(NODE_BRACE_EXPR, "NODE_BRACE_EXPR")

#define T(name, description) INK_##name,
enum ink_syntax_node_type {
    INK_NODE(T)
};
#undef T

struct ink_syntax_seq {
    size_t count;
    struct ink_syntax_node *nodes[1];
};

struct ink_syntax_node {
    enum ink_syntax_node_type type;
    struct ink_syntax_node *lhs;
    struct ink_syntax_node *rhs;
    struct ink_syntax_seq *seq;
};

extern void ink_syntax_node_print(const struct ink_syntax_node *node);
extern int ink_parse(struct ink_arena *arena, struct ink_source *source,
                     struct ink_syntax_node **tree);

#ifdef __cplusplus
}
#endif

#endif
