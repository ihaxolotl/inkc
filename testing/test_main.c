#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "story.h"

#define PATH_MAX 1024

struct test_stream {
    uint8_t *bytes;
    size_t length;
    size_t read_position;
};

static const char *TEST_ROOT = "testing";

static const char *TEST_FILES[] = {
    "exec/content/hello-world",
    "exec/choices/monsieur-fogg",
    "exec/gathers/monsieur-fogg",
};

static const size_t TEST_FILES_COUNT =
    sizeof(TEST_FILES) / sizeof(TEST_FILES[0]);

static int parse_int(const char *chars, size_t length)
{
    int res = 0;
    int sign = 1;
    size_t i = 0;

    if (!chars || length == 0) {
        return 0;
    }
    while (i < length && isspace((uint8_t)chars[i])) {
        i++;
    }
    if (i < length && (chars[i] == '-' || chars[i] == '+')) {
        sign = (chars[i] == '-') ? -1 : 1;
        i++;
    }
    while (i < length && isdigit((uint8_t)chars[i])) {
        res = res * 10 + (chars[i] - '0');
        i++;
    }
    return sign * res;
}

static int read_file_to_stream(struct test_stream *s, const char *filename)
{
    size_t sz = 0, nr = 0;
    FILE *const fp = fopen(filename, "rb");

    if (!fp) {
        fprintf(stderr, "Could not read file '%s'.\n", filename);
        return -1;
    }

    assert(s->bytes == NULL);
    assert(s->length == 0);

    fseek(fp, 0u, SEEK_END);
    sz = (size_t)ftell(fp);
    fseek(fp, 0u, SEEK_SET);

    s->length = sz;
    s->bytes = malloc(sz + 1);
    if (!s->bytes) {
        fclose(fp);
        return -1;
    }

    nr = fread(s->bytes, 1u, sz, fp);
    if (nr < s->length) {
        fprintf(stderr, "Could not read file '%s'.\n", filename);
        return -1;
    }

    s->bytes[nr] = '\0';
    fclose(fp);
    return 0;
}

static int read_from_stream(struct test_stream *s, uint8_t **data,
                            size_t *length)
{
    uint8_t *p_start = s->bytes + s->read_position;
    uint8_t *p_end = p_start;

    while (*p_end != '\0' && *p_end != '\n') {
        p_end++;
    }

    *data = p_start;
    *length = (size_t)(p_end - p_start);
    s->read_position += *length;
    return 0;
}

static int write_to_stream(struct test_stream *s, const char *fmt, ...)
{
    int n = 0;
    size_t bsz = 0;
    va_list ap;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n < 0) {
        return -1;
    }

    bsz = s->length + (size_t)n + 1;
    s->bytes = realloc(s->bytes, bsz);
    if (!s->bytes) {
        return -1;
    }

    va_start(ap, fmt);
    n = vsnprintf((char *)s->bytes + s->length, bsz - s->length, fmt, ap);
    va_end(ap);

    if (n < 0) {
        return -1;
    }

    s->length = bsz - 1;
    return 0;
}

static bool cmp_stream(struct test_stream *a, struct test_stream *b)
{
    return a->length == b->length && memcmp(a->bytes, b->bytes, a->length) == 0;
}

static void init_stream(struct test_stream *s)
{
    s->bytes = NULL;
    s->length = 0;
    s->read_position = 0;
}

static void free_stream(struct test_stream *s)
{
    free(s->bytes);
    s->bytes = NULL;
    s->length = 0;
    s->read_position = 0;
}

static int process_story(struct ink_story *story, struct test_stream *input,
                         struct test_stream *output)
{
    int rc = -1;
    int ch_index = 0;
    size_t line_len = 0;
    uint8_t *line = NULL;
    struct ink_string *content = NULL;
    struct ink_choice *choice = NULL;
    struct ink_choice_vec choices;

    ink_choice_vec_init(&choices);

    while (ink_story_can_continue(story)) {
        rc = ink_story_continue(story, &content);
        assert(!rc);

        if (content) {
            rc = write_to_stream(output, "%s\n", content->bytes);
            assert(!rc);
        }

        ink_story_get_choices(story, &choices);

        if (choices.count > 0) {
            ch_index = 0;

            for (size_t i = 0; i < choices.count; i++) {
                choice = &choices.entries[i];
                rc = write_to_stream(output, "%zu: %s\n", i + 1,
                                     choice->text->bytes);
                assert(!rc);
            }

            rc = read_from_stream(input, &line, &line_len);
            assert(!rc);

            ch_index = parse_int((char *)line, line_len);
            assert(ch_index >= 0);

            rc = ink_story_choose(story, (size_t)ch_index);
            assert(!rc);

            rc = write_to_stream(output, "?> ");
            assert(!rc);
        }
    }

    ink_choice_vec_deinit(&choices);
    return rc;
}

int main(void)
{
    int rc = -1;
    int story_flags = INK_F_DUMP_AST | INK_F_DUMP_CODE | INK_F_GC_ENABLE |
                      INK_F_GC_STRESS | INK_F_GC_TRACING | INK_F_VM_TRACING;
    char path[PATH_MAX];
    struct ink_story *story = NULL;
    struct test_stream input, output, expected;

    for (size_t i = 0; i < TEST_FILES_COUNT; i++) {
        init_stream(&input);
        init_stream(&expected);
        init_stream(&output);

        snprintf(path, PATH_MAX, "%s/%s/transcript.txt", TEST_ROOT,
                 TEST_FILES[i]);
        rc = read_file_to_stream(&expected, path);
        assert(!rc);

        snprintf(path, PATH_MAX, "%s/%s/input.txt", TEST_ROOT, TEST_FILES[i]);
        rc = read_file_to_stream(&input, path);
        assert(!rc);

        snprintf(path, PATH_MAX, "%s/%s/story.ink", TEST_ROOT, TEST_FILES[i]);
        story = ink_open();
        assert(story);

        rc = ink_story_load_file(story, path, story_flags);
        assert(!rc);

        process_story(story, &input, &output);
        printf("'%s', result=%d\n", path, cmp_stream(&expected, &output));
        ink_close(story);

        free_stream(&input);
        free_stream(&output);
        free_stream(&expected);
    }
}
