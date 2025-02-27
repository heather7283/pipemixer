#ifndef MACROS_H
#define MACROS_H

#include <stdlib.h> /* abort */
#include <stdio.h>  /* fprintf */

#define ANSI_DIM    "\033[2m"
#define ANSI_RED    "\033[031m"
#define ANSI_YELLOW "\033[033m"
#define ANSI_RESET  "\033[0m"

#define streq(a, b) (strcmp(a, b) == 0)

#define min(a, b) a < b ? a : b
#define max(a, b) a > b ? a : b

#define UNUSED(var) ((void)(var)) /* https://stackoverflow.com/a/3599170 */

#define BYTE_BINARY_FORMAT "0b%c%c%c%c%c%c%c%c"
#define BYTE_BINARY_ARGS(byte) \
    ((byte) & (1 << 7) ? '1' : '0'), \
    ((byte) & (1 << 6) ? '1' : '0'), \
    ((byte) & (1 << 5) ? '1' : '0'), \
    ((byte) & (1 << 4) ? '1' : '0'), \
    ((byte) & (1 << 3) ? '1' : '0'), \
    ((byte) & (1 << 2) ? '1' : '0'), \
    ((byte) & (1 << 1) ? '1' : '0'), \
    ((byte) & (1 << 0) ? '1' : '0')

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#endif /* #ifndef MACROS_H */

