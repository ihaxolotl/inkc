#ifndef __INK_OPCODE_H__
#define __INK_OPCODE_H__

#ifdef __cplusplus
extern "C" {
#endif

#define INK_MAKE_OPCODE_LIST(T)                                                \
    T(OP_RET, "RET")                                                           \
    T(OP_LOAD_CONST, "LOAD_CONST")                                             \
    T(OP_POP, "POP")                                                           \
    T(OP_ADD, "ADD")                                                           \
    T(OP_SUB, "SUB")                                                           \
    T(OP_MUL, "MUL")                                                           \
    T(OP_DIV, "DIV")                                                           \
    T(OP_MOD, "MOD")                                                           \
    T(OP_NEG, "NEG")

#define T(name, description) INK_##name,
enum ink_vm_opcode {
    INK_MAKE_OPCODE_LIST(T)
};
#undef T

#ifdef __cplusplus
}
#endif

#endif
