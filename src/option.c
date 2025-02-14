#include <string.h>

#include "option.h"

static const struct option *_g_option_opts;
static char *_g_arg_ptr;
static char **_g_option_argv;

char *option_operand;
char *option_unknown_opt;

/**
 * Set internal globals to point to opts and argv.
 */
void option_setopts(const struct option *opts, char **argv)
{
    _g_option_argv = argv;
    _g_option_opts = opts;
}

/**
 * Get the .id field from the next option in the opts array
 */
int option_nextopt(void)
{
    if (*_g_option_argv != NULL) {
        ++_g_option_argv;
    } else {
        return 0;
    }

    char **optstr_p = _g_option_argv;
    if (*optstr_p == NULL) {
        return 0;
    }

    const struct option *p = _g_option_opts;
    while (p->id != 0) {
        if (!strcmp(*optstr_p, p->name)) {
            if (p->has_args) {
                _g_arg_ptr = optstr_p[1];
                ++_g_option_argv;
            }

            return p->id;
        } else if (**optstr_p != '-') {
            option_operand = *optstr_p;
            return OPTION_OPERAND;
        }

        ++p;
    }

    option_unknown_opt = *optstr_p;
    return OPTION_UNKNOWN;
}

/**
 * Get the next argument from the last option with arguments.
 */
char *option_nextarg(void)
{
    char *arg = _g_arg_ptr;
    char *arg_p = arg;

    while (*arg_p != '\0' && *arg_p != ',') {
        ++arg_p;
    }
    if (*arg_p == '\0') {
        _g_arg_ptr = arg_p;
        return arg;
    }

    _g_arg_ptr = arg_p + 1;
    *arg_p = '\0';
    return arg;
}
