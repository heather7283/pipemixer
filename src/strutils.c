#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#include "strutils.h"
#include "xmalloc.h"

void string_from_pchar(struct string *str, const char *src) {
    size_t len = strlen(src);

    if (str->cap < len + 1) {
        str->data = xrealloc(str->data, len + 1);
        str->cap = len + 1;
    }

    memcpy(str->data, src, len);
    str->data[len] = '\0';
    str->len = len;
}

int string_printf(struct string *str, const char *format, ...) {
    va_list args, args_copy;
    va_start(args, format);

    va_copy(args_copy, args);
    unsigned int len = vsnprintf(NULL, 0, format, args_copy);
    if (str->cap < len + 1) {
        str->data = xrealloc(str->data, len + 1);
        str->cap = len + 1;
    }
    va_end(args_copy);

    len = vsnprintf(str->data, str->cap, format, args);
    str->len = len;

    va_end(args);

    return len;
}

void string_free(struct string *str) {
    free(str->data);
    str->data = NULL;
    str->len = 0;
    str->cap = 0;
}

void wstring_from_pchar(struct wstring *str, const char *src) {
    /* dumb utf-8 to wchar conversion */
    str->len = 0;

    const char *p = src;
    while (*p != '\0') {
        unsigned char c = *(p++);
        uint8_t seq_len = 0;
        uint32_t codepoint = 0;

        #define CHECK_BITS(val, mask, expected) (((val) & (mask)) == (expected))

        if (CHECK_BITS(c, 0b10000000, 0b00000000)) {
            seq_len = 1;
            codepoint = c & 0b01111111;
        } else if (CHECK_BITS(c, 0b11100000, 0b11000000)) {
            seq_len = 2;
            codepoint = c & 0b00011111;
        } else if (CHECK_BITS(c, 0b11110000, 0b11100000)) {
            seq_len = 3;
            codepoint = c & 0b00001111;
        } else if (CHECK_BITS(c, 0b11111000, 0b11110000)) {
            seq_len = 4;
            codepoint = c & 0b00000111;
        } else {
            goto again;
        }

        for (uint8_t i = 1; i < seq_len; i++) {
            unsigned char c = *(p++);
            if (c == '\0') {
                goto end;
            } else if (!CHECK_BITS(c, 0b11000000, 0b10000000)) {
                goto again;
            }

            codepoint = (codepoint << 6) | (c & 0b00111111);
        }

        #undef CHECK_BITS

        if (str->len + 1 > str->cap) {
            str->cap = (str->cap == 0) ? 128 : (str->cap * 2);
            str->data = xrealloc(str->data, str->cap * sizeof(*str->data));
        }
        str->data[str->len++] = codepoint;

    again:;
    }

end:
    str->data[str->len] = L'\0';
}

void wstring_free(struct wstring *str) {
    free(str->data);
    str->data = NULL;
    str->len = 0;
    str->cap = 0;
}

