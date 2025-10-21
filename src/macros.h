#pragma once

#define STREQ(a, b) (strcmp((a), (b)) == 0)
#define STRCASEEQ(a, b) (strcasecmp((a), (b)) == 0)
#define STRSTARTSWITH(a, b) (strncmp((a), (b), strlen(b)) == 0)

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

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define TYPEOF(x) __typeof__(x)

#define CONTAINER_OF(member_ptr, container_ptr, member_name) \
    ((TYPEOF(container_ptr))((char *)(member_ptr) - offsetof(TYPEOF(*container_ptr), member_name)))

#define SIZEOF_ARRAY(arr) (sizeof(arr) / sizeof((arr)[0]))

#define SWAP(a, b) \
    do { \
        TYPEOF(a) tmp = (a); \
        (a) = (b); \
        (b) = tmp; \
    } while (0)

#define _3(a, b) a##b
#define _2(a, b) _3(a, b)
#define _ _2(_dummy_param_, __COUNTER__)

