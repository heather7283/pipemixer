#ifndef MACROS_H
#define MACROS_H

#define STREQ(a, b) (strcmp(a, b) == 0)

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

