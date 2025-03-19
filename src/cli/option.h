#ifndef INKC_OPTION_H
#define INKC_OPTION_H

#include <stdbool.h>

enum {
    OPTION_UNKNOWN = -2,
    OPTION_OPERAND = -3
};

struct option {
    char *name;
    int id;
    bool has_args;
};

extern char *option_operand;
extern char *option_unknown_opt;

extern void option_setopts(const struct option *opts, char **argv);
extern int option_nextopt(void);
extern char *option_nextarg(void);

#endif
