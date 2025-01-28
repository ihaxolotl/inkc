#include <stdio.h>

#include "token.h"

#define T(name, description) description,
static const char *INK_TT_STR[] = {INK_TT(T)};
#undef T

const char *ink_token_type_strz(enum ink_token_type type)
{
    return INK_TT_STR[type];
}

void ink_token_print(const uint8_t *source_bytes, const struct ink_token *token)
{
    const size_t start = token->start_offset;
    const size_t end = token->end_offset;

    switch (token->type) {
    case INK_TT_EOF:
        printf("[DEBUG] %s(%zu, %zu): `\\0`\n",
               ink_token_type_strz(token->type), start, end);
        break;
    case INK_TT_NL:
        printf("[DEBUG] %s(%zu, %zu): `\\n`\n",
               ink_token_type_strz(token->type), start, end);
        break;
    default:
        printf("[DEBUG] %s(%zu, %zu): `%.*s`\n",
               ink_token_type_strz(token->type), start, end, (int)(end - start),
               source_bytes + start);
        break;
    }
}
