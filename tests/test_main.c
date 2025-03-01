#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <cmocka.h>

#include "hashmap.h"
#include "object.h"
#include "opcode.h"
#include "story.h"

INK_HASHMAP_T(test_hashmap_1, int, int)

static uint32_t __test_hashmap_1_hash(const void *data, size_t length)
{
    return ink_fnv32a((uint8_t *)data, length);
}

static bool __test_hashmap_1_cmp(const void *a, size_t a_length, const void *b,
                                 size_t b_length)
{
    const int key_a = *((int *)a);
    const int key_b = *((int *)b);

    return key_a == key_b;
}

static void test_ink_hashmap(void **state)
{
    int value;
    struct test_hashmap_1 ht;

    test_hashmap_1_init(&ht, 80, __test_hashmap_1_hash, __test_hashmap_1_cmp);
    test_hashmap_1_insert(&ht, 1, 123);
    test_hashmap_1_insert(&ht, 2, 456);
    test_hashmap_1_insert(&ht, 3, 789);

    assert_false(test_hashmap_1_lookup(&ht, 1, &value) < 0);
    assert_true(value == 123);
    assert_false(test_hashmap_1_lookup(&ht, 2, &value) < 0);
    assert_true(value == 456);
    assert_false(test_hashmap_1_lookup(&ht, 3, &value) < 0);
    assert_true(value == 789);

    test_hashmap_1_deinit(&ht);
}

static void test_bytecode(void **state)
{
    struct ink_story story;
    struct ink_object *obj;

    ink_story_init(&story, 0);

    obj = ink_number_new(&story, 1);
    ink_object_vec_push(&story.constants, obj);

    obj = ink_number_new(&story, 2);
    ink_object_vec_push(&story.constants, obj);
    ink_byte_vec_push(&story.code, INK_OP_LOAD_CONST);
    ink_byte_vec_push(&story.code, 0);
    ink_byte_vec_push(&story.code, INK_OP_LOAD_CONST);
    ink_byte_vec_push(&story.code, 1);
    ink_byte_vec_push(&story.code, INK_OP_ADD);
    ink_byte_vec_push(&story.code, 0);
    ink_byte_vec_push(&story.code, INK_OP_RET);
    ink_byte_vec_push(&story.code, 0);

    ink_story_execute(&story);
    ink_story_free(&story);
}

static void test_story_content(void **state)
{
    static const char *text = "Hello, world!";
    char *str;
    struct ink_story story;

    ink_story_load(&story, text, INK_F_DUMP_AST | INK_F_TRACING);

    str = ink_story_continue(&story);
    assert(str != NULL);
    assert_string_equal(str, "Hello, world!");
    ink_free(str);
    ink_story_free(&story);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ink_hashmap),
        cmocka_unit_test(test_bytecode),
        cmocka_unit_test(test_story_content),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
