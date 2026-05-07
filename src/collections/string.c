#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>

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

bool string_appendc(struct string *s, char c) {
    make_space(s, s->len + 1);
    s->data[s->len++] = c;
    s->data[s->len] = '\0';

    return true;
}

bool string_appendwc(struct string *s, wchar_t c) {
    const size_t len = wcrtomb(NULL, c, &(mbstate_t){0});
    if (len == (size_t)-1) {
        return false;
    }

    make_space(s, s->len + len);
    wcrtomb(&s->data[s->len], c, &(mbstate_t){0});
    s->len += len;
    s->data[s->len] = '\0';

    return true;
}

bool string_appendsn(struct string *s, const char *suff, size_t suff_len) {
    make_space(s, s->len + suff_len);
    memcpy(&s->data[s->len], suff, suff_len);
    s->len += suff_len;
    s->data[s->len] = '\0';

    return true;
}

bool string_appendsz(struct string *s, const char *suff) {
    return string_appendsn(s, suff, strlen(suff));
}

bool string_appendwsn(struct string *s, const wchar_t *suff, size_t suff_len) {
    const size_t suff_bytes = wcsnrtombs(NULL, &(const wchar_t *){suff},
                                         suff_len, 0, &(mbstate_t){0});
    if (suff_bytes == (size_t)-1) {
        return false;
    }

    make_space(s, s->len + suff_bytes);
    wcsnrtombs(&s->data[s->len], &suff, suff_len, suff_bytes, &(mbstate_t){0});
    s->len += suff_bytes;
    s->data[s->len] = '\0';

    return true;
}

bool string_appendwsz(struct string *s, const wchar_t *suff) {
    return string_appendwsn(s, suff, wcslen(suff));
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

