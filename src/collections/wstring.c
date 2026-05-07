#include <stdarg.h>

#include "collections/wstring.h"
#include "xmalloc.h"
#include "macros.h"

/* make space for len wide characters and a null terminator */
static void make_space(struct wstring *ws, size_t _len) {
    const size_t len = _len + 1;
    if (ws->cap < len) {
        ws->cap = ws->cap * 2 < len ? len : ws->cap * 2;
        ws->data = xreallocarray(ws->data, ws->cap, sizeof(ws->data[0]));
    }
}

void wstring_append(struct wstring *ws, wchar_t c) {
    make_space(ws, ws->len + 1);
    ws->data[ws->len++] = c;
    ws->data[ws->len] = L'\0';
}

int wstring_printf(struct wstring *ws, const wchar_t *fmt, ...) {
    /* fuck this shitty libc API. Surely this is enough :clueless: */
    static wchar_t buf[1024];

    va_list args;
    va_start(args, fmt);
    const int len = vswprintf(buf, SIZEOF_ARRAY(buf), fmt, args);
    va_end(args);

    if (len < 0) {
        return -1;
    }

    make_space(ws, len);
    wmemcpy(&ws->data[ws->len], buf, len);
    ws->len += len;
    ws->data[ws->len] = L'\0';

    return len;
}

void wstring_init(struct wstring *ws) {
    *ws = (struct wstring){0};
}

void wstring_clear(struct wstring *ws) {
    ws->len = 0;
    if (ws->data) {
        ws->data[0] = L'\0';
    }
}

void wstring_free(struct wstring *ws) {
    free(ws->data);
    wstring_init(ws);
}

