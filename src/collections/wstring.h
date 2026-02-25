#pragma once

#include <wchar.h>
#include <stdio.h>
#include <stdbool.h>

struct wstring {
    wchar_t *data; /* always null terminated */
    size_t len; /* without null terminator */
    FILE *stream;
};

bool wstring_puts(struct wstring *ws, const wchar_t *str);
bool wstring_printf(struct wstring *ws, const wchar_t *fmt, ...);

bool wstring_init(struct wstring *ws);
void wstring_clear(struct wstring *ws);
void wstring_free(struct wstring *ws);

