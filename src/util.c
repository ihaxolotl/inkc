#include <stdbool.h>

#include "util.h"

bool ink_is_alpha(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool ink_is_digit(unsigned char c)
{
    return (c >= '0' && c <= '9');
}
