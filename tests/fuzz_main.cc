#include <stddef.h>
#include <stdint.h>

#include "story.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    struct ink_story story;
    const struct ink_load_opts opts = {
        .filename = (uint8_t *)"<STDIN>",
        .source_text = data,
        .flags = 0,
    };

    int rc = ink_story_load_opts(&story, &opts);
    if (rc < 0) {
        return -1;
    }
    return 0;
}
