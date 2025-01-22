#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "object.h"
#include "opcode.h"
#include "story.h"

static void test_ink_story(void **state)
{
    struct ink_story story;
    struct ink_object *obj;

    ink_story_create(&story);

    obj = ink_number_new(&story, 1);
    ink_object_vec_push(&story.constants, obj);

    obj = ink_number_new(&story, 2);
    ink_object_vec_push(&story.constants, obj);
    ink_bytecode_vec_push(&story.code, INK_OP_LOAD_CONST);
    ink_bytecode_vec_push(&story.code, 0);
    ink_bytecode_vec_push(&story.code, INK_OP_LOAD_CONST);
    ink_bytecode_vec_push(&story.code, 1);
    ink_bytecode_vec_push(&story.code, INK_OP_ADD);
    ink_bytecode_vec_push(&story.code, INK_OP_RET);

    ink_story_execute(&story);
    ink_story_destroy(&story);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ink_story),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
