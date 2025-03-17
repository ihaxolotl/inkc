#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "story.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint8_t *b = NULL;
    struct ink_story *const s = ink_open();

    if (!s) {
        return -1;
    }

    b = malloc(size + 1);
    if (!b) {
        ink_close(s);
        return -1;
    }

    memcpy(b, data, size);
    b[size] = '\0';

    const struct ink_load_opts opts = {
        .filename = (uint8_t *)"<STDIN>",
        .source_bytes = b,
        .source_length = size,
        .flags = 0,
    };

    ink_story_load_opts(s, &opts);
    ink_close(s);
    free(b);
    return 0;
}
