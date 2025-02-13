#ifndef __INK_IR_H__
#define __INK_IR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common.h"
#include "vec.h"

#define INK_IR_OP(T)                                                           \
    T(ALLOC, "alloc")                                                          \
    T(LOAD, "load")                                                            \
    T(STORE, "store")                                                          \
    T(NUMBER, "num")                                                           \
    T(STRING, "str")                                                           \
    T(TRUE, "true")                                                            \
    T(FALSE, "false")                                                          \
    T(ADD, "add")                                                              \
    T(SUB, "sub")                                                              \
    T(MUL, "mul")                                                              \
    T(DIV, "div")                                                              \
    T(MOD, "mod")                                                              \
    T(NEG, "neg")                                                              \
    T(CMP_EQ, "cmp_eq")                                                        \
    T(CMP_NEQ, "cmp_neq")                                                      \
    T(CMP_LT, "cmp_lt")                                                        \
    T(CMP_LTE, "cmp_lte")                                                      \
    T(CMP_GT, "cmp_gt")                                                        \
    T(CMP_GTE, "cmp_gte")                                                      \
    T(BOOL_NOT, "bool_not")                                                    \
    T(BLOCK, "block")                                                          \
    T(CONDBR, "cond_br")                                                       \
    T(BR, "br")                                                                \
    T(SWITCH_BR, "switch_br")                                                  \
    T(SWITCH_CASE, "switch_case")                                              \
    T(CONTENT_PUSH, "content_push")                                            \
    T(DIVERT, "divert")                                                        \
    T(CHECK_RESULT, "check_result")                                            \
    T(DONE, "done")                                                            \
    T(END, "end")                                                              \
    T(DECL_KNOT, "decl_knot")                                                  \
    T(DECL_VAR, "decl_var")                                                    \
    T(RET_IMPLICIT, "ret_implicit")                                            \
    T(RET, "ret")

#define T(name, description) INK_IR_INST_##name,
enum ink_ir_inst_op {
    INK_IR_OP(T)
};
#undef T

struct ink_ir_inst_seq {
    struct ink_ir_inst_seq *next;
    size_t count;
    size_t entries[1];
};

struct ink_ir_inst {
    enum ink_ir_inst_op op;
    union {
        bool boolean;
        double number;
        size_t string;

        struct {
            size_t lhs;
        } unary;

        struct {
            size_t lhs;
            size_t rhs;
        } binary;

        struct {
            struct ink_ir_inst_seq *seq;
        } block;

        struct {
            size_t payload_index;
            struct ink_ir_inst_seq *then_;
            struct ink_ir_inst_seq *else_;
        } cond_br;

        struct {
            bool has_else;
            size_t payload_index;
            struct ink_ir_inst_seq *cases;
        } switch_block;

        struct {
            size_t payload_index;
            struct ink_ir_inst_seq *body;
        } switch_case;

        struct {
            size_t callee_index;
            struct ink_ir_inst_seq *args;
        } activation;

        struct {
            bool is_const;
            size_t name_offset;
        } var_decl;

        struct {
            size_t name_offset;
            struct ink_ir_inst_seq *body;
        } knot_decl;
    } as;
};

INK_VEC_T(ink_ir_byte_vec, uint8_t)
INK_VEC_T(ink_ir_inst_vec, struct ink_ir_inst)

struct ink_ir {
    struct ink_ir_byte_vec string_bytes;
    struct ink_ir_inst_vec instructions;
    struct ink_ir_inst_seq *sequence_list_head;
    struct ink_ir_inst_seq *sequence_list_tail;
};

extern void ink_ir_init(struct ink_ir *ir);
extern void ink_ir_deinit(struct ink_ir *ir);
extern void ink_ir_dump(const struct ink_ir *ir);
extern const char *ink_ir_inst_op_strz(enum ink_ir_inst_op op);

#ifdef __cplusplus
}
#endif

#endif
