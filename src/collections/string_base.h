#include <stddef.h>

#if !defined(STRING_STRUCT_TYPE) || !defined(STRING_ELEMENT_TYPE)
#error "define STRING_STRUCT_TYPE and STRING_ELEMENT_TYPE"
#endif

struct STRING_STRUCT_TYPE {
    STRING_ELEMENT_TYPE *data; /* always null terminated */
    size_t len; /* without null terminator */
    size_t cap; /* with null terminator */
};

/* I love the C preprocessor */
#define __STRING_FUNCTION(ret, type, name, ...) \
    ret type##_##name(struct type *s, ##__VA_ARGS__)
#define _STRING_FUNCTION(ret, type, name, ...) \
    __STRING_FUNCTION(ret, type, name, ##__VA_ARGS__)
#define STRING_FUNCTION(ret, name, ...) \
    _STRING_FUNCTION(ret, STRING_STRUCT_TYPE, name, ##__VA_ARGS__)

STRING_FUNCTION(void, append, STRING_ELEMENT_TYPE c);
STRING_FUNCTION(int, printf, const STRING_ELEMENT_TYPE *fmt, ...);

STRING_FUNCTION(void, init);
STRING_FUNCTION(void, clear);
STRING_FUNCTION(void, free);

#undef STRING_STRUCT_TYPE
#undef STRING_ELEMENT_TYPE

