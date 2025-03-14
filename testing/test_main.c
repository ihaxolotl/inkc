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

static bool __test_hashmap_1_cmp(const void *lhs, const void *rhs)
{
    const int key_lhs = *((int *)lhs);
    const int key_rhs = *((int *)rhs);

    return key_lhs == key_rhs;
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

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ink_hashmap),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
