#ifndef SRC_COLLECTIONS_STRING_H
#define SRC_COLLECTIONS_STRING_H

#include <stdlib.h>
#include <stdbool.h>
#include <wchar.h>

struct string {
    char *data; /* null-terminated */
    size_t len; /* WITHOUT NULL TERMINATOR */
    size_t cap; /* WITH NULL TERMINATOR */
};

struct wstring {
    wchar_t *data; /* null-terminated */
    size_t len; /* WITHOUT NULL TERMINATOR */
    size_t cap; /* WITH NULL TERMINATOR */
};

void string_from_pchar(struct string *str, const char *src);
int string_printf(struct string *str, const char *format, ...);
static inline bool string_is_empty(const struct string *str) { return str->len == 0; };
void string_free(struct string *str);

void wstring_from_pchar(struct wstring *str, const char *src);
static inline bool wstring_is_empty(const struct wstring *str) { return str->len == 0; };
void wstring_free(struct wstring *str);

#endif /* #ifndef SRC_COLLECTIONS_STRING_H */

