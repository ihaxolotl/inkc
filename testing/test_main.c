#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmocka.h>

#include "hashmap.h"
#include "memory.h"
#include "object.h"
#include "story.h"
#include "vec.h"

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

static void *nullgpa_alloc(struct ink_allocator *self, size_t size)
{
    (void)self;
    (void)size;
    return NULL;
}

static void *nullgpa_realloc(struct ink_allocator *self, void *ptr, size_t size)
{
    (void)self;
    (void)ptr;
    (void)size;
    return NULL;
}

static void nullgpa_dealloc(struct ink_allocator *self, void *ptr)
{
    (void)self;
    (void)ptr;
}

static struct ink_allocator NULL_GPA = {
    .allocate = nullgpa_alloc,
    .resize = nullgpa_realloc,
    .free = nullgpa_dealloc,
};

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

static void test_exec(void)
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

struct test_state {
    struct ink_allocator *gpa;
};

static int t_group_setup(void **state)
{
    struct test_state *t = calloc(1, sizeof(*t));

    assert(t);

    ink_get_global_allocator(&t->gpa);
    *state = t;
    return 0;
}

static int t_group_teardown(void **state)
{
    struct test_state *t = *state;

    free(t);
    return 0;
}

static int t_setup(void **state)
{
    struct test_state *t = *state;
    ink_set_global_allocator(t->gpa);
    return 0;
}

static int t_teardown(void **state)
{
    struct test_state *t = *state;
    ink_set_global_allocator(t->gpa);
    return 0;
}

INK_VEC_T(tvec, int)
INK_HASHMAP_T(tht, int, int)

static void test_vec_oom(void **state)
{
    struct tvec v;

    tvec_init(&v);
    ink_set_global_allocator(&NULL_GPA);
    assert_int_equal(tvec_reserve(&v, 100), -INK_E_OOM);
    assert_int_equal(tvec_push(&v, 100), -INK_E_OOM);
    tvec_deinit(&v);
}

static void test_vec_push(void **state)
{
    struct tvec v;

    tvec_init(&v);

    for (int i = 0; i < 100000; i++) {
        assert_int_equal(tvec_push(&v, i), INK_E_OK);
    }

    assert_int_equal(v.entries[123], 123);
    assert_int_equal(v.entries[456], 456);
    assert_int_equal(v.entries[789], 789);
    tvec_deinit(&v);
}

static void test_vec_pop(void **state)
{
    struct tvec v;

    tvec_init(&v);
    assert_int_equal(tvec_push(&v, 100), INK_E_OK);
    assert_int_equal(tvec_pop(&v), 100);
    assert_int_equal(v.count, 0);
    tvec_deinit(&v);
}

static void test_vec_reserve(void **state)
{
    struct tvec v;

    tvec_init(&v);
    tvec_reserve(&v, 10000);
    assert_int_equal(v.capacity, 10000);
    assert_int_equal(v.count, 0);
    assert_non_null(v.entries);
    tvec_deinit(&v);
}

static uint32_t tht_hash(const void *key, size_t length)
{
    return ink_fnv32a((uint8_t *)key, length);
}

static bool tht_cmp(const void *lhs, const void *rhs)
{
    const int key_lhs = *(int *)lhs;
    const int key_rhs = *(int *)rhs;

    return (key_lhs == key_rhs);
}

static void test_hashmap_oom(void **state)
{
    struct tht ht;

    tht_init(&ht, 80u, tht_hash, tht_cmp);
    ink_set_global_allocator(&NULL_GPA);
    assert_int_equal(tht_insert(&ht, 100, 100), -INK_E_OOM);
    tht_deinit(&ht);
}

static void test_hashmap_insert(void **state)
{
    struct tht ht;
    int entry = 0;

    tht_init(&ht, 80u, tht_hash, tht_cmp);

    for (int i = 0; i < 0x1000; i++) {
        assert_int_equal(tht_insert(&ht, i, i), INK_E_OK);
    }
    assert_int_equal(ht.count, 0x1000);
    assert_int_equal(tht_lookup(&ht, 123, &entry), INK_E_OK);
    assert_int_equal(entry, 123);
    tht_deinit(&ht);
}

static void test_hashmap_remove(void **state)
{
    struct tht ht;

    tht_init(&ht, 80u, tht_hash, tht_cmp);

    for (int i = 0; i < 100000; i++) {
        assert_int_equal(tht_insert(&ht, 100 + i, i + 1), INK_E_OK);
    }

    assert_int_equal(tht_remove(&ht, 150), INK_E_OK);
    assert_int_equal(tht_remove(&ht, 199), INK_E_OK);
    assert_int_equal(tht_remove(&ht, 101), INK_E_OK);

    tht_deinit(&ht);
}

int main(void)
{
    test_exec();

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_vec_oom, t_setup, t_teardown),
        cmocka_unit_test_setup_teardown(test_vec_push, t_setup, t_teardown),
        cmocka_unit_test_setup_teardown(test_vec_pop, t_setup, t_teardown),
        cmocka_unit_test_setup_teardown(test_vec_reserve, t_setup, t_teardown),
        cmocka_unit_test_setup_teardown(test_hashmap_oom, t_setup, t_teardown),
        cmocka_unit_test_setup_teardown(test_hashmap_insert, t_setup,
                                        t_teardown),
        cmocka_unit_test_setup_teardown(test_hashmap_remove, t_setup,
                                        t_teardown),
    };

    return cmocka_run_group_tests(tests, t_group_setup, t_group_teardown);
}
