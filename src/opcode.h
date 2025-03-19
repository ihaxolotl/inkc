#ifndef __INK_OPCODE_H__
#define __INK_OPCODE_H__

#ifdef __cplusplus
extern "C" {
#endif

#define INK_MAKE_OPCODE_LIST(T)                                                \
    T(OP_EXIT, "exit")                                                         \
    T(OP_RET, "ret")                                                           \
    T(OP_POP, "pop")                                                           \
    T(OP_TRUE, "true")                                                         \
    T(OP_FALSE, "false")                                                       \
    T(OP_CONST, "const")                                                       \
    T(OP_ADD, "add")                                                           \
    T(OP_SUB, "sub")                                                           \
    T(OP_MUL, "mul")                                                           \
    T(OP_DIV, "div")                                                           \
    T(OP_MOD, "mod")                                                           \
    T(OP_NEG, "neg")                                                           \
    T(OP_NOT, "not")                                                           \
    T(OP_CMP_EQ, "cmp_eq")                                                     \
    T(OP_CMP_LT, "cmp_lt")                                                     \
    T(OP_CMP_GT, "cmp_gt")                                                     \
    T(OP_CMP_LTE, "cmp_lte")                                                   \
    T(OP_CMP_GTE, "cmp_gte")                                                   \
    T(OP_JMP, "jmp")                                                           \
    T(OP_JMP_T, "jmp_t")                                                       \
    T(OP_JMP_F, "jmp_f")                                                       \
    T(OP_CALL, "call")                                                         \
    T(OP_DIVERT, "divert")                                                     \
    T(OP_LOAD, "load")                                                         \
    T(OP_STORE, "store")                                                       \
    T(OP_LOAD_GLOBAL, "load_global")                                           \
    T(OP_STORE_GLOBAL, "store_global")                                         \
    T(OP_LOAD_CHOICE_ID, "load_choice_id")                                     \
    T(OP_CONTENT, "content")                                                   \
    T(OP_LINE, "line")                                                         \
    T(OP_GLUE, "glue")                                                         \
    T(OP_CHOICE, "choice")                                                     \
    T(OP_FLUSH, "flush")

#define T(name, description) INK_##name,
enum ink_vm_opcode {
    INK_MAKE_OPCODE_LIST(T)
};
#undef T

#ifdef __cplusplus
}
#endif

#endif
