#include <stdarg.h>
#include <stdio.h>

#include "collections/string.h"
#include "xmalloc.h"

/* make space for len wide characters and a null terminator */
static void make_space(struct string *s, size_t _len) {
    const size_t len = _len + 1;
    if (s->cap < len) {
        s->cap = s->cap * 2 < len ? len : s->cap * 2;
        s->data = xreallocarray(s->data, s->cap, sizeof(s->data[0]));
    }
}

void string_append(struct string *s, char c) {
    make_space(s, s->len + 1);
    s->data[s->len++] = c;
    s->data[s->len] = '\0';
}

int string_printf(struct string *s, const char *fmt, ...) {
    va_list args, args_copy;
    va_start(args, fmt);

    va_copy(args_copy, args);
    const int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (len < 0) {
        return len;
    }

    make_space(s, s->len + len);
    vsnprintf(&s->data[s->len], len + 1, fmt, args);

    va_end(args);
    return len;
}

void string_init(struct string *s) {
    *s = (struct string){0};
}

void string_clear(struct string *s) {
    s->len = 0;
    if (s->data) {
        s->data[0] = '\0';
    }
}

void string_free(struct string *s) {
    free(s->data);
    string_init(s);
}

