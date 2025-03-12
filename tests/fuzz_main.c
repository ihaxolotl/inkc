#include <stddef.h>
#include <stdint.h>

#include "story.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    struct ink_story *const story = ink_open();

    if (!story) {
        return -1;
    }

    const struct ink_load_opts opts = {
        .filename = (uint8_t *)"<STDIN>",
        .source_bytes = data,
        .source_length = size,
        .flags = 0,
    };

    const int rc = ink_story_load_opts(story, &opts);
    if (rc < 0) {
        ink_close(story);
        return -1;
    }

    ink_close(story);
    return 0;
}
